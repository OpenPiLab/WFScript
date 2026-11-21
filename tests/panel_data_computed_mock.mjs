// Execute exactly the production QJSEngine bootstrap in Node's JS VM.
// The companion native CTest uses the real C++ PanelUiBridge.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source=fs.readFileSync(new URL('../src/PanelDataEvents.h',import.meta.url),'utf8');
const bootstrap=source.match(/return QStringLiteral\(R"JS\(([\s\S]*?)\)JS"\);/);
assert.ok(bootstrap);
const vars=new Map([
 ['Motor.RPM',600],['Motor.RPS',0],['Motor.Period',0],['Motor.HzDouble',0],
 ['Motor.Overload',false],['Motor.Load',50],['Motor.Efficiency',0],
 ['Cycle.A',1],['Cycle.B',2],['Other.Name','Idle'],['Other.Text',''],
 ['Bad.Target',0],['Input.Derived',0]
]);
const readOnly=new Set();
const faults=[];
let lastError='';
const inputBindings=new Set(['Input.Derived']);
let writes=0;
const host={
 hasData:n=>vars.has(n),getData:n=>vars.get(n),dataNamesJs:()=>[...vars.keys()],
 lastDataError:()=>lastError,
 validateComputedValue(n,v){
   if(!vars.has(n)) {lastError='Unknown '+n;return false;}
   const before=vars.get(n);
   if(typeof before!==typeof v || (typeof v==='number'&&!Number.isFinite(v)) || (typeof v==='string'&&v.length>4096)){
     lastError='Wrong typed value for '+n;return false;
   }
   lastError='';return true;
 },
 registerComputedData(n){
   if(!vars.has(n)||readOnly.has(n)||inputBindings.has(n)){lastError='Cannot mark '+n+' computed';return false;}
   readOnly.add(n);return true;
 },
 setComputedData(n,v){if(!readOnly.has(n)||!this.validateComputedValue(n,v))return false;
   if(vars.get(n)!==v)++writes;vars.set(n,v);return true;},
 setData(n,v){
   if(readOnly.has(n)) {lastError='computed and read-only';return false;}
   if(!this.validateComputedValue(n,v))return false;
   vars.set(n,v);return true;
 },
 reportDataCallbackError(n,e){faults.push([n,String(e)]);}
};
const context={__panelHost:host};
vm.createContext(context);
vm.runInContext(bootstrap[1],context,{filename:'PanelDataEvents.h'});
const {Data}=context;
assert.equal(Data.isComputed('Motor.RPM'),false);
assert.equal(Data.computed('Motor.RPS',['Motor.RPM'],rpm=>rpm/60),'Motor.RPS');
assert.equal(Data.get('Motor.RPS'),10);
assert.equal(Data.isComputed('Motor.RPS'),true);
assert.throws(()=>Data.set('Motor.RPS',5),/read-only/);
assert.equal(host.setData('Motor.RPS',5),false,'native bridge must also reject external writes');
assert.equal(Data.computed('Motor.Period',['Motor.RPS'],rps=>rps===0?0:1/rps),'Motor.Period');
assert.equal(Data.computed('Motor.HzDouble',['Motor.RPM','Motor.RPS'],(rpm,rps)=>rpm/60+rps),'Motor.HzDouble');
assert.equal(Data.computed('Motor.Overload',['Motor.RPM'],rpm=>rpm>2000),'Motor.Overload');
assert.equal(Data.computed('Motor.Efficiency',['Motor.RPM','Motor.Load'],(rpm,load)=>Number((rpm*(100-load)/100).toFixed(2))),'Motor.Efficiency');
assert.equal(Data.computed('Other.Text',['Other.Name'],name=>name+'!'),'Other.Text');
assert.equal(Data.get('Other.Text'),'Idle!');
const observed=[];
const id=Data.onChange('Motor.RPM',(rpm,old)=>{
  observed.push(['rpm',rpm,old,Data.get('Motor.RPS'),Data.get('Motor.Period')]);
});
const rpsId=Data.onChange('Motor.RPS',(rps,old)=>observed.push(['rps',rps,old]));
const pId=Data.onChange('Motor.Period',(period)=>observed.push(['period',period]));
const dId=Data.onChange('Motor.HzDouble',(value)=>observed.push(['double',value]));
const beforeWrites=writes;
Data.set('Motor.RPM',1200);
assert.equal(Data.get('Motor.RPS'),20);
assert.equal(Data.get('Motor.Period'),0.05);
assert.equal(Data.get('Motor.HzDouble'),40);
assert.equal(writes-beforeWrites,4,'four derived targets, each calculated and written once (diamond)');
assert.deepEqual(observed.map(x=>x[0]),['rpm','rps','period','double']);
assert.equal(observed[0][3],20,'source listener sees already updated computed value');
assert.equal(observed[0][4],0.05);
for(const token of [id,rpsId,pId,dId])Data.offChange(token);
const valuesBefore=writes;
Data.set('Motor.RPM',1200);
assert.equal(writes,valuesBefore,'equal input must not compute or publish');
Data.set('Motor.RPM',2100);
assert.equal(Data.get('Motor.Overload'),true);
Data.set('Motor.Load',20);
assert.equal(Data.get('Motor.Efficiency'),1680);
Data.set('Other.Name','Ready');
assert.equal(Data.get('Other.Text'),'Ready!');
// Native bound input must dispatch via the production native-input entry point.
const old=vars.get('Motor.RPM');
assert.equal(host.setData('Motor.RPM',3000),true);
context.__panelDataDispatch('Motor.RPM',3000,old);
assert.equal(Data.get('Motor.RPS'),50);
assert.equal(Data.get('Motor.Period'),0.02);
assert.equal(Data.get('Motor.HzDouble'),100);
assert.throws(()=>Data.computed('Motor.RPS',['Motor.RPM'],v=>v),/already registered/);
for(const op of [
 ()=>Data.computed('missing',['Motor.RPM'],x=>x),
 ()=>Data.computed('Bad.Target',[],()=>0),
 ()=>Data.computed('Bad.Target',['Missing'],()=>0),
 ()=>Data.computed('Bad.Target',['Motor.RPM','Motor.RPM'],()=>0),
 ()=>Data.computed('Bad.Target',['Bad.Target'],x=>x),
 ()=>Data.computed('Bad.Target',['Motor.RPM'],22),
 ()=>Data.computed('Bad.Target',['Motor.RPM'],x=>'wrong'),
 ()=>Data.computed('Bad.Target',['Motor.RPM'],x=>{Data.set('Motor.RPM',4);return x;}),
 ()=>Data.computed('Input.Derived',['Motor.RPM'],v=>v),
]) assert.throws(op);
assert.equal(Data.isComputed('Bad.Target'),false,'failed registration must not lock target');
assert.equal(Data.isComputed('Input.Derived'),false,'editable target must not be converted');
assert.equal(host.setData('Bad.Target',3),true);
assert.equal(Data.computed('Cycle.A',['Cycle.B'],x=>x),'Cycle.A');
assert.throws(()=>Data.computed('Cycle.B',['Cycle.A'],x=>x),/circular/);
assert.equal(Data.isComputed('Cycle.B'),false);
assert.throws(()=>Data.set('Cycle.A',50),/read-only/);
// Bad calculation on update is isolated: no partial computed commits; the
// source subscription still receives its update, and a diagnostic is recorded.
assert.equal(Data.computed('Bad.Target',['Motor.Load'],v=>v===99?NaN:v),'Bad.Target');
let originalSeen=0;
const listener=Data.onChange('Motor.Load',()=>++originalSeen);
const snapshot=Data.get('Bad.Target');
Data.set('Motor.Load',99);
assert.equal(originalSeen,1);
assert.equal(Data.get('Bad.Target'),snapshot);
assert.equal(faults.length,1);
assert.match(faults[0][0],/Data.computed after Motor.Load/);
Data.offChange(listener);
Data.set('Motor.Load',10);
assert.equal(Data.get('Bad.Target'),10,'computed target recovers on next good input');
assert.equal(faults.length,1);
console.log('PASS: computed variables — source/native input, typed read-only targets, diamonds, chains, cycles, initialization, rejected writes, recovery, diagnostics');
