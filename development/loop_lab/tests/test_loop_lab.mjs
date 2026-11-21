// Independent JavaScript mock of the native RC1.5.11 loop plan and callback lab.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import * as procedures from '../SweepLoopSteps.mjs';
const panel=JSON.parse(fs.readFileSync(new URL('../WFScript_Test_Executive_Sweeps_Loops_Lab_RC1_5_11.wfpanel',import.meta.url),'utf8'));
assert.equal(panel.testDefinition.steps.length,5);
let token=null,resolveStep=null,rejectStep=null,serial=0;
globalThis.TestRunner={
    isActive(t){return t===token && resolveStep!==null;},
    complete(t,value){if(!this.isActive(t))return false;const r=resolveStep;token=null;resolveStep=null;rejectStep=null;r(value);return true;},
    fail(t,reason){if(!this.isActive(t))return false;const r=rejectStep;token=null;resolveStep=null;rejectStep=null;r(Error(String(reason)));return true;}
};
const actual=[];
function runAxis(axis){
    if(axis.values)return axis.values;
    const count=Math.round((axis.stop-axis.start)/axis.increment)+1;
    return Array.from({length:count},(_,i)=>axis.start+i*axis.increment);
}
function expand(node,params={}) {
    const axes=node.loop ? (node.loop.axes||[node.loop]) : [];
    let contexts=[{...params}];
    for(const axis of axes){
        contexts=contexts.flatMap(p=>runAxis(axis).map(v=>({...p,[axis.parameter]:v})));
    }
    const count=node.repeat?.count||1;
    for(const p of contexts)for(let i=0;i<count;i++){
        const context={...p,...(node.repeat?{[node.repeat.index||'iteration']:i}:{})};
        if(node.steps)for(const child of node.steps)expand(child,context);
        else actual.push({...node,parameters:{...(node.parameters||{}),...context}});
    }
}
for(const node of panel.testDefinition.steps)expand(node);
assert.equal(actual.length,12);
let checks=0;
function bound(spec,params){return typeof spec==='number'?spec:params[spec.param]*(spec.factor??1)+(spec.offset??0)}
for(const step of actual){
    const t='token_'+(++serial);
    token=t;
    const resultPromise=new Promise((resolve,reject)=>{resolveStep=resolve;rejectStep=reject});
    const outcome=procedures[step.handler]({id:step.id,params:step.parameters,token:t});
    assert.equal(outcome,undefined);
    const result=await Promise.race([resultPromise,new Promise((_,reject)=>setTimeout(()=>reject(Error('timeout')),1200))]);
    if(step.measurements){
        for(const [path,limit] of Object.entries(step.measurements)){
            const n=result[path];
            assert(Number.isFinite(n),`${step.id}.${path} not numeric`);
            assert(n>=bound(limit.low,step.parameters) && n<=bound(limit.high,step.parameters));
            ++checks;
        }
    } else if(step.limits) {
        assert(result>=bound(step.limits.low,step.parameters) && result<=bound(step.limits.high,step.parameters));
        ++checks;
    } else assert.equal(result,true);
}
assert.equal(checks,17);
assert.equal(TestRunner.complete('expired',true),false);
console.log('PASS: hardware-free lab 5 authored items → 12 steps, 17 measurements; token completion and stale callback rejection');
