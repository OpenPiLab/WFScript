import fs from 'node:fs';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import * as Circuit from '../../examples/libraries/CircuitLab.mjs';
import * as Capture from '../../examples/libraries/CaptureAnalysis.mjs';
import * as Archive from '../../examples/libraries/SignalPackage.mjs';
const P=JSON.parse(fs.readFileSync(new URL('../../examples/panels/ACNetworkLaboratory.wfpanel',import.meta.url)));
assert.equal(P.pages.length,10,'three teaching Pages appended');
assert.equal(P.pages.flatMap(p=>p.controls).length,131);
const notebook=P.pages.flatMap(p=>p.controls).find(c=>c.id==='resultsTable');
assert.ok(notebook && notebook.type==='experiment_table');
assert.equal(notebook.properties.columns.split('|').length,11);
assert.equal(notebook.properties.maxRows,500);
const ids=new Set();for(const page of P.pages) for(const c of page.controls){assert.ok(!ids.has(c.id),'duplicate '+c.id);ids.add(c.id);}
const known={...Object.fromEntries(P.pages.flatMap(p=>p.controls.map(c=>[c.id,c.type==='option_selector'?(c.properties.options.split('|')[c.properties.defaultIndex]):c.value])))};
const snapshots={...known};
let calls={run:0,stop:0,scope:0,configure:[],plots:0,files:{},diag:[],timerQueue:[],last:false};
const wave={
    Channel1:{enable:false,Mode:{text:'Simple'},Simple:{Type:{text:'Sine'},Frequency:{value:1000},Amplitude:{value:1},Offset:{value:0}}},
    Channel2:{enable:false},lastError:'',run(){calls.run++;return true},stop(){calls.stop++;return true},status(){return{carrierFunction:'Sine'}}};
let rate={value:20000,real:20000},samples={value:4096};
const ch1={Range:{value:5},getData(){const f=wave.Channel1.Simple.Frequency.value;return Array.from({length:samples.value},(_,i)=>0.99*Math.sin(2*Math.PI*f*i/rate.value+0.27));}};
const ch2={Range:{value:5},getData(){const f=wave.Channel1.Simple.Frequency.value;
      const kind=known.topology,R=known.rOhm,Cnf=known.cNf;
      const h=Circuit.transfer(kind,R,Cnf,f);
      return Array.from({length:samples.value},(_,i)=>0.99*h.gain*Math.sin(2*Math.PI*f*i/rate.value+0.27+h.phaseDeg*Math.PI/180));}};
const scope={Time:{Samples:samples,Rate:rate},Channel1:ch1,Channel2:ch2,Trigger:{Trigger:{text:'None'}},lastError:'',single(){calls.scope++;rate.real=rate.value;return true},wait(){return true},stop(){return true},captureInfo(){return {sampleRateHz:rate.real}}};
let isPhysical=false;
const ctx={
    Device:{get SN(){return isPhysical?'AD3-TEST':'DEMO'}},
    Wavegen1:wave,Scope1:scope,
    ui:{get(id){assert.ok(id in known,id);return known[id];},set(id,v){assert.ok(id in known,id);
        if(id==='resultsTable'){assert.ok(Array.isArray(v));assert.ok(v.length<=notebook.properties.maxRows);
          for(const row of v)assert.equal(row.length,11,'Notebook runtime cell count must match column headers');}
        known[id]=v;return true;}},
    plots:{clear(id){assert.ok(ids.has(id));},configure(id,opts){assert.ok(opts.chartType==='XY');if(id.includes('Gain')||id.includes('Phase'))assert.equal(opts.xLog,true);},setSeries(id,name,x,y){assert.ok(ids.has(id),id);assert.equal(x.length,y.length);assert.ok(x.length>0);calls.plots++;return true;}},
    modules:{load(name){return({'CircuitLab.mjs':Circuit,'CaptureAnalysis.mjs':Capture,'SignalPackage.mjs':Archive})[name]||(()=>{throw Error('unknown '+name)})()}},
    FileIO:{lastError:'',dataDirectory(){return '/LabData'},dataPath(name){return '/LabData/'+name;},writeCsv(path,rows){calls.files[path]=rows;return true},writeJson(path,val){calls.files[path]=val;return true}},
    setTimeout(fn,ms){assert.ok(ms>=0);calls.timerQueue.push(fn);return calls.timerQueue.length;},clearTimeout(id){calls.timerQueue=[];},
    print(...args){calls.diag.push(args.join(' '));},
};
vm.createContext(ctx);vm.runInContext(P.script,ctx,{timeout:25000,filename:'ACNetworkLaboratory.js'});
function flushTimers(max=100){for(let i=0;calls.timerQueue.length;i++){assert.ok(i<max,'unbounded timers');let fn=calls.timerQueue.shift();fn();}}
assert.equal(calls.run,0,'no output at startup');assert.equal(calls.stop,0,'not even W1.stop at startup');
// Startup hooks are explicit project options.
ctx.onInitialize();assert.ok(known.statCutoff.includes('1591.5'));assert.match(known.labStatus,/READY/);assert.equal(calls.run,0);
assert.match(known.fitSource,/SYNTHETIC/);assert.match(known.fitComparison,/FITTED GAIN/);
assert.match(known.fitQualityIn,/RMS residual/);
// Noiseless fractional-cycle synthetic fit should have nearly zero residual.
known.fitDemoNoise=0;known.fitDemoHarmonic=0;ctx.onFitDemoChanged();
assert.ok(Number(known.fitQualityIn.match(/RMS residual ([0-9.]+) mV/)[1])<0.001);
known.fitDemoNoise=8;known.fitDemoHarmonic=4;ctx.onFitDemoChanged();
assert.ok(Number(known.fitQualityIn.match(/RMS residual ([0-9.]+) mV/)[1])>10);
assert.match(known.fitInterpretation,/SYNTHETIC|generated values/);
ctx.onModelSweepClicked();assert.equal(ctx.state.rows.length,9);assert.equal(ctx.state.rows[0].source,'MODEL');
assert.equal(calls.run,0);
// No W1 run when Simulation selected.
known.wiringVerified=true;ctx.onArmClicked();assert.equal(calls.run,0);assert.match(known.labStatus,/ERROR/);
// Physical operation requires verified toggle.
isPhysical=true;known.wiringVerified=false;ctx.onArmClicked();assert.equal(calls.run,0);assert.match(known.labStatus,/ERROR/);
// A measured nominal 1 kOhm resistor at 980 ohms is accepted; much lower values are rejected.
known.rOhm=800;known.wiringVerified=true;ctx.onArmClicked();assert.equal(calls.run,0);
known.rOhm=980;known.wiringVerified=true;ctx.onArmClicked();assert.equal(calls.run,1);
known.rOhm=1000; // restore nominal model without invoking change handler in the mock
ctx.state.lastPhysicalR=1000;assert.equal(ctx.state.armed,true);
ctx.onCaptureClicked();assert.equal(ctx.state.busy,true);flushTimers();
assert.equal(ctx.state.rows.at(-1).source,'AD3');assert.ok(ctx.state.rows.at(-1).gain>0.5);
assert.ok(Math.abs(ctx.state.rows.at(-1).phaseDeg+45.15)<0.3);
assert.equal(ctx.state.rows.at(-1).rateHz,rate.value);
assert.ok(ctx.state.rows.at(-1).fitInRmsV<1e-10);
const countsBeforeFit={run:calls.run,scope:calls.scope};
ctx.onFitPhysicalClicked();assert.match(known.fitSource,/PHYSICAL AD3/);
assert.ok(Number(known.fitQualityIn.match(/RMS residual ([0-9.]+) mV/)[1])<0.001);
assert.equal(calls.run,countsBeforeFit.run);assert.equal(calls.scope,countsBeforeFit.scope);
// A revoked wiring verification must promptly release hardware, before next capture.
const priorStop=calls.stop;
known.wiringVerified=false;ctx.onWiringVerificationChanged();
assert.equal(ctx.state.armed,false);assert.ok(calls.stop>priorStop);
known.wiringVerified=true;ctx.onArmClicked();assert.equal(ctx.state.armed,true);
// Under a physical sweep W1 stops automatically after final measurement.
ctx.onHardwareSweepClicked();flushTimers(100);assert.equal(ctx.state.armed,false);assert.equal(ctx.state.sweep,false);
assert.ok(ctx.state.rows.filter(r=>r.source==='AD3').length>=5);
assert.ok(calls.stop>=1);
// Even if W1 successfully starts but the hardware status query fails before
// state.armed is true, the partial ARM must stop W1.
const savedStatus=wave.status;
wave.status=()=>{throw Error('simulated readback failure');};
const stopBeforeReadback=calls.stop;
ctx.onArmClicked();assert.equal(ctx.state.armed,false);
assert.ok(calls.stop>stopBeforeReadback,'status failure must stop W1');
wave.status=savedStatus;
ctx.onExportClicked();assert.ok(calls.files['/LabData/ac_network_lab_results.csv']);assert.ok(calls.files['/LabData/ac_network_lab_report.json']);
assert.equal(calls.files['/LabData/ac_network_lab_results.csv'][0].length,16,'CSV adds fitting diagnostics');
assert.ok(calls.files['/LabData/ac_network_lab_report.json'].measurements.some(r=>r.fitInRmsV!==undefined));
ctx.onExportWaveClicked();assert.equal(calls.files['/LabData/ac_network_last_ad3_waveform.csv'].length,4097);
ctx.onPackClicked();assert.match(known.packedResult,/lossless PASS/i);assert.ok(calls.files['/LabData/ac_network_last_ad3_capture_packed.json']);
// Clearing must discard prior physical captures and explicitly leave real fit view.
ctx.onClearLogClicked();assert.equal(ctx.state.lastCapture,null);
assert.match(known.fitSource,/SYNTHETIC/);
// Changing topology releases verified flag and requires verifying the new layout.
known.wiringVerified=true;known.topology='High-pass';ctx.onModelParametersChanged();assert.equal(known.wiringVerified,false);
known.wiringVerified=true;ctx.onArmClicked();ctx.onCaptureClicked();flushTimers();
assert.ok(Math.abs(ctx.state.rows.at(-1).phaseDeg-44.85)<0.5);
// Simulated acquisition failure forces Wavegen output off and keeps false measurements out of log.
let before=ctx.state.rows.length;scope.single=()=>false;ctx.onCaptureClicked();flushTimers();
assert.equal(ctx.state.armed,false);assert.equal(ctx.state.rows.length,before);
ctx.onShutdown();assert.equal(ctx.state.armed,false);
console.log('PASS AC Lab panel: '+P.pages.length+' pages, '+ids.size+' controls, synthetic/real fit/residual plots, startup safe, model-only data, preflight gate, live LP/HP captured phasors, sweep auto-stop, export, compression, cleanup');
