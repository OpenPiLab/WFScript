'use strict';
// JS mock of the NEW panel's public API. Not Qt or actual hardware validation.
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const root = path.join(__dirname, '..');
const panel = JSON.parse(fs.readFileSync(path.join(root,'examples/panels/TableLimitPlot.wfpanel'),'utf8'));
const ids = new Map();
for (const page of panel.pages) for (const widget of page.controls) ids.set(widget.id,widget);
const state = new Map();
for (const [id,widget] of ids) {
  if(widget.type==='experiment_table') {
    const rows = (widget.properties.initialRows || '').split('\n').filter(x=>x.length>0)
      .map(line=>line.split('|'));
    state.set(id,rows);
  } else state.set(id,widget.value || 0);
}
const events = new Map();
let series = new Map(), configureCalls = 0, plotClears = 0;
const logs = [];
const context={
  ui:{
    get(id){assert(ids.has(id),id); return state.get(id);},
    set(id,value){assert(ids.has(id),id);state.set(id,value);},
    on(id,event,handler){assert(ids.has(id),id); assert.equal(typeof handler,'function');events.set(id+':'+event,handler);}
  },
  plots:{
    clear(id){assert.equal(id,'limitCurve');series.clear();plotClears++;},
    configure(id,o){assert.equal(id,'limitCurve');assert.equal(o.chartType,'XY'); configureCalls++;},
    setSeries(id,name,x,y,style){assert.equal(id,'limitCurve');assert(Array.isArray(x) && Array.isArray(y));assert.equal(x.length,y.length);
      series.set(name,{x:x.slice(),y:y.slice(),style});return true;}
  },
  print(message){logs.push(String(message));},
};
vm.createContext(context);
vm.runInContext(panel.script,context,{timeout:2500,filename:'TableLimitPlot.js'});
assert.equal(state.get('pointCount'),6);
assert.equal(state.get('outsideCount'),1);
assert.equal(Number(state.get('largestDeviation').toFixed(3)),0.150);
assert.equal(state.get('limitResults').length,6);
assert.equal(series.size,5); // 4 curves plus out-of-limit markers
assert.equal(series.get('Out-of-limit points').x[0],400);
assert.equal(series.get('Out-of-limit points').y[0],1.65);
assert.equal(state.get('limitResults')[3][6],'OUTSIDE');
assert.equal(state.get('limitInputTable').length,6);

// A normal edit, then refresh. Confirm stale results are cleared immediately.
let rows=state.get('limitInputTable').map(row=>row.slice());
rows[3][2]='1.75';
state.set('limitInputTable',rows);
events.get('limitInputTable:valueChanged')(rows);
assert.equal(state.get('limitResults').length,0);
assert.equal(series.size,0);
events.get('plotEnteredData:clicked')();
assert.equal(state.get('outsideCount'),0);
assert.equal(series.size,4);
assert.equal(state.get('limitResults')[3][6],'IN LIMITS');

// Unsorted input should sort the PLOT COPY, not reorder the user's rows.
rows=state.get('limitInputTable').map(row=>row.slice());
rows.reverse();state.set('limitInputTable',rows);
events.get('limitInputTable:valueChanged')(rows);
events.get('plotEnteredData:clicked')();
assert.equal(series.get('Nominal').x[0],100);
assert.equal(state.get('limitInputTable')[0][0],'600');

// Duplicate X, partial input, and inverted bounds must reject cleanly.
function rejected(mutator,pattern){
 const v=state.get('limitInputTable').map(r=>r.slice());mutator(v);
 state.set('limitInputTable',v);events.get('limitInputTable:valueChanged')(v);
 events.get('plotEnteredData:clicked')();
 assert(state.get('plotStatus').includes('ERROR:'));
 assert(pattern.test(state.get('plotStatus')),state.get('plotStatus'));
 assert.equal(series.size,0);
 assert.deepEqual(state.get('limitResults'),[]);
}
rejected(v=>{v[1][0]=v[0][0]},/Duplicate X/);
// Restart from default using RESTORE; then test other invalid inputs.
events.get('restoreLimitData:clicked')();
rejected(v=>{v[2][4]=''},/Row 3/);
events.get('restoreLimitData:clicked')();
rejected(v=>{v[0][3]='1.20'},/lower limit is greater/);
events.get('restoreLimitData:clicked')();
assert.equal(state.get('outsideCount'),1);

// Entirely blank additional row is accepted and ignored.
rows=state.get('limitInputTable').map(r=>r.slice());rows.push(['','','','','']);
state.set('limitInputTable',rows);events.get('plotEnteredData:clicked')();
assert.equal(state.get('pointCount'),6);

// CLEAR only affects results, preserving the operator's source data.
events.get('clearLimitResults:clicked')();
assert.equal(state.get('pointCount'),0);
assert.equal(state.get('limitResults').length,0);
assert.equal(state.get('limitInputTable').length,7);
assert(configureCalls>=5 && plotClears>=6);
assert(logs.every(m=>m.includes('[Table-to-Plot]')));
console.log('PASS: TableLimitPlot native JS array workflow, 4/5 XY series, limits, edit, sort, invalid inputs, restore/clear; hardware API unused');
