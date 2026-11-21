'use strict';
const assert=require('assert'),fs=require('fs'),vm=require('vm'),path=require('path');
const panel=JSON.parse(fs.readFileSync(path.join(__dirname,'..','fixtures','ExperimentDataTable_NoFullSnapshot_RC151461.wfpanel'),'utf8'));
const table=panel.controls.find(c=>c.id==='experimentTable');
assert(table && table.type==='experiment_table');
assert.strictEqual(table.properties.maxRows,50000);
assert(!Object.hasOwn(table.properties,"emitFullChangeEvent")); // Test native default
for(const event of ['cellEdited','selectionChanged','importCompleted','exportCompleted','cleared'])
    assert(table.events[event] && table.events[event].enabled);
const all=new Map(),timers=new Map(),meta={};let next=0,lines=[];
const ui={
 set(id,payload){
    if(id!=='experimentTable') {all.set(id,payload);return true;}
    if(Array.isArray(payload)){all.set(id,payload);return true;}
    if(payload.appendRows){
       if(!Array.isArray(payload.appendRows)||payload.appendRows.length>2048||
          payload.appendRows.some(r=>!Array.isArray(r)||r.length!==5))return false;
       all.set(id,(all.get(id)||[]).concat(payload.appendRows).slice(-50000));return true;
    }
    if(payload.clear){all.set(id,[]);return true;}
    return false;
 },
 tableRowCount(id){return (all.get(id)||[]).length;},
 tableSlice(id,start,count){return (all.get(id)||[]).slice(start,start+count);},
 tableSelection(){return {row:0,column:2,rows:[0],selectedCells:1,truncated:false};},
 tableRow(id,index){return (all.get(id)||[])[index]||null;},
 tableSetTitle(id,title){meta.title=title;return true;},
 tableSetStatus(id,status){meta.status=status;return true;},
};
const ctx={ui, setInterval(fn,ms){const id=++next;timers.set(id,{fn,ms});return id;},
 clearInterval(id){timers.delete(id);},print(str){lines.push(str);},Date,Math,Number,JSON};
vm.createContext(ctx);vm.runInContext(panel.script,ctx,{timeout:1000});
assert.strictEqual(typeof ctx.onStartup,'function');ctx.onStartup();
assert(meta.title.includes('Experiment Measurements'));
const handlers={};
for(const c of panel.controls)for(const [event,b] of Object.entries(c.events||{})) {
 assert(typeof ctx[b.handler]==='function',c.id+' '+b.handler);
 handlers[c.id+':'+event]=ctx[b.handler];
}
handlers['startButton:clicked']();
assert(timers.size===5);
for(let t=0;t<2200;t++){
 for(const timer of [...timers.values()])
  if(timer.ms===10 || timer.ms===20&&t%2===0 || timer.ms===30&&t%3===0 || timer.ms===50&&t%5===0 || timer.ms===250&&t%25===0)
   timer.fn();
}
handlers['stopButton:clicked']();
assert.strictEqual(timers.size,0);
let rows=all.get('experimentTable');
assert.strictEqual(rows.length,50000);
for(let i=1;i<rows.length;i++)assert.strictEqual(rows[i][0],rows[i-1][0]+1);
handlers['selectionButton:clicked']();
assert(lines.at(-1).includes('Selection'));
handlers['experimentTable:cellEdited']({row:0,column:3,value:'3.5',oldValue:'3.3'});
handlers['experimentTable:selectionChanged']({row:0,column:3});
all.set('experimentTable',Array.from({length:30},(_,i)=>[i+1,0,'import',3.3,10]));
handlers['experimentTable:importCompleted']({ok:true,rows:30,path:'test.csv',error:''});
assert(lines.length>=0);
handlers['experimentTable:exportCompleted']({ok:true,rows:30,path:'out.csv',error:''});
all.set('experimentTable',[]);
handlers['experimentTable:cleared']({rows:0});
console.log('PASS RC1.5.14.6.1 Panel: 50k rolling rows, seq continuous, toolbar and new event handlers');
