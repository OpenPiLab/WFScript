// Physical-like delayed SDK readback model for the bundled Power Supplies Panel.
// No Digilent hardware is connected in this test.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.join(__dirname,'..');
const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/PowerSupplies.wfpanel'),'utf8'));
const script=fs.readFileSync(path.join(root,'examples/source/PowerSupplies.js'),'utf8');
assert.equal(panel.script,script,'bundled Panel script must be synchronized');
const controlList=panel.pages.flatMap(p=>p.controls);
const controls=new Map(controlList.map(c=>[c.id,c.value]));
const updates=[];
const registeredUiEvents=[];
let timerNext=0;
const timers=new Map();
let runs=0,configs=0,stops=0,statusReads=0;
let failedStop=false,canMeasure=true,throwStatus=false;
let applied={vp:0,vn:0};
let pending={vp:0,vn:0};
let settlingReads=0;
let running=false;
const output={PositiveSupply:{Voltage:{value:1},Enable:{value:0}},
              NegativeSupply:{Voltage:{value:-1},Enable:{value:0}}};
function committed() {
  pending={vp:running && output.PositiveSupply.Enable.value===1 ? output.PositiveSupply.Voltage.value : 0,
           vn:running && output.NegativeSupply.Enable.value===1 ? output.NegativeSupply.Voltage.value : 0};
  settlingReads=2; // a couple of AnalogIO status calls still show the old reading
}
const supplies={
  Output:output,
  get running(){return running;},
  get lastError(){return failedStop?'Emergency stop was not confirmed':'';},
  run(){runs++;running=true;committed();return true;},
  config(){configs++;committed();return true;},
  stop(){stops++;if(failedStop)return false;running=false;committed();return true;},
  status(){
    statusReads++;
    if(throwStatus)throw Error('transient status failure');
    if(settlingReads>0)settlingReads--;else applied={...pending};
    return {running,simulation:false,measuredAvailable:canMeasure,
      positiveVoltage:canMeasure?applied.vp:null,
      negativeVoltage:canMeasure?applied.vn:null,
      positiveSetpoint:output.PositiveSupply.Voltage.value,
      negativeSetpoint:output.NegativeSupply.Voltage.value,
      readbackError:canMeasure?'':'readback unsupported'};
  }
};
const ui={
  get(id){assert(controls.has(id),`missing control: ${id}`);return controls.get(id);},
  set(id,value){assert(controls.has(id),`missing control: ${id}`);controls.set(id,value);updates.push([id,value]);return true;},
  on(id,event){registeredUiEvents.push(`${id}::${event}`);}
};
const ctx={Supplies:supplies,ui,print(){},Date,Math,Number,Boolean,String,JSON,Error,isFinite,
  setInterval(callback,ms){assert.equal(ms,250,'expected 250ms readback interval');
    const id=++timerNext;timers.set(id,callback);return id;},
  clearInterval(id){assert(timers.delete(id),'clear an existing timer');}
};
vm.createContext(ctx);
vm.runInContext(script,ctx,{filename:'PowerSupplies.js',timeout:2000});
assert.deepEqual(registeredUiEvents,[], 'all Panel events should be saved designer bindings');
const event=(id,event='clicked')=>{
  const def=controlList.find(c=>c.id===id);
  assert(def && def.events[event] && def.events[event].enabled,`missing visual binding for ${id}/${event}`);
  const fn=ctx[def.events[event].handler];
  assert.equal(typeof fn,'function');
  return fn(controls.get(id));
};
function tick(){for(const fn of [...timers.values()])fn();}
const set=(id,v,eventName='valueChanged')=>{controls.set(id,v);event(id,eventName);};
function read(id){return controls.get(id);}
assert.equal(runs,0,'startup must not energize supplies');
assert.equal(statusReads,1,'startup performs one read-only status call');
assert.equal(timers.size,0,'no idle polling before explicit power on');
set('psVPlus',3.3);set('psVMinus',-3.3);
set('psEnablePlus',true,'toggled');set('psEnableMinus',true,'toggled');
assert.equal(runs,0,'stage-only edits must not energize outputs');
assert.equal(configs,0,'stage-only edits must not configure hardware');
event('psPowerOn');
assert.equal(runs,1,'explicit POWER ON starts output');
assert.equal(timers.size,1,'monitor starts when output becomes active');
assert.equal(read('psPlusReadout'),0,'immediate physical status may still be stale');
tick();tick();
assert.equal(read('psPlusReadout'),3.3,'positive readback catches up without operator refresh');
assert.equal(read('psMinusReadout'),-3.3,'negative readback catches up without operator refresh');
assert.equal(runs,1);
assert.equal(configs,0,'polling MUST NOT reconfigure supplies');
set('psVPlus',4.2);
assert.equal(configs,1,'live setting change configures once');
assert.equal(timers.size,1,'live change does not duplicate monitor');
assert.equal(read('psPlusReadout'),3.3,'new physical reading may still lag on change');
tick();tick();
assert.equal(read('psPlusReadout'),4.2,'periodic status eventually shows actual updated reading');
assert.equal(read('psMinusReadout'),-3.3);
set('psEnableMinus',false,'toggled');
assert.equal(configs,2);
tick();tick();
assert.equal(read('psMinusReadout'),0,'disabled channel readback settles to zero');
// No readback is not a physical 0 V measurement; preserve last known reading
// and communicate status unavailability instead.
canMeasure=false;
const lastReadout=read('psPlusReadout');
tick();
assert.equal(read('psPlusReadout'),lastReadout);
assert(String(read('psStatus')).includes('readback unavailable'));
assert.equal(read('psState'),1,'missing physical readback must be flagged as warning');
canMeasure=true;
// A failed stop must not claim the master is OFF or stop monitoring.
failedStop=true;
event('psPowerOff');
assert.equal(read('psMaster'),1);
assert.equal(timers.size,1);
assert(String(read('psStatus')).includes('STOP NOT CONFIRMED'));
failedStop=false;
event('psPowerOff');
assert.equal(read('psMaster'),0);
assert.equal(running,false);
assert.equal(read('psPlusReadout'),4.2,'physical output readback may remain from previous sample');
assert.equal(timers.size,1,'brief bounded polling continues after OFF until readback settles');
tick();tick();
assert.equal(read('psPlusReadout'),0,'post-off readback eventually returns zero');
assert.equal(read('psMinusReadout'),0);
assert.equal(timers.size,0,'post-off polling stops after readback settles');
// When an active polling read fails, stop polling rather than changing hardware.
event('psPowerOn');
assert.equal(timers.size,1);
throwStatus=true;
tick();
assert.equal(timers.size,0,'status exception cannot create a runaway timer');
assert.equal(running,true,'readback failure must not issue an unsafe output command');
assert(String(read('psStatus')).includes('READBACK MONITOR ERROR'));
throwStatus=false;
event('psPowerOff');
assert.equal(running,false);
console.log('PASS: PowerSupplies live readback settles after POWER ON/config, one timer, no polling writes, STOP/failure safety, unavailable readback, and visual bindings');
