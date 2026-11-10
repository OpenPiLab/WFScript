'use strict';
// Exercising the bundled JavaScript with array-exact mock ui.get()/ui.set().
const fs=require('fs'), path=require('path'), vm=require('vm'), assert=require('assert');
const root=path.resolve(__dirname,'..');
const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/CalibrationTable.wfpanel')));
const source=fs.readFileSync(path.join(root,'examples/source/CalibrationTable.js'),'utf8');
assert.equal(panel.script,source);
const widgets=Object.fromEntries(panel.pages.flatMap(p=>p.controls.map(w=>[w.id,w])));
function init(){
 const values={};const events={};const calls={sets:[],plots:[],clears:[],hardware:0,logs:[]};
 for(const [id,c] of Object.entries(widgets)){
   values[id]=c.type==='experiment_table' ? (c.properties.initialRows||'').split('\n').filter(Boolean).map(s=>s.split('|')) : c.value;
 }
 const ui={
    get(id){assert(widgets[id],`get unknown ${id}`);return values[id];},
    set(id,val){assert(widgets[id],`set unknown ${id}`);values[id]=val;calls.sets.push([id,val]);return true;},
    on(id,event,cb){assert(widgets[id],`on unknown ${id}`);events[id+'.'+event]=cb;}
 };
 const plots={
   clear(id){assert.equal(id,'calibrationPlot');calls.clears.push(id);return true;},
   configure(id,opts){assert.equal(id,'calibrationPlot');calls.plots.push(['configure',opts]);return true;},
   setSeries(id,name,x,y,style){assert.equal(id,'calibrationPlot');assert.equal(x.length,y.length);assert(x.every(Number.isFinite));assert(y.every(Number.isFinite));calls.plots.push([name,x.slice(),y.slice(),style]);return true;}
 };
 const ctx={Math,Number,String,Array,Error,isFinite,ui,plots,print(s){calls.logs.push(s);}};
 vm.createContext(ctx);
 vm.runInContext(source,ctx,{timeout:2000});
 return {values,events,calls,ctx,fire(id,event='clicked',arg){assert(events[id+'.'+event]);events[id+'.'+event](arg);}};
}
const t=init();
assert.equal(t.calls.hardware,0,'must never access hardware');
assert(t.calls.plots.some(p=>p[0]==='Interpolated calibration'));
// 2.5 V = 70 C between (2 V, 50 C) and (3 V, 90 C)
assert.equal(t.values.estimatedValue,70);
assert.equal(t.values.inputReadout,2.5);
assert.equal(t.values.sampleLog.length,0);
t.fire('saveSample');
assert.equal(t.values.sampleLog.length,1);
assert.equal(t.values.sampleLog[0][2],'70.00');
// Edited data changes interpolation after explicit validation.
t.values.calibrationTable[2][1]='70.0';
t.fire('calibrationTable','valueChanged',t.values.calibrationTable);
assert(t.values.calStatus.includes('EDITED'));
t.fire('validateCalibration');
assert.equal(t.values.estimatedValue,80);
t.fire('saveSample');
assert.equal(t.values.sampleLog.length,2);
assert.equal(t.values.sampleLog[1][2],'80.00');
// Sorting unordered valid rows without overwriting operator data.
t.values.calibrationTable.reverse();
t.fire('calibrationTable','valueChanged',t.values.calibrationTable);
t.fire('validateCalibration');
assert.equal(t.values.estimatedValue,80);
assert.equal(t.values.calibrationTable[0][0],'4.50');
// Duplicate x fails validation, output isn't marked as a new valid curve.
t.values.calibrationTable[1][0]=t.values.calibrationTable[0][0];
t.fire('calibrationTable','valueChanged',t.values.calibrationTable);
t.fire('validateCalibration');
assert(t.values.calStatus.includes('ERROR:'));
// Restore + sample log clear.
t.fire('restoreCalibration');
assert.equal(t.values.sampleLog.length,0);
assert.equal(t.values.estimatedValue,70);
t.fire('saveSample');t.fire('clearSamples');
assert.equal(t.values.sampleLog.length,0);
// Out of range should not extrapolate.
t.values.probeInput=0.1;
t.fire('probeInput','valueChanged',0.1);
assert(t.values.lookupStatus.includes('OUT OF RANGE'));
assert.equal(t.calls.hardware,0);
console.log('PASS calibration example: 2-page startup, valid native row arrays, linear interpolation, edits, sorting, duplicate rejection, log/clear, no extrapolation, no hardware');
