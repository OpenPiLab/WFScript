// RC1.5.14.9: hardware-free smoke of delivered Panel script and saved UI handlers.
const fs=require('fs'),vm=require('vm'),path=require('path'),assert=require('assert');
const filename=process.argv[2]||path.resolve(__dirname,'../examples/panels/ThemeLifecycleRC15149.wfpanel');
const panel=JSON.parse(fs.readFileSync(filename,'utf8'));
assert.strictEqual(panel.version,2);
assert.strictEqual(panel.appearance.themeMode,'follow');
assert.strictEqual(panel.pages.length,2);
const ids=new Set(panel.pages.flatMap(p=>p.controls.map(c=>c.id)));
assert.strictEqual(ids.size,panel.pages.flatMap(p=>p.controls).length);
for(const typ of ['measurement_table','experiment_table','event_log','toggle','test_sequence']) {
 if(typ==='test_sequence')continue; // native smoke checks Test Sequence separately
 assert(panel.pages[0].controls.some(c=>c.type===typ));
}
const values={experimentTable:[],measurementTable:[],diagnostics:[],status:'',themeCheck:false};
const context={
 Date,Math,print:()=>{},
 ui:{tableRowCount:(id)=>(values[id]||[]).length,set:(id,v)=>{
   if(id==='status'){values[id]=v;return;}
   if(v&&v.reset===true || v&&v.clear===true){values[id]=[];return;}
   if(v&&Object.prototype.hasOwnProperty.call(v,'append'))values[id].push(...(Array.isArray(v.append)?v.append:[v.append]));
   if(v&&Object.prototype.hasOwnProperty.call(v,'appendRows'))values[id].push(...v.appendRows);
 }}
};
vm.createContext(context);
vm.runInContext(panel.script,context,{filename:'ThemeLifecycle_RC15149.js'});
for(let k=0;k<150;k++)context.fillTables();
assert.strictEqual(values.measurementTable.length,30000);
assert.strictEqual(values.experimentTable.length,30000);
assert(values.experimentTable.every(row=>row.length===5));
assert.strictEqual(values.diagnostics.length,150);
context.verifyCounts();assert(values.status.includes('30000'));
context.clearTables();assert.strictEqual(values.experimentTable.length,0);
assert.strictEqual(values.measurementTable.length,0);
assert.strictEqual(values.diagnostics.length,151);
for(const c of panel.pages[0].controls) for(const ev of Object.values(c.events||{})) {
 if(ev.enabled)assert.strictEqual(typeof context[ev.handler],'function',`Missing handler ${ev.handler}`);
}
console.log('PASS RC1.5.14.9 Panel script 30k rows, 5-column batches, clear, saved handlers');
