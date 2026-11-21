const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('examples/source/ScriptUtilityLab.js','utf8');
const panel=JSON.parse(fs.readFileSync('examples/panels/ScriptUtilityLab.wfpanel','utf8'));
assert.equal(panel.pages.length,2);
for (const page of panel.pages) {
  for (const key of ['id','name','width','height','controls']) assert(page[key]!==undefined,`Page ${page.id} missing ${key}`);
  assert.equal(page.width,1160);
  assert.equal(page.height,800);
  assert(page.controls.length>0);
}
const controls=panel.pages.flatMap(p=>p.controls);const ids=new Set(controls.map(x=>x.id));
const handlers=new Map(),store=new Map(),updates=[];let tick,stopCount=0;
const ctx={Math,JSON,Date,Number,String,print:()=>{},
  ui:{on:(id,event,fn)=>{assert(ids.has(id),id);assert.equal(event,'clicked');handlers.set(id,fn);},set:(id,value)=>{assert(ids.has(id),id);updates.push({id,value});}},
  FileIO:{dataDirectory:()=>'/safe',dataPath:n=>'/safe/'+n,exists:n=>store.has(n),readText:n=>store.get(n)||'',writeText:(p,v)=>{store.set(p,v);return true},appendText:(p,v)=>{store.set(p,(store.get(p)||'')+v);return true},readCsv:p=>store.get(p)||[],writeCsv:(p,v)=>{store.set(p,v);return true},readJson:p=>store.get(p),writeJson:(p,v)=>{store.set(p,v);return true},readBytes:p=>store.get(p),writeBytes:(p,v)=>{store.set(p,v);return true}},
  Tool:{question:(msg,cb)=>cb({kind:'question',accepted:true,value:true}),getText:(msg,init,cb)=>cb({kind:'getText',accepted:true,value:init}),getNumber:(msg,init,min,max,cb)=>cb({kind:'getNumber',accepted:true,value:init}),getItem:(msg,items,cb)=>cb({kind:'getItem',accepted:true,value:items[0]}),getOpenFile:(msg,filter,cb)=>cb({kind:'getOpenFile',accepted:false,value:''}),getSaveFile:(msg,filter,cb)=>cb({kind:'getSaveFile',accepted:true,value:'/safe/saved.csv'})},
  setInterval:fn=>{tick=fn;return 1;},clearInterval:()=>{tick=null;stopCount++;}}
vm.createContext(ctx);vm.runInContext(source,ctx,{filename:'ScriptUtilityLab.js'});
// Simulate PanelRuntime's project-owned event dispatch, not legacy ui.on().
for(const control of controls) for(const [event,binding] of Object.entries(control.events||{})) {
  assert.equal(event,'clicked');
  assert.equal(binding.enabled,true);
  assert(!handlers.has(control.id),'duplicate event registration '+control.id);
  assert.equal(typeof ctx[binding.handler],'function',binding.handler);
  handlers.set(control.id,ctx[binding.handler]);
}
assert.equal(handlers.size,14);
ctx.onInitialize();
['saveConfig','readConfig','writeBinary','startLog'].forEach(k=>handlers.get(k)());
for(let i=0;i<4;i++)tick();
handlers.get('stopLog')();
for(const k of ['readCsv','exportCsv','importCsv','askConfirm','askText','askNumber','askItem','askFileOpen','askFileSave'])handlers.get(k)();
ctx.onShutdown();
assert(store.has('/safe/saved.csv'));assert(updates.length>5);
console.log(`PASS ${controls.length} controls, ${handlers.size} events, CSV/JSON/binary/dialogs, interval cleanup`);
