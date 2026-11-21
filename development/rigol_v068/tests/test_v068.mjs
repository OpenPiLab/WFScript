import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import * as Steps from '../RigolTestSteps.mjs';
import * as Core from '../../rigol_v064/libraries/WFScpiCore.mjs';
import * as Scope from '../../rigol_v064/libraries/RigolMHO900.mjs';
import * as Gen from '../../rigol_v064/libraries/RigolDG900Pro.mjs';
import * as Meter from '../../rigol_v064/libraries/RigolDM858.mjs';
import * as Psu from '../../rigol_v064/libraries/RigolDP900.mjs';
const filepath=new URL('../Rigol_Four_Instrument_Operator_Bench_v0_6_8_Structured_Results.wfpanel',import.meta.url);
const panel=JSON.parse(fs.readFileSync(filepath,'utf8'));
const base=JSON.parse(fs.readFileSync(new URL('../../rigol_v064/Rigol_Four_Instrument_Operator_Bench_v0_6_4_Native_Charts.wfpanel',import.meta.url),'utf8'));
assert.equal(panel.pages.length,5);
for(let p=0;p<4;p++) assert.deepEqual(panel.pages[p],base.pages[p],`Original ${p} page changed`);
for(let i=0;i<5;i++) assert.equal(panel.libraries[i].source,base.libraries[i].source,`Embedded driver ${i} mutated`);
assert.equal(panel.testLibrary,'RigolTestSteps.mjs');
assert.equal(panel.testDefinition.steps.length,38);
assert.equal(panel.testDefinition.steps.filter(s=>s.type==='numeric').length,34);
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
let currentToken=null; let currentResolve=null;let currentReject=null;
const runner={
 isActive(token){return token===currentToken && Boolean(currentResolve);},
 complete(token,value){if(!this.isActive(token))return false;const cb=currentResolve;currentToken=null;currentResolve=null;currentReject=null;cb(value===undefined?true:value);return true;},
 fail(token,message){if(!this.isActive(token))return false;const cb=currentReject;currentToken=null;currentResolve=null;currentReject=null;cb(Error(message));return true;},
 setVariable(k,v){reportVariables[k]=v;return true;},onFinished(cb){finished=cb;return true;},run(){return true;},terminate(){return true;},reset(){return true;},
 exportReport(f){return '/reports/test.'+f;},archiveInfo(){return {status:'saved',path:'/reports/record.json'}},lastError:''};
const plots={setSeries(){},setNativeSamples(){},clear(){},configure(){}};
const ctx={ui:{get:id=>state[id],set:(id,v)=>{state[id]=v;}},plots,
  modules:{load:n=>({'WFScpiCore.mjs':Core,'RigolMHO900.mjs':Scope,'RigolDG900Pro.mjs':Gen,
    'RigolDM858.mjs':Meter,'RigolDP900.mjs':Psu,'RigolTestSteps.mjs':Steps})[n]},
  TestRunner:runner,print:(x)=>logs.push(String(x)),TCP:{create(){throw Error('hardware not used')}},
  Math,JSON,Number,Boolean,Array,Map,Set,String,Error,Date,Promise,
  setTimeout,clearTimeout,setInterval,clearInterval};
vm.createContext(ctx);
vm.runInContext(panel.script,ctx,{filename:'RigolBench_4Instruments_v0_6_8.js'});
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

const measurements=[];let serial=0;
async function runStep(step) {
    const token='test_'+(++serial)+'_'+step.id;
    const context={token,id:step.id,params:step.parameters||{},runId:'mockRun'};
    let result;
    if(step.mode==='callback') {
        const promise=new Promise((resolve,reject)=>{
            currentToken=token;currentResolve=resolve;currentReject=reject;
        });
        const immediate=Steps[step.handler](context);
        assert.equal(immediate,undefined,'Callback handler should not return a direct result');
        let watchdog;
        try {
            result=await Promise.race([promise,new Promise((_,reject)=>{
                watchdog=setTimeout(()=>reject(Error('Timeout '+step.id)),8000);
            })]);
        } finally { clearTimeout(watchdog); }
    } else if(step.mode==='once') result=Steps[step.handler](context);
    else throw Error('Unexpected '+step.mode);
    if(step.type==='numeric') {
        if(step.measurements) {
            assert.equal(typeof result,'object',step.id);
            for(const [path,rule] of Object.entries(step.measurements)) {
                const number=result[path];
                assert(Number.isFinite(number),'Invalid '+path+' in '+step.id);
                const pass=number>=rule.low && number<=rule.high;
                measurements.push({id:step.id,path,result:number,pass});
                assert(pass,'FAILED '+step.id+' '+path+' '+number);
            }
        } else {
            assert.equal(typeof result,'number',step.id);
            assert(Number.isFinite(result),step.id);
            const pass=result>=step.limits.low && result<=step.limits.high;
            measurements.push({id:step.id,result,pass});
            assert(pass,'FAILED '+step.id+' '+result);
        }
    } else assert.equal(result,true,'Action '+step.id+' did not complete');
}
ctx.onRigolTestRun();
for(const step of panel.testDefinition.steps)await runStep(step);
assert.equal(measurements.length,50);
assert.equal(reportVariables.rigolMode,'PHYSICAL_RIGOL_SCPI_TCP');
assert(measurements.every(m=>m.pass));
assert.equal(psuOn,false);
assert.equal(genOn,false);
assert.equal(Steps.state().psuOwned,false);
assert.equal(Steps.state().genOwned,false);
assert(state.rigolTestLatest.includes('square_2Vpp_5000Hz'));
finished({state:'passed',passed:50,failed:0});
console.log('PASS: 38 structured callback/once steps; 50 measurements; physical-state mocks; both outputs OFF');

// Reject a physical PSU ON before the test takes control.
psuOn=true;
let rejected=false;
try{await runStep(panel.testDefinition.steps[0]);}
catch(e){rejected=true;assert.match(String(e),/must be physically OFF/);}
assert(rejected);
assert.equal(psuOn,true,'An operator-owned output must remain untouched');
assert.equal(Steps.state().psuOwned,false);
console.log('PASS: preflight rejects operator-owned PSU ON without switching it off');
psuOn=false;

// Overload fails a numeric callback, and the original cleanup still works.
await runStep(panel.testDefinition.steps[0]);
await runStep(panel.testDefinition.steps[1]);
const oldRead=meter.acquisition.read;
meter.acquisition.read=cb=>later(cb,{value:null,overload:true,function:'dcVoltage'});
rejected=false;
try{await runStep(panel.testDefinition.steps[2]);}
catch(e){rejected=true;assert.match(String(e),/overload/);}
assert(rejected);
await runStep(panel.testDefinition.steps.at(-2));
await runStep(panel.testDefinition.steps.at(-1));
assert.equal(psuOn,false);
meter.acquisition.read=oldRead;
console.log('PASS: DMM overload fails a callback, cleanup remains effective');

// A stale token is never allowed to complete the current step.
assert.equal(runner.complete('old-step',5),false);
assert.equal(runner.fail('old-step','stale'),false);
console.log('PASS: stale completion and error callbacks rejected');
