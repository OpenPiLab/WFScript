// Mock only panel-native services; use REAL imported modules from .wfpanel.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import * as DSP from '../../examples/libraries/DSPCore.mjs';
import * as pack from '../../examples/libraries/SignalPackage.mjs';
const project=JSON.parse(fs.readFileSync(new URL('../../examples/panels/JavaScriptDSPStudio.wfpanel',import.meta.url),'utf8'));
const controls=project.pages[0].controls;
const values=Object.fromEntries(controls.map(x=>[x.id,x.type==='option_selector'?'Sine':x.value]));
const series=[];const updates=[];const output=[];
const context={
    print(...args){output.push(args.join(' '));},
    modules:{load(path){if(path==='DSPCore.mjs')return DSP;if(path==='SignalPackage.mjs')return pack;throw Error('Unexpected import '+path);}},
    ui:{get(id){assert.ok(id in values);return values[id];},set(id,value){assert.ok(id in values);values[id]=value;updates.push([id,value]);return true;}},
    plots:{clear(id){assert.ok(['timePlot','fftPlot','filterPlot'].includes(id));},
           configure(id,settings){assert.equal(settings.chartType,'XY');},
           setSeries(id,label,x,y){assert.equal(x.length,y.length);assert.ok(x.length>=181);series.push([id,label,x.length]);}}
};
vm.createContext(context);
vm.runInContext(project.script,context,{timeout:20000,filename:'JavaScriptDSPStudio.js'});
assert.equal(series.length,5);
assert.match(values.studioStatus,/ANALYZED/);
assert.match(values.metricPeak,/512 Hz/);
assert.equal(project.libraries.length,5);
context.onPackClicked();
assert.match(values.studioStatus,/PACK VERIFIED/);
assert.match(values.metricPacked,/bytes/);
values.toneHz=880; values.cutoff=350;
context.onControlChanged();
assert.equal(series.length,10);
assert.match(values.studioStatus,/880 Hz/);
context.onNewNoiseClicked(); assert.equal(series.length,15);
context.onResetClicked();assert.equal(values.toneHz,512);assert.equal(values.noise,15);
values.cutoff=0;context.onControlChanged();assert.match(values.studioStatus,/ERROR:/);
console.log('PASS DSP Studio: actual module imports, 3 charts, live events, reset, packet roundtrip and errors');
