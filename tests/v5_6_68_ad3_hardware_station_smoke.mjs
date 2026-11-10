// Offline integration rehearsal. Mock the existing WFScript Wavegen/Scope JS
// bridges; the Windows AD3 hardware run remains the decisive validation.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/AD3HardwareTestStation.wfpanel'),'utf8'));
const libFile=path.join(root,'examples/libraries/AD3LoopbackSteps.mjs');
const source=fs.readFileSync(libFile,'utf8');
assert.equal(panel.libraries.length,1);
assert.equal(panel.libraries[0].name,panel.testLibrary);
assert.equal(panel.libraries[0].source,source);
assert.equal(panel.script,fs.readFileSync(path.join(root,'examples/source/AD3HardwareTestStation.js'),'utf8'));
assert.equal(panel.reportStorage.autoSave,true);
assert.equal(panel.testDefinition.config.stopOnFailure,true);
assert.equal(panel.testDefinition.config.sequenceId,'stationSequence');
assert.equal(panel.pages.length,6);
const controls=panel.pages.flatMap(p=>p.controls);
assert.equal(controls.length,new Set(controls.map(c=>c.id)).size);
const types=new Map(controls.map(c=>[c.id,c.type]));
for(const s of panel.testDefinition.steps) {
  if(s.resultWidget)assert.equal(types.get(s.resultWidget),'measurement_result');
  if(s.handler)assert.match(source,new RegExp(`export function ${s.handler}\\(`));
}
const numeric=panel.testDefinition.steps.filter(s=>s.type==='numeric');
assert.deepEqual(numeric.map(s=>s.id),['acRms','frequency','peakToPeak','dcOffset']);
for(const s of numeric)assert.ok(s.limits.low<s.limits.high);
assert.equal(numeric.length,4);
assert.equal(panel.testDefinition.steps.at(-1).group,'cleanup');
assert.ok(!/TestRunner\.(configure|loadDefinition|add|clear|setRunInfo)\s*\(/.test(panel.script));

let running=false,captures=0,scopestop=0,wavestop=0,throwCapture=false,graph=0;
const wave={enable:false,Mode:{text:''},Simple:{Type:{text:''},Frequency:{value:0},Amplitude:{value:0},Offset:{value:0},Phase:{value:0}}};
globalThis.Device={isConnected:()=>true};
globalThis.Wavegen1={Channel1:wave,Channel2:{enable:false},lastError:'',stop(){running=false;wavestop++},run(){running=true;return true},status(){return {carrierFunction:'Sine'}}};
globalThis.Scope1={
 Time:{Rate:{value:0},Samples:{value:0}},Channel1:{Range:{value:0},Offset:{value:0},Attenuation:{value:0},statistics(){return {count:16384,rms:Math.SQRT1_2,mean:0,frequencyHz:999.82,vpp:1.999}}},
 Trigger:{Trigger:{text:''}},lastError:'',single(){assert.ok(running);return true},wait(){captures++;return !throwCapture},stop(){scopestop++}
};
globalThis.plots={fromScope(){graph++}};
globalThis.print=()=>{};
const steps=await import('../examples/libraries/AD3LoopbackSteps.mjs');
// One deterministic passing simulated hardware frame, one capture only.
assert.equal(steps.configureLoopback(),true);
assert.equal(wave.Simple.Frequency.value,1000);
assert.equal(wave.Simple.Amplitude.value,1);
assert.equal(globalThis.Scope1.Time.Samples.value,16384);
const results={acRms:steps.captureAcRms(),frequency:steps.measureFrequency(),peakToPeak:steps.measureVpp(),dcOffset:steps.measureMean()};
for(const test of numeric){assert.ok(results[test.id]>=test.limits.low && results[test.id]<=test.limits.high,test.id);}
assert.equal(captures,1);assert.equal(graph,1);
assert.equal(steps.cleanupLoopback(),true);assert.equal(running,false);
// Deliberately fail a numeric limit but still invoke Cleanup (runner's
// separate native path handles this in the real application).
assert.equal(steps.configureLoopback(),true);
const broken=0.5;
assert.ok(broken<numeric[0].limits.low);
assert.equal(steps.cleanupLoopback(),true);assert.equal(running,false);
// Acquisition error, even when thrown, does not inhibit cleanup.
assert.equal(steps.configureLoopback(),true);
throwCapture=true;
assert.throws(()=>steps.captureAcRms(),/Scope acquisition failed/);
assert.equal(steps.cleanupLoopback(),true);assert.equal(running,false);
assert.ok(wavestop>=6);assert.equal(scopestop,3);
console.log('PASS v5.6.68: example schema, embedded library, native numeric limits, single-capture mock, failure/cleanup simulation');
