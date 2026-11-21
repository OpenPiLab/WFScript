import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import * as Steps from '../libraries/RigolTestSteps.mjs';
import * as Core from '../../rigol_v064/libraries/WFScpiCore.mjs';
import * as Scope from '../../rigol_v064/libraries/RigolMHO900.mjs';
import * as Gen from '../../rigol_v064/libraries/RigolDG900Pro.mjs';
import * as Meter from '../../rigol_v064/libraries/RigolDM858.mjs';
import * as Psu from '../../rigol_v064/libraries/RigolDP900.mjs';
const filepath=new URL('../Rigol_Four_Instrument_Operator_Bench_v0_6_6_Readable_Tests.wfpanel',import.meta.url);
const panel=JSON.parse(fs.readFileSync(filepath,'utf8'));
const base=JSON.parse(fs.readFileSync(new URL('../../rigol_v064/Rigol_Four_Instrument_Operator_Bench_v0_6_4_Native_Charts.wfpanel',import.meta.url),'utf8'));
assert.equal(panel.pages.length,5);
for(let p=0;p<4;p++) assert.deepEqual(panel.pages[p],base.pages[p],`Original ${p} page changed`);
for(let i=0;i<5;i++) assert.equal(panel.libraries[i].source,base.libraries[i].source,`Embedded driver ${i} mutated`);
assert.equal(panel.testLibrary,'RigolTestSteps.mjs');
assert.equal(panel.testDefinition.steps.length,62);
assert.equal(panel.testDefinition.steps.filter(s=>s.type==='numeric').length,50);
assert.equal(panel.pages[4].controls.length,17);
const names=new Set();
for(const page of panel.pages) for(const c of page.controls) {assert(!names.has(c.id));names.add(c.id);}
const exports=new Set(Object.keys(Steps));
for(const step of panel.testDefinition.steps){
  if(step.handler)assert(exports.has(step.handler),step.handler);
  if(step.handlers) for(const h of Object.values(step.handlers))assert(exports.has(h),h);
}
const state={};const logs=[];
for(const page of panel.pages)for(const c of page.controls)
  state[c.id] = c.type==='option_selector'?(c.properties.options||'').split('|')[c.properties.defaultIndex||0] :
    c.type==='toggle'?Boolean(c.value):
    (c.type==='ip_address_input'||c.type==='text_input')?c.properties.defaultText:c.text;
state.rigolTestWiring=true;
let finished=null;
const reportVariables={};
const runner={setVariable(k,v){reportVariables[k]=v;return true;},onFinished(cb){finished=cb;return true;},run(){return true;},terminate(){return true;},reset(){return true;},
 exportReport(f){return '/reports/test.'+f;},archiveInfo(){return {status:'saved',path:'/reports/record.json'}},lastError:''};
const plots={setSeries(){},setNativeSamples(){},clear(){},configure(){}};
const ctx={ui:{get:id=>state[id],set:(id,v)=>{state[id]=v;}},plots,
  modules:{load:n=>({'WFScpiCore.mjs':Core,'RigolMHO900.mjs':Scope,'RigolDG900Pro.mjs':Gen,
    'RigolDM858.mjs':Meter,'RigolDP900.mjs':Psu,'RigolTestSteps.mjs':Steps})[n]},
  TestRunner:runner,print:(x)=>logs.push(String(x)),TCP:{create(){throw Error('hardware not used')}},
  Math,JSON,Number,Boolean,Array,Map,Set,String,Error,Date,Promise,
  setTimeout,clearTimeout,setInterval,clearInterval};
vm.createContext(ctx);
vm.runInContext(panel.script,ctx,{filename:'RigolBench_4Instruments_v0_6_6.js'});
globalThis.RigolTestHost=ctx.RigolTestHost;
globalThis.TestRunner=runner;
function later(cb,v){setTimeout(()=>cb(null,v),0)}
let powerV=0,psuOn=false,genOn=false,genConfig=null,scopeScale=0.001,scopeVertical=0.5,scopeRunning=false,scopeHz=1000;
let instruments={};
const psu={connected:true,ch1:{
 getOutput(cb){later(cb,psuOn)},
 setup(opts,cb){assert.equal(opts.voltage,5);assert.equal(opts.current,.20);assert.equal(psuOn,false);powerV=5;later(cb,true);},
 enable(cb){psuOn=true;later(cb,true);},disable(cb){psuOn=false;later(cb,true);},
 setVoltage(voltage,cb){assert(psuOn);powerV=voltage;later(cb,true);},
 measureVoltage(cb){later(cb,powerV+.001)}
}};
const gen={connected:true,ch1:{
 getOutput(cb){later(cb,genOn)},
 configure(o,cb){assert.equal(genOn,false);genConfig=o;assert.equal(o.load,'highZ');assert.equal(o.offset,0);later(cb,{shape:o.shape,frequency:o.frequency,amplitude:o.amplitude});},
 enable(cb){assert(genConfig);genOn=true;later(cb,true)},
 disable(cb){genOn=false;later(cb,true)}
}};
function waveform(){
  assert(genOn,'Scope sampling while generator OFF');
  const {shape,amplitude,frequency}=genConfig;
  assert.equal(scopeHz,frequency);
  const t=Array.from({length:1000},(_,i)=>i*10/frequency/1000);
  const voltage=t.map((time,i)=>{
    const theta=2*Math.PI*frequency*time;
    let value=shape==='sine'?Math.sin(theta):Math.sin(theta)>=0?1:-1;
    return value*amplitude/2+.001*Math.sin(i*.7);
  });
  return {time:t,ch1:{voltage}};
}
const scope={connected:true,ch1:{
 getSettings(cb){later(cb,{scale:0.5,impedance:'OMEG',enabled:true})},
 configure(opts,cb){scopeVertical=opts.scale;assert.equal(opts.impedance,1000000);assert.equal(opts.probe,1);later(cb,{enabled:true,scale:opts.scale})}
 },timebase:{configure(opts,cb){scopeHz=1/opts.scale;later(cb,{scale:opts.scale})}},
 trigger:{edge(opts,cb){assert.equal(opts.channel,1);later(cb,{level:0})}},
 acquisition:{run(cb){scopeRunning=true;later(cb,true)},stop(cb){assert(scopeRunning);scopeRunning=false;later(cb,true)},
 status(cb){later(cb,scopeRunning?'RUN':'STOP')}},
 waveform:{captureAll(opts,cb){assert(!scopeRunning);assert.equal(opts.channels[0],1);later(cb,waveform());}}
};
const meter={connected:true,scpi:{query(cmd,cb){assert.equal(cmd,'*IDN?');later(cb,'RIGOL,DM858,TEST,1.0')}},
 configure:{dcVoltage(opts,cb){assert.equal(opts.nplc,5);later(cb,{function:'dcVoltage'})}},
 acquisition:{read(cb){assert(psuOn);later(cb,{value:powerV+0.002,overload:false,function:'dcVoltage',unit:'V'})}}};
Object.assign(ctx.dev,{psu,gen,meter,scope});
const nop=x=>x;
// No SCPI 'invoke' is needed: use the same callback promise adapter already supported by the Panel.
ctx.SCPI_CORE={};
ctx.onRigolTestRun();
assert.equal(ctx.rigolTestRunning,true);
assert.equal(state.rigolTestSafety.includes('RUNNING'),true);
const measurements=[],statuses=[];
async function runStep(step){
  let result;
  if(step.handler){result=Steps[step.handler]();}
  else {
    Steps[step.handlers.start]();
    const started=Date.now();
    while(true){
      await new Promise(resolve=>setTimeout(resolve,2));
      result=Steps[step.handlers.poll]();
      if(result!==undefined && result!==null)break;
      if(Date.now()-started>8000)throw Error('Offline step timed out '+step.id);
    }
  }
  if(step.type==='numeric'){
    assert.equal(typeof result,'number',step.id);
    assert(Number.isFinite(result),step.id);
    const pass=result>=step.limits.low&&result<=step.limits.high;
    statuses.push(pass?'PASS':'FAIL');
    measurements.push({id:step.id,result,pass});
  } else assert.equal(result,true,`Action ${step.id} not done`);
}
for(const step of panel.testDefinition.steps) await runStep(step);
assert.equal(measurements.length,50);
assert.equal(reportVariables.rigolMode,'PHYSICAL_RIGOL_SCPI_TCP');
assert.equal(reportVariables.rigolPsuEndpoint,'192.168.6.5:5555');
assert(measurements.every(m=>m.pass),JSON.stringify(measurements.filter(m=>!m.pass)));
assert.equal(psuOn,false);
assert.equal(genOn,false);
assert.equal(Steps.state().genOwned,false);
assert.equal(Steps.state().psuOwned,false);
assert.equal(Steps.state().lastWave.expected.shape,'square');
assert(state.rigolTestLatest.includes('square_2Vpp_5000Hz'));
finished({state:'passed',passed:50,failed:0});
assert.equal(ctx.rigolTestRunning,false);
assert(state.rigolTestSafety.includes('released/untouched'));
assert(state.rigolTestReport.includes('saved'));
console.log('PASS: five pages; original pages/driver bytes unchanged; 62 native steps, 50 numeric; 13 PSU targets, 13 DMM samples, 8 scope waveforms; 50/50 within limits; outputs OFF; report event');

// Failure and preflight: an already-on operator output is never commanded OFF.
psuOn=true;
ctx.rigolTestRunning=true;
Steps.preflight();
let caught=false;
for(let i=0;i<200;i++){
 await new Promise(resolve=>setTimeout(resolve,2));
 try{if(Steps.pollDone()===true)break;}catch(e){caught=true;assert.match(String(e),/must be physically OFF/);break;}
}
assert(caught,'already-on PSU not rejected');
assert(psuOn,'preflight unexpectedly switched off operator output');
assert.equal(Steps.state().psuOwned,false);
console.log('PASS: preflight refuses already-enabled PSU CH1 without changing its state');

// An overload is not a numeric PASS and causes the native executive to take its cleanup path.
psuOn=false;
Steps.preflight();
async function waitPoll(method,timeout=2500){
 const start=Date.now();
 for(;;){
   await new Promise(resolve=>setTimeout(resolve,2));
   const value=Steps[method]();
   if(value!==undefined&&value!==null)return value;
   if(Date.now()-start>timeout)throw Error('Mock waitPoll timeout');
 }
}
await waitPoll('pollDone');
Steps.power500();
assert(Math.abs(await waitPoll('pollValue')-5)<.05);
const originalMeterRead=meter.acquisition.read;
meter.acquisition.read=(cb)=>later(cb,{value:null,overload:true,function:'dcVoltage'});
Steps.dmm500();
let gotOverload=false;
try{await waitPoll('pollValue')}catch(e){gotOverload=true;assert.match(String(e),/overload/);}
assert(gotOverload,'DMM overload must not silently pass');
Steps.cleanupGen();await waitPoll('pollDone');
Steps.cleanupPsu();await waitPoll('pollDone');
assert.equal(psuOn,false);
meter.acquisition.read=originalMeterRead;
console.log('PASS: DMM overload causes explicit failure; independent cleanup verifies PSU OFF');

// STOP during an asynchronous PSU setup cannot enable CH1 AFTER cleanup finishes.
Steps.preflight();await waitPoll('pollDone');
Steps.power500(); // ownership asserted before setup callback
assert.equal(Steps.state().psuOwned,true);
Steps.cleanupGen();await waitPoll('pollDone');
Steps.cleanupPsu();await waitPoll('pollDone');
assert.equal(psuOn,false);
await new Promise(resolve=>setTimeout(resolve,50));
assert.equal(psuOn,false);
console.log('PASS: STOP-before-enable race cannot leave PSU on after cleanup');

// STOP during generator configuration cannot issue an uncanceled enable.
Steps.preflight();await waitPoll('pollDone');
Steps.capture_sine_1Vpp_1000Hz();
// The new interlock verifies PSU OFF and generator OFF before ownership.
const startedGen=Date.now();
while(!Steps.state().genOwned && Date.now()-startedGen < 1000)
  await new Promise(resolve=>setTimeout(resolve,1));
assert.equal(Steps.state().genOwned,true);
Steps.cleanupGen();await waitPoll('pollDone');
Steps.cleanupPsu();await waitPoll('pollDone');
await new Promise(resolve=>setTimeout(resolve,50));
assert.equal(genOn,false);
console.log('PASS: STOP-during-waveform-config race cannot leave generator on after cleanup');

// Edited sequence must not skip the physical PSU-off interlock.
Steps.preflight(); await waitPoll('pollDone');
Steps.power500(); await waitPoll('pollValue');
assert.equal(psuOn,true);
Steps.capture_sine_1Vpp_1000Hz();
let gotSequenceInterlock=false;
try { await waitPoll('pollDone'); }
catch(e) { gotSequenceInterlock=true; assert.match(String(e),/PSU CH1 remains owned/); }
assert(gotSequenceInterlock);
assert.equal(genOn,false);
Steps.cleanupGen(); await waitPoll('pollDone');
Steps.cleanupPsu(); await waitPoll('pollDone');
assert.equal(psuOn,false);
console.log('PASS: edited sequence cannot start generator while test-owned PSU remains on');

// A physically ON, operator-owned PSU must not be silently switched OFF.
Steps.preflight(); await waitPoll('pollDone');
psuOn=true; // Simulates unexpected external state change after preflight.
Steps.capture_sine_1Vpp_1000Hz();
let gotPhysicalInterlock=false;
try { await waitPoll('pollDone'); }
catch(e) { gotPhysicalInterlock=true; assert.match(String(e),/PSU CH1 is physically ON/); }
assert(gotPhysicalInterlock);
assert.equal(psuOn,true,'Interlock must not modify an operator-owned PSU');
assert.equal(genOn,false);
Steps.cleanupGen();await waitPoll('pollDone');
Steps.cleanupPsu();await waitPoll('pollDone');
assert.equal(psuOn,true,'Cleanup must not switch off unowned outputs');
psuOn=false;
console.log('PASS: physical PSU interlock refuses generator with external PSU ON');

// A generator changed to ON between preflight and first waveform capture is not ours to disable.
Steps.preflight();await waitPoll('pollDone');
genOn=true;
Steps.capture_sine_1Vpp_1000Hz();
let gotGeneratorGuard=false;
try { await waitPoll('pollDone'); }
catch(e) { gotGeneratorGuard=true; assert.match(String(e),/Generator CH1 unexpectedly ON/); }
assert(gotGeneratorGuard);
assert.equal(Steps.state().genOwned,false,'Do not take ownership after rejected external generator state');
Steps.cleanupGen();await waitPoll('pollDone');
Steps.cleanupPsu();await waitPoll('pollDone');
assert.equal(genOn,true,'Cleanup must not switch off operator-owned generator');
genOn=false;
console.log('PASS: generator ownership acquired only after initial physical OFF verification');
