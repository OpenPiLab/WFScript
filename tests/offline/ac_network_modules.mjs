import assert from 'node:assert/strict';
import * as C from '../../examples/libraries/CircuitLab.mjs';
import * as A from '../../examples/libraries/CaptureAnalysis.mjs';
import * as Package from '../../examples/libraries/SignalPackage.mjs';
for(const topology of ['Low-pass','High-pass']){
 const fc=C.parameters(1000,100).fc;
 const t=C.transfer(topology,1000,100,fc);
 assert.ok(Math.abs(t.gain-Math.SQRT1_2)<1e-12);
 assert.ok(Math.abs(t.phaseDeg-(topology==='Low-pass'?-45:45))<1e-10);
 let n=4096,fs=65536,f=1024;
 let phase=t.phaseDeg*Math.PI/180;
 let inData=Array.from({length:n},(_,i)=>Math.sin(2*Math.PI*f*i/fs+0.2));
 let outData=Array.from({length:n},(_,i)=>t.gain*Math.sin(2*Math.PI*f*i/fs+0.2+phase));
 let analysis=A.analyzeTwoChannel(inData,outData,fs,f);
 assert.ok(Math.abs(analysis.gain-t.gain)<1e-10,JSON.stringify(analysis));
 assert.ok(Math.abs(analysis.phaseDeg-t.phaseDeg)<1e-9);
 console.log('PASS',topology,'at fc',fc.toFixed(3),t.db.toFixed(4),t.phaseDeg);
}
const grid=C.makeBode('Low-pass',1000,100,80,20000,200);
assert.equal(grid.hz.length,200);assert.ok(grid.db[0]>-1);assert.ok(grid.db[199]<-20);
assert.equal(C.nominalSampleRatio(32000,1000),'32');
const original={ch1:[0,.2,.3,-.6],ch2:[0,.01,.03,-.1],fHz:500};
const packed=Package.packCapture(original);const restored=Package.unpackCapture(packed.encoded);
assert.deepEqual(restored,original);console.log('PASS theory, phase-fit and vendor-pako round-trip');
// v0.2-dev.8.9.2 educational diagnostics: fractional-cycle capture, DC offset,
// deterministic noise/harmonics, reconstruction and physical-model residuals.
{
  const fs=50000,f=1367.3,n=1027,phi=-0.71,offset=0.21,amplitude=0.73;
  const data=Array.from({length:n},(_,i)=>offset+amplitude*Math.sin(2*Math.PI*f*i/fs+phi));
  const fit=A.fitSinusoid(data,fs,f);
  assert.ok(Math.abs(fit.offset-offset)<1e-11);
  assert.ok(Math.abs(fit.amplitude-amplitude)<1e-11);
  assert.ok(Math.abs(fit.angle-phi)<1e-11);
  assert.ok(fit.rmsResidualV<1e-11,`noiseless ${fit.rmsResidualV}`);
  assert.ok(Math.abs(fit.rSquared-1)<1e-12);
  assert.ok(Math.abs(A.predictSample(fit,fs,f,157)-data[157])<1e-11);
  const noisy=data.map((v,i)=>v+0.03*Math.sin(2*Math.PI*3*f*i/fs+0.19));
  const noisyFit=A.fitSinusoid(noisy,fs,f);
  assert.ok(noisyFit.rmsResidualV>0.019 && noisyFit.rmsResidualV<0.023,'third-harmonic residual');
  assert.ok(noisyFit.rSquared<1 && noisyFit.residualPct>2);
  assert.throws(()=>A.fitSinusoid(data,2*f,f),/Invalid/);
  const two=A.analyzeTwoChannel(data,data.map(v=>v*0.6),fs,f);
  assert.ok(Math.abs(two.gain-0.6)<1e-11 && Math.abs(two.phaseDeg)<1e-9);
  assert.ok(two.inputFit && two.outputFit && two.inputFit.samples===n);
  console.log('PASS educational least-squares diagnostics: fractional cycles, DC, residual, prediction, gain and phase');
}
{
  // Physical signal thresholds remain enforced; synthetic studies may explore
  // deeply attenuated signals without misrepresenting them as physical captures.
  const n=1024,fs=65536,f=1024;
  const weak=Array.from({length:n},(_,i)=>1e-4*Math.sin(2*Math.PI*f*i/fs));
  const vin=weak.map(x=>x*10000);
  assert.throws(()=>A.analyzeTwoChannel(vin,weak,fs,f),/floor/);
  const simulated=A.analyzeTwoChannel(vin,weak,fs,f,{synthetic:true});
  assert.ok(Math.abs(simulated.gain-0.0001)<1e-12);
  assert.throws(()=>A.analyzeTwoChannel(weak,vin,fs,f),/input too small/);
  console.log('PASS physical measurement gates preserved; explicit synthetic low-level exploration');
}
