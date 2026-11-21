'use strict';
// Offline behavioral tests for Phase 3 operator Panels. These mock the native
// host and dispatch ONLY through project-owned control.events, never ui.on().
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'..');
function load(stem,extras={}) {
 const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels',stem+'.wfpanel'),'utf8'));
 const source=fs.readFileSync(path.join(root,'examples/source',stem+'.js'),'utf8');
 assert.equal(panel.script,source);
 const controls=panel.pages.flatMap(p=>p.controls),values=new Map(),bound=new Map(),updates=[];
 for(const c of controls)values.set(c.id,c.value ?? c.text ?? null);
 const manual=new Map();
 const ui={get(id){assert(values.has(id),id);return values.get(id);},
  set(id,value){assert(values.has(id),id);updates.push([id,value]);values.set(id,value);return true;},
  on(id,event,fn){const k=id+'::'+event;assert(!manual.has(k),k);manual.set(k,fn);}};
 const ctx={ui,print(){},Math,Array,Number,String,Boolean,Date,JSON,Error,isFinite,parseInt,parseFloat,...extras};
 vm.createContext(ctx);
 vm.runInContext(source,ctx,{filename:stem+'.js',timeout:3000});
 for(const c of controls)for(const [event,b] of Object.entries(c.events||{})) {
  const k=c.id+'::'+event;
  assert(b.enabled && typeof ctx[b.handler]==='function',k+' missing '+b.handler);
  assert(!manual.has(k),'double dispatch '+k);
  bound.set(k,ctx[b.handler]);
 }
 assert.equal(manual.size,0,stem+' unexpectedly registers manual UI events');
 function fire(id,event='clicked',arg){const k=id+'::'+event;assert(bound.has(k),k);return bound.get(k)(arg);}
 return {panel,ctx,values,updates,bound,manual,fire};
}
function testRunner() {
 const calls={run:0,reset:0,next:0,terminate:0,exports:[]};
 const runner={lastError:'',onFinished(fn){this.onComplete=fn;},
  run(){calls.run++;return true;},reset(){calls.reset++;return true;},next(){calls.next++;return true;},
  terminate(){calls.terminate++;return true;},
  summary(){return {state:'IDLE',passed:0,failed:0,elapsedMs:0,totalTimeoutMs:15000};},
  report(){return {steps:[],measurements:[],runInfo:{},executionId:null};},
  archiveInfo(){return {status:'idle'};},
  exportReport(format){calls.exports.push(format);return 'mock.'+format;},
  pendingRunInfo(){return {serial:'mock'};}, runInfo(){return {};}};
 return {runner,calls};
}
// Simulator-only Automated Test Station: no test auto-run on project load.
{
 const {runner,calls}=testRunner();
 const x=load('AutomatedTestStation',{TestRunner:runner});
 assert.equal(x.bound.size,10);
 assert.equal(calls.run,0);
 x.fire('uutRun');assert.equal(calls.run,1);
 x.fire('stationPass');assert.equal(calls.run,2);
 x.fire('stationNext');assert.equal(calls.next,1);
 x.fire('stationFail');assert.equal(calls.terminate,1);
 x.fire('uutReset');x.fire('stationReset');assert.equal(calls.reset,2);
 x.fire('reportRefresh');x.fire('reportExportJson');x.fire('reportExportCsv');
 assert.deepEqual(calls.exports,['json','csv']);
 x.fire('mirrorRead');assert(String(x.values.get('mirrorStatus')).includes('Pending UUT'));
}
// AD3 hardware station: the operator Panel never opens hardware at startup.
{
 const {runner,calls}=testRunner();
 const x=load('AD3HardwareTestStation',{TestRunner:runner});
 assert.equal(x.bound.size,9);
 assert.equal(calls.run,0);
 x.fire('stationRun');x.fire('uutRun');assert.equal(calls.run,2);
 x.fire('stationStop');assert.equal(calls.terminate,1);
 x.fire('stationReset');x.fire('uutReset');assert.equal(calls.reset,2);
 x.fire('reportExportJson');x.fire('reportExportCsv');assert.deepEqual(calls.exports,['json','csv']);
}
// Visual Signal Lab: initial waveform preview is math only, then explicit
// start, running shape edits, stop, and fail-safe shutdown on reconfigure fault.
{
 let runs=0,stops=0,shouldFail=false,previews=0;
 const wavegen={lastError:'mock W1 error',Channel1:{enable:false,Mode:{text:'Simple'},Simple:{Type:{text:'Sine'},Frequency:{value:0},Amplitude:{value:0},Offset:{value:0}}},
  run(){runs++;return !shouldFail;},stop(){stops++;return true;},status(){return {carrierFunction:this.Channel1.Simple.Type.text};}};
 const plots={clear(){return true;},setSeries(){previews++;return true;}};
 const x=load('SignalLab',{Wavegen1:wavegen,Device:{SN:'DEMO'},plots});
 assert.equal(x.bound.size,8);assert.equal(runs,0,'startup must not energize W1');
 x.values.set('sigShape','Sine');x.values.set('sigFrequency',1000);x.values.set('sigAmplitude',1);
 x.values.set('sigOffset',0);x.values.set('sigRate','1 MS/s');x.values.set('sigSamples',4096);x.values.set('sigRange','5 V');
 x.fire('sigPreview');assert(previews>0 && runs===0);
 x.fire('sigStart');assert.equal(runs,1);
 x.values.set('sigShape','Square');x.fire('sigShape','selectionChanged','Square');
 assert.equal(wavegen.Channel1.Simple.Type.text,'Square');assert.equal(runs,2);
 shouldFail=true;x.values.set('sigFrequency',1500);x.fire('sigFrequency','valueChanged',1500);
 assert.equal(stops,1,'failure must stop live W1');
 const prevRuns=runs;x.fire('sigStop');assert.equal(stops,2);
 x.values.set('sigAmplitude',1.2);x.fire('sigAmplitude','valueChanged',1.2);
 assert.equal(runs,prevRuns,'subsequent setting edits must only preview after STOP');
}
// Chart helper refactor: new named designer handlers invoke the original safe
// action wrapper and continue to update plots, with no ui.on registrations.
for(const [stem,buttons] of [
 ['AnalogCharts',['analogRefresh','xyRefresh','freqRefresh']],
 ['DigitalTimingCharts',['logicRefresh','protoRefresh']],
 ['ChartTechniques',['trendAppend','trendReset','axisToggle','axisStyle']]]) {
 const calls={series:0,clears:0,append:0};
 const plots={setSeries(){calls.series++;},clear(){calls.clears++;},
  appendPoint(){calls.append++;},configure(){},styleSeries(){}};
 const x=load(stem,{plots});
 assert.equal(x.bound.size,buttons.length);
 for(const id of buttons)x.fire(id);
 assert(calls.series>0 || calls.append>0,stem+' produced no plotted data');
}
// Native measurement widgets remain driven through the same simulated data.
{
 const x=load('AutomatedTestBench');
 assert.equal(x.bound.size,3);
 x.fire('runDemo');assert.equal(x.values.get('resultPwm'),50.1);
 x.fire('injectFailure');assert.equal(x.values.get('resultPwm'),47.5);
 x.fire('resetDemo');assert.equal(typeof x.values.get('measurements'),'object');
}
console.log('PASS: Phase 3 Test Executive, Signal Lab W1 safety, Charts and Measurement Panel handlers; no duplicates');
