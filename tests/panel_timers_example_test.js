// Offline PanelTimers example logic contract. Native timing is tested by WFScriptTimerSmoke.
const fs=require('fs'), vm=require('vm'), assert=require('assert'), path=require('path');
const root=path.join(__dirname,'..');
const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/PanelTimers.wfpanel'),'utf8'));
const source=fs.readFileSync(path.join(root,'examples/source/PanelTimers.js'),'utf8');
assert.equal(panel.script,source);
assert.equal(panel.pages.length,2);
assert.equal(panel.lifecycle.initializeEnabled,true);
assert.equal(panel.lifecycle.shutdownEnabled,true);
assert.equal(panel.testDefinition && Object.keys(panel.testDefinition).length,0);
let id=0, timers=new Map(), events=new Map(), data={}, log=[], prints=[];
const controls=new Set(panel.pages.flatMap(p=>p.controls.map(c=>c.id)));
for(const pg of panel.pages)for(const c of pg.controls)assert(!c.events || typeof c.events==='object');
const ctx={
  print:(...args)=>prints.push(args.join(' ')),
  ui:{set(name,value){assert(controls.has(name),'Unknown widget '+name);data[name]=value;if(name==='timerLog') log.push(value);return true;},get(name){return data[name];},on(name,ev,fn){assert(controls.has(name));events.set(name+'::'+ev,fn);}},
  setInterval(fn,ms,...args){assert(ms>=10);const next=++id;timers.set(next,{fn,ms,args,repeat:true});return next;},
  setTimeout(fn,ms,...args){assert(ms>=10);const next=++id;timers.set(next,{fn,ms,args,repeat:false});return next;},
  clearTimeout(n){timers.delete(n);},clearInterval(n){timers.delete(n);},Date,JSON,String,Number,Boolean,Math,
};
vm.createContext(ctx);
vm.runInContext(source,ctx,{filename:'PanelTimers.js'});
// The real PanelRuntime dispatches metadata-assigned functions directly after
// recording the UI state; mimic that path instead of relying on ui.on().
for(const page of panel.pages)for(const c of page.controls){
  for(const [event,binding] of Object.entries(c.events||{})){
    assert(binding.enabled && typeof ctx[binding.handler]==='function',c.id);
    const key=c.id+'::'+event;
    assert(!events.has(key),'duplicate manual registration '+key);
    events.set(key,payload=>ctx[binding.handler](payload));
  }
}
assert.equal(events.size,10,'ten designer-managed timer buttons');

assert.equal(timers.size,0,'no timers before onInitialize');
assert(!source.includes('Protocol1.') && !source.includes('Wavegen1.') && !source.includes('Supplies.'),'no hardware API calls');
vm.runInContext('onInitialize()',ctx);
assert.equal(timers.size,1,'onInitialize creates only one interval');
assert(events.has('timerStartFast::clicked') && events.has('guideStart::clicked'));
events.get('timerStartFast::clicked')();
assert.equal(timers.size,2,'second timer runs simultaneously');
const fast=[...timers.entries()].find(([k,v])=>v.ms===250);assert(fast);
fast[1].fn(...fast[1].args);
assert.equal(data.timerFastCount,1);
events.get('guideStart::clicked')();assert.equal(timers.size,2,'double-start must not register duplicate');
events.get('timerStartOnce::clicked')();
assert.equal(timers.size,3);
const once=[...timers.entries()].find(([k,v])=>!v.repeat);assert.equal(once[1].args[0].startsWith('2-second'),true);
events.get('guideCancel::clicked')();assert.equal(timers.size,2);
events.get('guideOnce::clicked')();const once2=[...timers.entries()].find(([k,v])=>!v.repeat);
timers.delete(once2[0]);once2[1].fn(...once2[1].args);
assert.equal(data.timerNotice,'2-second one-shot fired (argument passed into callback)');
assert.equal(timers.size,2);
events.get('guideStop::clicked')();assert.equal(timers.size,1);
vm.runInContext('onShutdown()',ctx);assert.equal(timers.size,0);
assert(log.some(l=>l.append && l.append.level==='PASS'));
console.log('PASS: Timer Lab event handlers, lifecycle, two Pages, interval, timeout, cancellation, no hardware startup');
