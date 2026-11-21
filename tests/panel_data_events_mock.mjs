// Execute the production Data JS bootstrap with a native-value bridge mock.
// The companion WFScriptPanelDataEventsSmoke CTest runs the same source in
// Qt's QJSEngine; this offline test works on machines without Qt.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const code=fs.readFileSync(new URL('../src/PanelDataEvents.h',import.meta.url),'utf8');
const m=code.match(/return QStringLiteral\(R"JS\(([\s\S]*?)\)JS"\);/);
assert.ok(m,'Native runtime JS source must remain extractable');
const values=new Map([['Motor.Speed',0],['Motor.Ready',false],['Motor.Status','Stopped']]);
const warnings=[];
let lastError='';
const context={__panelHost:{
  hasData(n){return values.has(n);},getData(n){return values.get(n);},dataNamesJs(){return [...values.keys()];},
  lastDataError(){return lastError;},
  setData(n,v){
    if(!values.has(n)){lastError='Unknown '+n;return false;}
    const old=values.get(n);
    if(typeof v!==typeof old||(typeof v==='number'&&!Number.isFinite(v))){lastError='Wrong type for '+n;return false;}
    values.set(n,v);return true;
  },
  reportDataCallbackError(n,error){warnings.push([n,String(error)]);}
}};
vm.createContext(context);
vm.runInContext(m[1],context,{filename:'PanelDataEvents.h'});
const {Data}=context;
const events=[];
const id=Data.onChange('Motor.Speed',(v,previous,name)=>events.push([v,previous,name]));
Data.set('Motor.Speed',10);
Data.set('Motor.Speed',10);
Data.set('Motor.Speed',11);
assert.deepEqual(events,[[10,0,'Motor.Speed'],[11,10,'Motor.Speed']]);
assert.equal(Data.offChange(id),true);
assert.equal(Data.offChange(id),false);
Data.set('Motor.Speed',12);
assert.equal(events.length,2);
const immediate=[];
const imm=Data.onChange('Motor.Status',(v,p,n)=>immediate.push([v,p,n]),{immediate:true});
assert.deepEqual(immediate,[['Stopped',undefined,'Motor.Status']]);
assert.equal(Data.offChange(imm),true);
for(const call of [
  ()=>Data.onChange('Missing.Value',()=>{}),
  ()=>Data.onChange('Motor.Speed',10),
  ()=>Data.onChange('Motor.Speed',()=>{},{immediate:'yes'}),
  ()=>Data.onChange('Motor.Speed',()=>{},{other:true}),
  ()=>Data.set('Motor.Speed','bad'),
  ()=>Data.set('Missing.Value',1),
]) assert.throws(call);
assert.equal(Data.get('Motor.Speed'),12);
const order=[];
const a=Data.onChange('Motor.Speed',(v)=>{order.push('speed:1');Data.set('Motor.Ready',true);});
const b=Data.onChange('Motor.Speed',()=>order.push('speed:2'));
const c=Data.onChange('Motor.Ready',()=>order.push('ready'));
Data.set('Motor.Speed',13);
assert.deepEqual(order,['speed:1','speed:2','ready'],'nested update must queue instead of recurse');
for(const token of [a,b,c])Data.offChange(token);
const throwing=Data.onChange('Motor.Speed',()=>{throw Error('intentional');});
let later=0;
const good=Data.onChange('Motor.Speed',()=>++later);
Data.set('Motor.Speed',14);
assert.equal(later,1,'throwing callback must not interrupt other callbacks');
assert.equal(warnings.length,1);
assert.match(warnings[0][1],/intentional/);
Data.offChange(throwing);Data.offChange(good);
// Emulate PanelRuntime's worker-thread dispatcher after an operator changes a
// native bound input. The actual native CTest additionally exercises the bridge.
const operatorCalls=[];
const operator=Data.onChange('Motor.Speed',(v,p)=>operatorCalls.push([v,p]));
const before=values.get('Motor.Speed');
assert.equal(context.__panelHost.setData('Motor.Speed',18),true);
context.__panelDataDispatch('Motor.Speed',values.get('Motor.Speed'),before);
assert.deepEqual(operatorCalls,[[18,14]]);
Data.offChange(operator);
// Unregistering a pending listener cancels its delivery; newly added listeners
// cannot retroactively receive the currently dispatched event.
const changing=[];
let late=0;
let secondToken,lateToken;
const firstToken=Data.onChange('Motor.Speed',()=>{
  changing.push('first');Data.offChange(secondToken);
  lateToken=Data.onChange('Motor.Speed',()=>++late);
});
secondToken=Data.onChange('Motor.Speed',()=>changing.push('second'));
Data.set('Motor.Speed',19);
assert.deepEqual(changing,['first']);
assert.equal(late,0);
Data.offChange(firstToken);
Data.offChange(lateToken);
const loop=Data.onChange('Motor.Speed',(v)=>Data.set('Motor.Speed',v+1));
assert.throws(()=>Data.set('Motor.Speed',100),/change cycle exceeded 256 events/);
assert.equal(Data.offChange(loop),true);
Data.set('Motor.Speed',7);
assert.equal(Data.get('Motor.Speed'),7,'queue recovers after loop guard');
let added=0;
const ids=[];
for(let i=0;i<256;i++)ids.push(Data.onChange('Motor.Status',()=>added++));
assert.throws(()=>Data.onChange('Motor.Status',()=>{}),/limit/);
for(const token of ids)assert.equal(Data.offChange(token),true);
Data.set('Motor.Status','Done');
assert.equal(added,0,'unsubscribed callbacks must not remain referenced');
console.log('PASS: production Data.onChange/offChange JS 10 cases: typed changes, native input, immediate, dedup, isolation, order, limits, feedback guard');
