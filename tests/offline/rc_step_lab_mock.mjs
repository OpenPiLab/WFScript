import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import * as Step from '../../examples/libraries/RCStepAnalysis.mjs';
const path=new URL('../../examples/panels/RCStepResponseLaboratory.wfpanel',import.meta.url);
const p=JSON.parse(fs.readFileSync(path,'utf8'));
assert.equal(p.pages.length,11);
assert.equal(p.pages.flatMap(x=>x.controls).length,116);
assert.deepEqual(p.libraries.map(x=>x.name),['RCStepAnalysis.mjs']);
assert.equal(p.libraries[0].source,fs.readFileSync(new URL('../../examples/libraries/RCStepAnalysis.mjs',import.meta.url),'utf8'));
assert.equal(p.script,fs.readFileSync(new URL('../../examples/source/RCStepResponseLaboratory.js',import.meta.url),'utf8'));
const all=p.pages.flatMap(x=>x.controls),ids=new Set(all.map(x=>x.id));
assert.equal(ids.size,all.length);
const table=all.find(c=>c.id==='stepNotebook');
assert.equal(table.properties.columns.split('|').length,12);
assert.equal(table.properties.maxRows,500);
let known=Object.fromEntries(all.map(c=>[c.id,c.value]));
let calls={run:0,stop:0,scopeStop:0,scopeSingle:0,files:{},plots:0,timers:[],logs:[],status:()=>({carrierFunction:'Square'})};
let physical=false,forcedScopeFailure=false;
const rate={value:250000,real:250000},samples={value:8192};
function makeScope(){return Step.syntheticCapture({R:Number(known.rcR),Cnf:Number(known.rcC),fHz:Number(known.rcFreq),
  amplitudeVpk:Number(known.rcVpk),count:samples.value,rateHz:rate.value,noiseRmsMv:0.2});}
const wave={Channel1:{enable:false,Mode:{text:'Simple'},Simple:{Type:{text:'Sine'},Frequency:{value:100},Amplitude:{value:.5},Offset:{value:0}}},Channel2:{enable:false},
 lastError:'',run(){calls.run++;return true},stop(){calls.stop++;return true},status(){return calls.status()}};
const scope={Time:{Samples:samples,Rate:rate},Channel1:{Range:{value:5},getData(){return makeScope().ch1}},
 Channel2:{Range:{value:5},getData(){return makeScope().ch2}},Trigger:{Trigger:{text:'None'}},lastError:'simulated scope failure',
 single(){calls.scopeSingle++;rate.real=rate.value;return !forcedScopeFailure},wait(){return true},stop(){calls.scopeStop++;return true},captureInfo(){return{sampleRateHz:rate.real}}};
const ctx={Device:{get SN(){return physical?'MOCK-AD3':'DEMO'}},Wavegen1:wave,Scope1:scope,
 modules:{load(name){assert.equal(name,'RCStepAnalysis.mjs');return Step}},
 ui:{get(id){assert.ok(ids.has(id),`get: ${id}`);return known[id]},set(id,v){assert.ok(ids.has(id),`set: ${id}`);
   if(id==='stepNotebook'){assert.ok(Array.isArray(v));assert.ok(v.length<=500);for(const row of v)assert.equal(row.length,12)}
   known[id]=v;return true}},
 plots:{clear(id){assert.ok(ids.has(id),id)},configure(id,o){assert.ok(ids.has(id),id);assert.equal(o.chartType,'XY')},
 setSeries(id,name,x,y){assert.ok(ids.has(id),id);assert.equal(x.length,y.length);assert.ok(x.length>0);calls.plots++}},
 FileIO:{lastError:'',dataPath(n){return '/PanelData/'+n},dataDirectory(){return '/PanelData'},
 writeCsv(n,rows){calls.files[n]=rows;return true},writeJson(n,rows){calls.files[n]=rows;return true}},
 setTimeout(fn,ms){assert.ok(ms>0);calls.timers.push(fn);return calls.timers.length;},
 clearTimeout(){calls.timers=[]},print(...args){calls.logs.push(args.join(' '))}};
vm.createContext(ctx);vm.runInContext(p.script,ctx,{timeout:25000});
for(const c of all)for(const [event,bind] of Object.entries(c.events))assert.equal(typeof ctx[bind.handler],'function',`${c.id}.${event}`);
ctx.onInitialize();assert.equal(calls.run,0);assert.equal(calls.stop,0);assert.match(known.analysisOrigin,/SYNTHETIC MODEL/);
assert.ok(calls.plots>0);assert.equal(ctx.state.lastResult.source,'MODEL');
ctx.onSyntheticClicked();assert.equal(ctx.state.records.length,1);assert.equal(ctx.state.records[0].source,'MODEL');
assert.equal(calls.run,0);assert.equal(calls.scopeSingle,0);
known.rcNoise=5;ctx.onSyntheticSettingsChanged();assert.ok(ctx.state.lastResult.residualRmsV>0.003);
// Physical device must be deliberately selected, wiring verified, and physical R/tau windows met.
known.stepWiringVerified=true;ctx.onCaptureClicked();assert.equal(calls.run,0);assert.match(known.rcStatus,/Select an actual Physical Device/);
physical=true;known.stepWiringVerified=false;ctx.onCaptureClicked();assert.equal(calls.run,0);
known.stepWiringVerified=true;known.rcR=500;ctx.onCaptureClicked();assert.equal(calls.run,0);
known.rcR=980;ctx.onCaptureClicked();assert.equal(calls.run,1);assert.equal(wave.Channel1.Simple.Type.text,'Square');
assert.equal(wave.Channel1.Simple.Offset.value,0);assert.ok(wave.Channel1.Simple.Amplitude.value<=.5);
assert.equal(ctx.state.busy,true);assert.equal(calls.timers.length,1);
calls.timers.shift()();assert.equal(ctx.state.busy,false);assert.equal(ctx.state.energized,false);
assert.equal(calls.scopeSingle,1);assert.ok(calls.stop>=1);assert.equal(ctx.state.records.length,2);
assert.equal(ctx.state.records.at(-1).source,'AD3');assert.ok(Math.abs(ctx.state.records.at(-1).fitTauS-0.00055958)<1e-5);
assert.match(known.analysisOrigin,/PHYSICAL AD3/);assert.match(known.hardwareOutput,/W1 OFF/);
assert.ok(calls.plots>=6);
ctx.onExportClicked();const files=Object.keys(calls.files);assert.ok(files.some(n=>n.endsWith('.json')));
const csv=files.find(n=>n.endsWith('.csv'));
assert.equal(calls.files[csv][0].length,12);assert.equal(calls.files[csv].length,3);
ctx.onExportClicked();assert.equal(Object.keys(calls.files).length,4,'unique file names for a second export');
ctx.onWaveExportClicked();assert.ok(Object.keys(calls.files).some(n=>n.endsWith('_AD3_waveform.csv')));
const actual=Object.keys(calls.files).find(n=>n.endsWith('_AD3_waveform.csv'));
assert.equal(calls.files[actual].length,8193);
const successful=calls.scopeSingle;
ctx.onCaptureClicked();assert.equal(calls.timers.length,1);
ctx.onStopClicked();assert.equal(calls.timers.length,0);assert.equal(ctx.state.energized,false);assert.equal(calls.scopeSingle,successful);
// Wrong firmware waveform readback is a hard preflight error, output is stopped.
let stopBefore=calls.stop;calls.status=()=>({carrierFunction:'Triangle'});
ctx.onCaptureClicked();assert.ok(calls.stop>stopBefore);assert.equal(ctx.state.energized,false);assert.equal(calls.timers.length,0);
calls.status=()=>({carrierFunction:'Square'});
// Scope failures must not append fake AD3 rows.
const prior=ctx.state.records.length;forcedScopeFailure=true;
ctx.onCaptureClicked();calls.timers.shift()();assert.equal(ctx.state.records.length,prior);
assert.equal(ctx.state.energized,false);forcedScopeFailure=false;
// Changing settings must revoke wiring and never enable W1.
known.stepWiringVerified=true;known.rcC=550;ctx.changeConfig();assert.equal(known.stepWiringVerified,false);
assert.equal(ctx.state.records.at(-1).source,'AD3');
ctx.onClearClicked();assert.equal(ctx.state.records.length,0);assert.equal(ctx.state.lastPhysical,null);
ctx.onShowRealClicked();assert.match(known.rcStatus,/No genuine AD3 capture/);
ctx.onShutdown();assert.equal(ctx.state.energized,false);
console.log('PASS RC Panel: 11 Pages, 116 controls, safe startup, deterministic MODEL, physical preflight, square-W1, 2-channel acquired AD3, auto-stop, distinct exports, waveform, readback and error cleanup');
