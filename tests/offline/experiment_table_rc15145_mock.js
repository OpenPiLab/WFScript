'use strict';
const assert=require('assert'), fs=require('fs'), vm=require('vm'),path=require('path');
const file=path.join(__dirname,'..','fixtures','ExperimentDataTable_Streaming_50k_RC15145.wfpanel');
const panel=JSON.parse(fs.readFileSync(file,'utf8'));
assert.equal(panel.format,'WFScriptPanel');
const table=panel.controls.find(c=>c.id==='experimentTable');
assert(table && table.type==='experiment_table' && table.properties.maxRows===50000);
assert.equal(table.properties.columns.split('|').length,5);
const state=new Map(),timers=new Map();let tid=0;
const context={
 ui:{
   set(id,payload){
     if(id==='experimentTable') {
       let rows=state.get(id)||[];
       if(Array.isArray(payload))rows=payload;
       else if(payload.clear)rows=[];
       else if(payload.appendRows){
         if(!Array.isArray(payload.appendRows) || payload.appendRows.length>2048 ||
            payload.appendRows.some(r=>!Array.isArray(r)||r.length!==5))return false;
         rows=rows.concat(payload.appendRows).slice(-50000);
       } else return false;
       state.set(id,rows);return true;
     }
     if(id==='diagnostics') {state.set(id,[]);return true;}
     state.set(id,payload);return true;
   },
   tableRowCount(id){return state.has(id)?state.get(id).length:0;},
   tableSlice(id,start,count){return (state.get(id)||[]).slice(start,start+count);}
 },
 setInterval(fn,ms){let id=++tid;timers.set(id,{fn,ms});return id;},
 clearInterval(id){timers.delete(id);},
 print(){},Date,Math,Number
};
vm.createContext(context);
vm.runInContext(panel.script,context,{timeout:1000});
assert.equal(typeof context.onStartup,'function');
context.onStartup();
const handlers=new Map();
for(const c of panel.controls)for(const [name,e] of Object.entries(c.events||{})){
 assert(e.enabled && typeof context[e.handler]==='function');
 handlers.set(c.id+':'+name,context[e.handler]);
}
assert(handlers.get('startButton:clicked'));
assert(handlers.get('stopButton:clicked'));
handlers.get('startButton:clicked')();
assert.equal(timers.size,5);
for(let i=0;i<1800;++i){
 // 1800 synthetic cycles generate >50k rows and exercise rolling retention.
 for(const item of [...timers.values()]){
    if(item.ms===10 || (item.ms===20 && i%2===0) ||
       (item.ms===30 && i%3===0) || item.ms===50 && i%5===0 || item.ms===250 && i%25===0)
         item.fn();
 }
}
handlers.get('sampleButton:clicked')();
handlers.get('stopButton:clicked')();
assert.equal(timers.size,0);
let rows=state.get('experimentTable');
assert(rows.length===50000 && rows.every(r=>r.length===5));
for(let i=1;i<rows.length;i++)assert.equal(rows[i][0],rows[i-1][0]+1);
handlers.get('clearButton:clicked')();
assert.equal(state.get('experimentTable').length,0);
handlers.get('sampleButton:clicked')();
assert.equal(state.get('experimentTable').length,1000);
console.log('PASS RC1.5.14.5 Panel JS: 10/20/30ms simulated callbacks, batch append, sequence continuity, clear and manual injection');
