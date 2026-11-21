// Actual designer dispatch for Phase 2 examples (Simulation only; no DWF).
const assert=require('node:assert/strict');
const fs=require('fs'),path=require('path'),vm=require('node:vm');
const root=path.join(__dirname,'..');
function load(stem,extras={}){
 const project=JSON.parse(fs.readFileSync(path.join(root,'examples/panels',stem+'.wfpanel'),'utf8'));
 const source=fs.readFileSync(path.join(root,'examples/source',stem+'.js'),'utf8');
 assert.equal(project.script,source);
 const values=new Map(),manual=new Map(),updates=[];
 const controls=project.pages.flatMap(p=>p.controls);
 for(const c of controls)values.set(c.id,c.text||'');
 const ui={get(id){assert(values.has(id),id);return values.get(id);},
  set(id,value){assert(values.has(id),id);updates.push([id,value]);values.set(id,value);return true;},
  on(id,ev,fn){const key=id+'::'+ev;assert(!manual.has(key));manual.set(key,fn);}};
 // JS timers execute on the Panel runtime worker; emulate their lifetime
 // rather than treating setInterval as an unavailable browser global.
 let timerId=0;
 const timers=new Map();
 const ctx={ui,print(){},Math,Number,String,Boolean,Date,JSON,Error,isFinite,parseInt,
  setInterval(fn,ms){assert.equal(typeof fn,'function');assert(ms>=10);const id=++timerId;timers.set(id,fn);return id;},
  clearInterval(id){timers.delete(id);},...extras};
 vm.createContext(ctx);vm.runInContext(source,ctx,{filename:stem+'.js',timeout:2000});
 const bound=new Map();
 for(const c of controls)for(const [event,b] of Object.entries(c.events||{})){
  const key=c.id+'::'+event;assert.equal(b.enabled,true);
  assert.equal(typeof ctx[b.handler],'function',stem+' '+b.handler);
  assert(!manual.has(key),'double callback '+key);
  bound.set(key,ctx[b.handler]);
 }
 function fire(id,event='clicked',value=true){const fn=bound.get(id+'::'+event);assert(fn,id+'::'+event);return fn(value);}
 return {fire,ctx,values,updates,manual,bound,timers};
}
// Label demo: exactly one designer event, with a visible ui.set result.
{
 const x=load('LabelFormatting');assert.equal(x.bound.size,1);assert.equal(x.manual.size,0);
 x.fire('demoUpdate');assert(x.values.get('demoLive').includes('TEST RESULT'));
}
// Static DIO keeps the 16 generated handlers on purpose; the three action
// controls are discoverable through the designer, not ui.on registrations.
{
 let statusReads=0,stopCalls=0,outputWrites=0;
 const mock={simulation:true,lastError:'',status(){statusReads++;return {valid:true,simulation:true,
  outputEnableMask:0,inputMask:0};},setPin(){outputWrites++;return true;},stop(){stopCalls++;return true;}};
 const x=load('StaticDigitalIO',{StaticIO:mock,wait(){}});
 assert.equal(statusReads,1,'initial passive snapshot');assert.equal(outputWrites,0,'startup never drives outputs');
 assert.equal(x.bound.size,3);assert.equal(x.manual.size,16);
 x.fire('dioRead');assert.equal(statusReads,2);
 x.fire('dioAllZ');assert.equal(stopCalls,1);
 x.fire('dioLoopback');assert.equal(outputWrites,0,'simulation loopback never drives physical pins');
}
// Power supplies preserve the explicit POWER ON operation and settings reads.
{
 let starts=0,stops=0,configs=0;
 const state={running:false,simulation:true,measuredAvailable:true,
  positiveVoltage:0,negativeVoltage:0,positiveSetpoint:0,negativeSetpoint:0};
 const supplies={lastError:'',running:false,
  Output:{PositiveSupply:{Voltage:{value:0},Enable:{value:0}},NegativeSupply:{Voltage:{value:0},Enable:{value:0}}},
  status(){return {...state};},run(){starts++;state.running=true;return true;},
  stop(){stops++;state.running=false;return true;},config(){configs++;return true;}};
 const x=load('PowerSupplies',{Supplies:supplies});
 x.values.set('psVPlus',3.3);x.values.set('psVMinus',-3.3);
 x.values.set('psEnablePlus',true);x.values.set('psEnableMinus',true);
 assert.equal(starts,0);assert.equal(stops,0,'startup cannot energize hardware');
 assert.equal(x.bound.size,7);assert.equal(x.manual.size,0);
 x.fire('psRefresh');assert.equal(starts,0);
 x.fire('psPowerOn');assert.equal(starts,1);assert.equal(x.timers.size,1,'live readback polling starts');
 x.fire('psPowerOff');assert.equal(stops,1);assert.equal(x.timers.size,0,'poll stops after power-off');
}
console.log('PASS: designer event dispatch for Label, StaticIO and PowerSupplies; no duplicate actions or startup writes');
