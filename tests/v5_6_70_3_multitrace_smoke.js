const fs=require('fs'),vm=require('vm'),assert=require('assert');
const path=require('path');
const base=path.resolve(__dirname,'..');
const script=fs.readFileSync(path.join(base,'examples/source/TableDrivenWavegen.js'),'utf8');
const panel=JSON.parse(fs.readFileSync(path.join(base,'examples/panels/TableDrivenWavegen.wfpanel'),'utf8'));
assert.strictEqual(panel.script,script,'Embedded JS identical to standalone source');
const init=panel.pages.find(p=>p.id==='recipe').controls.find(c=>c.id==='recipeTable').properties.initialRows.split('\n').map(x=>x.split('|'));
function setup(physical=false,faultAt=0){
 const model={}; const handlers={}; const logs=[]; const plotSeries=new Map();
 const calls={fromScope:[],setSeries:[],style:[],clears:0,configure:[],hwRuns:[],dwell:[],scopeStop:0,waveStop:0,scopeSingle:0};
 const controls={recipeTable:init.map(x=>x.slice())};
 const wg={Channel1:{enable:false,Mode:{text:'Simple'},Simple:{Type:{text:'Sine'},Frequency:{value:100},Amplitude:{value:.5},Offset:{value:0},Phase:{value:0}}},Channel2:{enable:false},run(){calls.hwRuns.push({hz:this.Channel1.Simple.Frequency.value,amp:this.Channel1.Simple.Amplitude.value});return true;},stop(){calls.waveStop++;return true;},status(){return {carrierFunction:'Sine'};}};
 let scopeBuffer=[];
 const scope={Time:{Rate:{value:0},Samples:{value:0}},Channel1:{Range:{value:5},Offset:{value:0},Attenuation:{value:1},statistics(){if(faultAt&&calls.scopeSingle===faultAt)throw Error('simulated acquisition fault');const amp=wg.Channel1.Simple.Amplitude.value;return {count:8192,mean:0,rms:amp/Math.SQRT2};}},Trigger:{Trigger:{text:'None'}},single(){calls.scopeSingle++;return true;},wait(){const f=wg.Channel1.Simple.Frequency.value,a=wg.Channel1.Simple.Amplitude.value;scopeBuffer=Array.from({length:128},(_,i)=>a*Math.sin(2*Math.PI*f*i/250000));return true;},stop(){calls.scopeStop++;return true;}};
 const ctx={Device:{SN:physical?'210415AAA':'DEMO'},Wavegen1:wg,Scope1:scope,ui:{get(id){return controls[id];},set(id,v){controls[id]=v;return true;},on(id,event,fn){handlers[id+'.'+event]=fn;}},plots:{clear(id){assert(id==='wavePreview');plotSeries.clear();calls.clears++;return true;},configure(id,opts){calls.configure.push(opts);return true;},fromScope(id,ch,name){assert(physical,'physical capture expected');assert(ch===1);calls.fromScope.push(name);plotSeries.set(name,scopeBuffer.slice());return true;},setSeries(id,name,x,y,opts){assert(!physical||name.includes('MODEL ONLY'),'physical setSeries allowed only for explicit preview');assert.strictEqual(x.length,y.length);calls.setSeries.push({name,length:x.length,opts});plotSeries.set(name,y.slice());return true;},styleSeries(id,name,opts){assert(plotSeries.has(name),'style must follow copied capture');calls.style.push({name,opts});return true;}},wait(s){assert(s>=.1&&s<=1);calls.dwell.push(s);},print(x){logs.push(x);},Math,Array,Number,String,Error,isFinite};
 vm.createContext(ctx);vm.runInContext(script,ctx);
 return {controls,handlers,logs,calls,plotSeries,wg,scope,ctx};
}
function fire(test,id,e='clicked',arg){assert(test.handlers[id+'.'+e],id+'.'+e);test.handlers[id+'.'+e](arg);}
// Model-only PREVIEW ALL creates a separate waveform for each enabled recipe row.
let sim=setup(false);
fire(sim,'previewRecipe');
assert.equal(sim.plotSeries.size,4,'preview must have 4 distinct model curves');
assert([...sim.plotSeries.keys()].every(n=>n.includes('MODEL ONLY')));
assert.equal(sim.calls.fromScope.length,0);
assert.equal(sim.calls.hwRuns.length,0);
assert.equal(sim.controls.sweepResults.length,4);
assert(sim.controls.sweepResults.every(r=>r[7]==='MODEL ONLY'));
assert.equal(sim.calls.clears,1,'preview should clear once only');
fire(sim,'runAll');
assert.equal(sim.plotSeries.size,4);
assert.equal(sim.calls.clears,2,'new sweep clears once');
assert.equal(sim.calls.fromScope.length,0);
assert.equal(sim.calls.hwRuns.length,0);
fire(sim,'resetCursor');assert.equal(sim.plotSeries.size,0);
fire(sim,'applyNext');fire(sim,'applyNext');
assert.equal(sim.plotSeries.size,2,'apply next retains prior model waveform');
assert.equal(sim.controls.currentHz,250);
// Physical RUN ALL must copy actual independent Scope data for each row.
const physical=setup(true);
fire(physical,'runAll');
assert.equal(physical.calls.hwRuns.length,4);
assert.equal(physical.calls.scopeSingle,4);
assert.equal(physical.calls.fromScope.length,4);
assert.equal(physical.plotSeries.size,4);
assert.equal(physical.calls.clears,1,'must not clear plot for each row');
assert.equal(physical.calls.style.length,4,'traces distinguishable by colour');
assert([...physical.plotSeries.keys()].every(n=>n.includes('CH1 measured')));
assert.equal(physical.calls.setSeries.length,0,'no model waveforms substituted for physical data');
assert.equal(physical.controls.sweepResults.length,4);
assert(physical.controls.sweepResults.every(r=>r[7]==='PASS'));
assert(physical.controls.runStatus.includes('4 separate CH1 captures'));
assert(physical.calls.waveStop>0);
// Earlier captured arrays must not turn into latest Scope buffer when W1 changes.
const physicalTraces=[...physical.plotSeries.values()];
assert.notDeepStrictEqual(physicalTraces[0],physicalTraces[3],'capture 1 and 4 cannot be same samples');
const firstRMS=physicalTraces[0].reduce((s,v)=>s+v*v,0);
assert(firstRMS>0);
// APPLY NEXT physically captures too, and retains each captured trace.
const manual=setup(true);
fire(manual,'applyNext');fire(manual,'applyNext');
assert.equal(manual.calls.fromScope.length,2);
assert.equal(manual.plotSeries.size,2);
assert.equal(manual.calls.hwRuns.length,2);
assert.equal(manual.controls.sweepResults.length,2);
assert(manual.controls.runStatus.includes('W1 ACTIVE'));
fire(manual,'stopW1');
// Failed acquisition preserves earlier captures; stops physical output.
const failed=setup(true,3);
fire(failed,'runAll');
assert(failed.controls.runStatus.includes('ERROR'));
assert.equal(failed.calls.fromScope.length,2);
assert.equal(failed.plotSeries.size,2,'partial history retained on failure');
assert.equal(failed.calls.waveStop>0,true);
// Editing recipe invalidates previous traces and never enables output.
const edits=setup(false);
fire(edits,'previewRecipe');
edits.controls.recipeTable[0][0]='350';
fire(edits,'recipeTable','valueChanged',edits.controls.recipeTable);
assert.equal(edits.plotSeries.size,0,'clear stale capture for edited recipe');
assert.equal(edits.controls.sweepResults.length,0);
assert.equal(edits.calls.hwRuns.length,0);
console.log('PASS: 4 independent measured capture series / physical; model-only overlay; manual NEXT capture; previous traces retained; plot cleared once per sweep; failure cleanup; no simulated IO; stale trace invalidation');
