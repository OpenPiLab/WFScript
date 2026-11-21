// Offline mock of PanelRuntime's event metadata dispatch, not a substitute for Qt CTests.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.join(__dirname,'..');
function load(stem,api={}){
 const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels',stem+'.wfpanel'),'utf8'));
 const source=fs.readFileSync(path.join(root,'examples/source',stem+'.js'),'utf8');
 assert.equal(panel.script,source);
 const updates=new Map(),manual=[];
 const controls=new Set(panel.pages.flatMap(p=>p.controls.map(c=>c.id)));
 const ui={
   set(id,value){assert(controls.has(id),id);updates.set(id,value);return true;},
   get(id){return updates.get(id);},
   on(id,event,fn){manual.push({id,event});}
 };
 const ctx=vm.createContext({ui,print(){},Date,Math,Number,String,Boolean,JSON,isFinite,...api});
 vm.runInContext(source,ctx,{filename:stem+'.js'});
 const bindings=new Map();
 for(const p of panel.pages)for(const c of p.controls)for(const [ev,b] of Object.entries(c.events||{})){
  const key=c.id+'::'+ev;
  assert.equal(b.enabled,true,key);
  assert.equal(typeof ctx[b.handler],'function',key);
  assert(!bindings.has(key));
  bindings.set(key,ctx[b.handler]);
 }
 function fire(id,event,payload){
   const key=id+'::'+event;
   assert(!manual.some(x=>x.id===id&&x.event===event),'duplicate event '+key);
   assert(bindings.has(key),key);
   return bindings.get(key)(payload);
 }
 return {ctx,panel,ui,updates,bindings,manual,fire};
}
// Introductory demo shows visual binding without any ui.on registrations.
{
 const t=load('GettingStarted');
 assert.equal(t.manual.length,0);
 t.fire('tourButton','clicked',true);
 assert(t.updates.get('tourResult').includes('1 time.'));
 t.fire('tourButton','clicked',true);
 assert(t.updates.get('tourResult').includes('2 times.'));
}
// Widget Playground: change input, press/release, reset through metadata only.
{
 const t=load('WidgetPlayground');
 for(const [id,val] of Object.entries({wbTarget:48,wbFrequency:25,wbLoad:65,wbMode:'Nominal',wbEnable:true}))t.ui.set(id,val);
 t.fire('wbTarget','valueChanged',48);
 assert(Number(t.updates.get('wbVoltageReadout'))>0);
 t.fire('wbPulse','pressed',true);
 const pressed=t.updates.get('wbVoltageReadout');
 t.fire('wbPulse','released',false);
 assert(t.updates.get('wbVoltageReadout')>pressed);
 t.fire('wbReset','clicked',true);
 assert.equal(t.updates.get('wbTarget'),48);
 assert.equal(t.manual.length,0);
}
// SQLite: retained async callbacks, timer cancellation and SQL parameter binding.
{
 let timer=null;const inserts=[];let opens=0,closes=0;
 const dbApi={
  open(name,cb){opens++;cb({ok:true,db:7,path:'/safe/'+name});},
  exec(handle,sql,params,cb){assert.equal(handle,7);if(sql.startsWith('INSERT'))inserts.push(params);
   if(sql.startsWith('ATTACH'))cb({ok:false,error:'ATTACH denied'});else cb({ok:true});},
  query(handle,sql,params,cb){assert.equal(handle,7);const aggregate=sql.includes('COUNT(');
   cb({ok:true,rowCount:aggregate?1:inserts.length,rows:aggregate?[[inserts.length,3.29,3.30,3.31]]:inserts.map(x=>x.slice())});},
  close(handle,cb){assert.equal(handle,7);closes++;cb({ok:true});}
 };
 const t=load('SQLiteDataLab',{
    Database:dbApi,setInterval(fn,ms){assert.equal(ms,250);timer=fn;return 1;},
    clearInterval(){timer=null;}
 });
 // Existing filterMinVoltage UI property comes from the design definition.
 t.ui.set('filterMinVoltage',3.2);
 t.ctx.onInitialize();assert.equal(opens,0);
 t.fire('openSqlDb','clicked',true);assert.equal(opens,1);
 t.fire('startSqlLog','clicked',true);assert(timer);
 for(let i=0;i<3;i++)timer();
 assert.equal(inserts.length,3);
 t.fire('stopSqlLog','clicked',true);assert.equal(timer,null);
 t.fire('querySql','clicked',true);
 t.fire('filterSql','clicked',true);
 t.fire('queryStats','clicked',true);
 t.fire('invalidSql','clicked',true);
 t.fire('closeSqlDb','clicked',true);assert.equal(closes,1);
 assert.equal(t.manual.length,0);
 t.ctx.onShutdown();
}
console.log('PASS designer event tutorial, widget state/press/release, SQLite async logging/queries, no duplicated handlers');
