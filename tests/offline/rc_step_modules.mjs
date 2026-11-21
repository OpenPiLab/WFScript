import assert from 'node:assert/strict';
import * as Step from '../../examples/libraries/RCStepAnalysis.mjs';
const tau=980*571e-9;
let p=Step.theory(980,571,100);
assert.ok(Math.abs(p.tau-tau)<1e-14);
assert.ok(Math.abs(p.fc-284.418)<0.02,p.fc);
assert.ok(Math.abs(p.halfPeriodsPerTau-8.935)<0.02);
const sample=Step.syntheticCapture({R:980,Cnf:571,fHz:100,noiseRmsMv:0});
const r=Step.analyzeCapture(sample);
assert.equal(r.source,'MODEL');
assert.equal(r.count,8192);
assert.ok(r.transitions.length>=4 && r.edgeCount>=5);
assert.ok(r.transitions.some(t=>t.direction==='RISE'));
assert.ok(r.transitions.some(t=>t.direction==='FALL'));
assert.ok(Math.abs(r.fitTauS-tau)<2e-8,r.fitTauS);
assert.ok(Math.abs(r.riseTauS-r.fallTauS)<2e-8);
assert.ok(Math.abs(r.tau63S-tau)<2e-6);
assert.ok(Math.abs(r.fitFcHz-p.fc)<0.02);
assert.ok(r.residualRmsV<1e-8);
for(const n of [0,1,3,10]) {
 const s=Step.syntheticCapture({R:980,Cnf:571,fHz:100,noiseRmsMv:n});
 const result=Step.analyzeCapture(s);
 assert.ok(Math.abs(result.errorPct)<2,'fit should remain near true tau at noise '+n);
 assert.ok(n===0?result.residualRmsV<1e-8:result.residualRmsV>n*0.0007);
}
for(const hz of [60,75,125,150]){
 const result=Step.analyzeCapture(Step.syntheticCapture({R:980,Cnf:571,fHz:hz,noiseRmsMv:0.5}));
 assert.ok(Math.abs(result.errorPct)<1,`freq ${hz}: ${result.errorPct}`);
}
for(const val of [{R:1500,Cnf:680,fHz:60},{R:1000,Cnf:100,fHz:100},{R:900,Cnf:800,fHz:60}]){
 const result=Step.analyzeCapture(Step.syntheticCapture({...val,noiseRmsMv:0.3}));
 assert.ok(Math.abs(result.errorPct)<3,`RC case ${JSON.stringify(val)}: ${result.errorPct}`);
}
// Fitted step model is constructed from actual waveform voltages, not CH1 sine coefficients.
const first=r.transitions[0];
let rms=0;
for(let i=first.first;i<first.last;i++)rms+=(sample.ch2[i]-Step.predict(first,i,sample.rateHz))**2;
assert.ok(Math.sqrt(rms/(first.last-first.first))<1e-8);
// Fundamental physical and sampling gates must reject bad acquisitions.
assert.throws(()=>Step.analyzeCapture({...sample,ch1:Array(8192).fill(0)}),/square-wave swing/);
assert.throws(()=>Step.analyzeCapture({...sample,ch2:Array(8192).fill(0)}),/amplitude too small/);
assert.throws(()=>Step.analyzeCapture({...sample,rateHz:NaN}),/sample rate/);
assert.throws(()=>Step.analyzeCapture({...sample,ch2:[...sample.ch2,1]}),/equal-length/);
assert.throws(()=>Step.analyzeCapture({...sample,ch1:[...sample.ch1.slice(0,450),NaN,...sample.ch1.slice(451)]}),/non-finite/);
assert.throws(()=>Step.theory(100,0,100),/R=100/);
console.log('PASS RC modules: 63.2% crossing, rise/fall fitting, exact/noisy/short/long RC, samples/edges/gates, no external math library');
