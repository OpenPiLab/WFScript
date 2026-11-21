// RC1.5.14.8: mocks actual saved visual event bindings and 100k streaming.
const fs=require('fs'), vm=require('vm'),assert=require('assert'),path=require('path');
const panel=JSON.parse(fs.readFileSync(path.join(__dirname,'../development/performance/Measurement_100k_Header_Lab_RC15148.wfpanel'),'utf8'));
const control=panel.controls.find(c=>c.id==='measurementTable');
assert(control && control.type==='measurement_table');
assert.strictEqual(control.text,'Measurement history - streaming validation');
assert.strictEqual(control.properties.showToolbar,true);
assert.strictEqual(control.properties.showFilter,true);
assert.strictEqual(control.properties.showClearButton,false);
let data=[],badOps=0,clock=1,timers=new Map();
const ui={set(id,payload){
    if(id==='measurementTable'){
        if(payload.reset){data=[];return true;}
        if(Array.isArray(payload.append) && data.length+payload.append.length<=100000){data.push(...payload.append);return true;}
        badOps++;return false;
    }
    if(id==='diagnostics'||id==='status')return true;
    badOps++;return false;
}};
const ctx={ui,Date,Math,Number,print(){},setInterval(fn){let n=clock++;timers.set(n,fn);return n},clearInterval(id){timers.delete(id)}};
vm.runInNewContext(panel.script,ctx,{timeout:4000});
ctx.onStartup();ctx.startStreaming();
for(let i=0;i<100 && timers.size;++i)for(const fn of [...timers.values()])fn();
assert.strictEqual(data.length,100000);
assert.strictEqual(data.filter(x=>x.value>3.4).length,500);
assert.strictEqual(timers.size,0);
ctx.appendManual();assert.strictEqual(data.length,100000);
ctx.resetResults();assert.strictEqual(data.length,0);
ctx.appendManual();assert.strictEqual(data.length,1000);
assert.strictEqual(badOps,0);
console.log('PASS RC1.5.14.8 panel mock: 100k streamed, 500 FAIL, toolbar definitions, reset and manual append');
