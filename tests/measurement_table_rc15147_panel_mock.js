// RC1.5.14.7 Node mock for the shipped measurement streaming Panel.
const fs=require('fs'), vm=require('vm'), assert=require('assert');
const path=require('path');
const panel=JSON.parse(fs.readFileSync(path.join(__dirname,'../development/performance/Measurement_100k_Streaming_Lab_RC15147.wfpanel'),'utf8'));
assert.strictEqual(panel.format,'WFScriptPanel');
const measurement=panel.controls.find(c=>c.id==='measurementTable');
assert(measurement&&measurement.type==='measurement_table');
const out=[],ticks=new Map();let nt=1,errors=0;
const ui={
 set(id,payload){
  if(id==='measurementTable'){
    if(payload.reset){out.length=0;return true;}
    if(Array.isArray(payload.append)){
     if(out.length+payload.append.length>100000)return false;
     out.push(...payload.append);return true;
    }
    errors++;return false;
  }
  if(id==='diagnostics'||id==='status')return true;
  errors++;return false;
 }
};
const context={ui,Date,Math,Number,print:()=>{},setInterval(fn){const n=nt++;ticks.set(n,fn);return n;},clearInterval(n){ticks.delete(n);}};
vm.runInNewContext(panel.script,context);
context.onStartup();context.startStreaming();
for(let k=0;k<150;++k){if(!ticks.size)break;for(const f of [...ticks.values()])f();}
assert.strictEqual(out.length,100000);
assert.strictEqual(out[0].name,'Voltage point 0');
assert.strictEqual(out[99999].name,'Voltage point 99999');
assert.strictEqual(ticks.size,0);
context.appendManual();assert.strictEqual(out.length,100000);
context.resetResults();assert.strictEqual(out.length,0);
context.appendManual();assert.strictEqual(out.length,1000);
assert.strictEqual(errors,0);
console.log('PASS RC1.5.14.7 mock: 100k streamed, overflow rejected by Panel, reset, manual 1k, no full snapshots');
