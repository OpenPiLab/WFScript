// v0.2-dev.4 Script example behavioral smoke using a fake StaticIO / TestRunner.
// A source/Node mock, not Qt native timing and not actual AD3 testing.
const vm=require('vm'),fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..');
const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/DigitalInputMonitor.wfpanel'),'utf8'));
const source=fs.readFileSync(path.join(root,'examples/source/DigitalInputMonitor.js'),'utf8');
const widgets=new Set(panel.pages.flatMap(p=>p.controls.map(c=>c.id)));
const events=new Map(), updates=new Map(), pins=new Map([[0,0],[1,0]]);
const watches=new Map();let polling=false,started=0,stopped=0,runs=0,sdkSnapshots=0,sim=true;
const io={lastError:'',get simulation(){return sim;},get monitoring(){return polling;},
    onChange(pin,fn,opts){watches.set(pin,{fn,opts});return true;},
    clearOnChange(pin){watches.delete(pin);},clearAllOnChange(){watches.clear();},
    startMonitor(){started++;polling=true;return true;},stopMonitor(){stopped++;polling=false;},
    monitorStats(){return {monitoring:polling,polls:20,callbacks:2,changes:2,lastPollDurationUs:350,watchCount:watches.size};},
    setPin(pin,level){if(!sim)throw new Error('not simulation');pins.set(pin,level);return true;},
    status(){sdkSnapshots++;return {valid:true,inputMask:(pins.get(0)||0)|((pins.get(1)||0)<<1)};},
    readPin(pin){return pins.get(pin)||0;}};
const runner={lastError:'',run(){runs++;return true;},terminate(){return true;},reset(){return true;},
 onFinished(fn){this.finished=fn;},summary(){return {state:'Passed',passed:1,failed:0};}};
const ctx={StaticIO:io,TestRunner:runner,Date,JSON,String,Error,print:()=>{},
 ui:{on(id,event,fn){assert(widgets.has(id));assert.equal(event,'clicked');events.set(id,fn);},
 set(id,v){assert(widgets.has(id),'unknown widget '+id);updates.set(id,v);}}};
vm.createContext(ctx);vm.runInContext(source,ctx,{filename:'DigitalInputMonitor.js'});
assert.equal(events.size,0,'no manual UI registrations');
for(const ctrl of panel.pages.flatMap(p=>p.controls)) for(const [event,b] of Object.entries(ctrl.events||{})) {
    assert(b.enabled && typeof ctx[b.handler]==='function',ctrl.id+'/'+event);
    assert(!events.has(ctrl.id),'duplicate UI callback');
    events.set(ctrl.id,ctx[b.handler]);
}
assert.equal(events.size,19,'all monitor operator actions use saved designer events');
assert(!polling && started===0,'Panel startup must not start monitor');
events.get('callbacksRegister')();assert.equal(watches.size,2);assert.equal(started,0,'registration must not start monitor');
events.get('monitorStart')();assert(polling && started===1);
io.onChange(0,io.onChange.bind(io),{}); // not used; replace with test handlers below
// Re-register callbacks from Panel; trigger simulated physical START.
events.get('callbacksRegister')();watches.get(0).fn(0,0,{edge:'falling',stableMs:31,timestampUtc:'mock',timestampSource:'HostPollAfterSDK'});
assert.equal(runs,0,'physical START not armed yet');
events.get('stationArm')();assert(polling);watches.get(0).fn(0,0,{edge:'falling',stableMs:31,timestampUtc:'mock',timestampSource:'HostPollAfterSDK'});
assert.equal(runs,1,'armed start calls project-owned TestRunner');
events.get('stationDisarm')();watches.get(0).fn(0,0,{edge:'falling',stableMs:31,timestampUtc:'mock',timestampSource:'HostPollAfterSDK'});
assert.equal(runs,1,'disarmed does not run test');
events.get('monitorRead')();assert.equal(sdkSnapshots,1,'manual read remains available');
events.get('monitorStop')();assert(!polling && stopped>=1,'explicit stop');
events.get('callbacksClearAll')();assert.equal(watches.size,0);
// Physical mode: SIM BUTTON must refuse to call setPin().
sim=false;const before=pins.get(0);events.get('simStartHigh')();assert.equal(pins.get(0),before);
assert(updates.has('monitorState'));
console.log('PASS: opt-in monitor Panel mock, callbacks separate from polling, test auto-arm, explicit stop, physical SIM safety');
