// v0.2-dev.8.5 single-Page Panel Data lab, using the PRODUCTION subscription bootstrap.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const path = new URL('../examples/panels/PanelDataLab.wfpanel', import.meta.url);
const doc = JSON.parse(fs.readFileSync(path,'utf8'));
assert.equal(doc.pages.length,1, 'controls and outputs stay together so responses can be observed live');
assert.equal(doc.pages[0].name,'Live Workbench');
const ids=doc.pages[0].controls.map(c=>c.id);
for(const id of ['startButton','stopButton','resetButton','targetInput','loadSlider','readySwitch',
                 'speedDisplay','speedDial','speedGauge','targetReadout','loadMeter','readyBeacon','motorStatus'])
  assert.ok(ids.includes(id), `${id} must be visible on the Live Workbench`);
for(const c of doc.pages[0].controls){
  assert.ok(c.x>=0 && c.y>=0 && c.x+c.width<=doc.pages[0].width &&
            c.y+c.height<=doc.pages[0].height,`${c.id} must fit within the workbench`);
}
assert.equal(doc.panelData.length,7);
const variables=new Map(doc.panelData.map(x=>[x.name,x.initial]));
const types=new Map(doc.panelData.map(x=>[x.name,x.type]));
const display=new Map();
const bindings=[];
let savedEvents=0;
for(const page of doc.pages) for(const c of page.controls){
  if(c.bindings?.value)bindings.push([c.id,c.bindings.value]);
  for(const [event,ref] of Object.entries(c.events||{})) if(ref.enabled){
    assert.match(ref.handler,/^[A-Za-z_$][\w$]*$/); savedEvents++;
  }
}
assert.equal(bindings.length,11);
assert.equal(savedEvents,5);
function refresh(name,val){for(const [id,variable] of bindings)if(variable===name)display.set(id,val);}
for(const [name,value] of variables)refresh(name,value);
let callback=null,serial=0,intervalId=null;
const source=fs.readFileSync(new URL('../src/PanelDataEvents.h',import.meta.url),'utf8');
const match=source.match(/return QStringLiteral\(R"JS\(([\s\S]*?)\)JS"\);/);
assert.ok(match,'production PanelDataEvents JS bootstrap found');
const unbound=new Map();
let lastError='';
const computedTargets=new Set();
const context={
  __panelHost:{
    hasData(name){return variables.has(name);},
    getData(name){return variables.get(name);},
    dataNamesJs(){return [...variables.keys()];},
    lastDataError(){return lastError;},
    validateComputedValue(name,value){
      const t=types.get(name);
      if(!t){lastError='Unknown '+name;return false;}
      const expected=t==='number'?'number':t==='text'?'string':'boolean';
      if(typeof value!==expected || (t==='number'&&!Number.isFinite(value)) ||
         (t==='text'&&value.length>4096)){lastError='Wrong type';return false;}
      return true;
    },
    registerComputedData(name){
      if(computedTargets.has(name)||!variables.has(name) || bindings.some(([id,n])=>
         n===name && ['slider','numeric_stepper','switch_button','toggle','option_selector','rotary_input'].includes(
           doc.pages[0].controls.find(c=>c.id===id).type))){lastError='Invalid computed target';return false;}
      computedTargets.add(name);return true;
    },
    setComputedData(name,value){
      if(!computedTargets.has(name)||!this.validateComputedValue(name,value))return false;
      if(variables.get(name)!==value){variables.set(name,value);refresh(name,value);}return true;
    },
    setData(name,value){
      if(computedTargets.has(name)){lastError='Computed target is read-only';return false;}
      const t=types.get(name);
      if(!t){lastError='Unknown '+name;return false;}
      const expected=t==='number'?'number':t==='text'?'string':'boolean';
      if(typeof value!==expected || (t==='number'&&!Number.isFinite(value))){lastError='Wrong type';return false;}
      if(variables.get(name)===value)return true;
      variables.set(name,value);refresh(name,value);return true;
    },
    reportDataCallbackError(name,error){throw Error('Unexpected onChange callback failure: '+name+' '+error);},
  },
  ui:{set(id,value){assert.ok(!bindings.some(([widget])=>widget===id),'cannot ui.set a bound widget'); unbound.set(id,value);return true;}},
  setInterval(fn,ms){assert.equal(ms,250);assert.equal(callback,null);callback=fn; intervalId=++serial;return intervalId;},
  clearInterval(id){assert.equal(id,intervalId);callback=null;intervalId=null;},
  print(...args){},
};
vm.createContext(context);
vm.runInContext(match[1],context,{filename:'PanelDataEvents.h'});
vm.runInContext(doc.script,context,{filename:'PanelDataLab.js'});
assert.match(unbound.get('hintControls'),/TARGET 1200 RPM = 20 rev\/s \(initial\)/, 'immediate first notification');
function event(id,type,value){
  const c=doc.pages.flatMap(p=>p.controls).find(c=>c.id===id);
  assert.ok(c);
  const name=c.bindings?.value;
  const interactive=(c.type==='numeric_stepper'&&type==='valueChanged')||
    (c.type==='switch_button'&&type==='toggled')||
    (c.type==='slider'&&type==='valueChanged')||
    (c.type==='option_selector'&&type==='selectionChanged');
  if(name&&interactive){
    const before=variables.get(name);
    assert.ok(context.__panelHost.setData(name,value));
    if(before!==variables.get(name))context.__panelDataDispatch(name,variables.get(name),before);
  }
  const fn=c.events?.[type]?.handler;
  if(fn)context[fn](value);
}
event('startButton','clicked',true);
assert.equal(callback,null,'START must be explicitly interlocked by READY');
assert.match(variables.get('Motor.Status'),/READY/);
event('readySwitch','toggled',true);
event('targetInput','valueChanged',2000);
event('modeSelector','selectionChanged','Manual');
assert.equal(variables.get('Motor.Mode'),'Manual');
assert.equal(display.get('targetReadout'),33.33);
assert.equal(context.Data.get('Motor.TargetRPS'),33.33);
assert.match(unbound.get('hintControls'),/TARGET 2000 RPM = 33.33 rev\/s \(changed from 1200\)/);
event('startButton','clicked',true);
assert.ok(callback);
callback();
assert.ok(variables.get('Motor.ActualRPM')>0, 'speed begins updating on same Page immediately');
for(let i=0;i<19;i++)callback();
assert.ok(variables.get('Motor.ActualRPM')>1500);
for(const id of ['speedDial','speedDisplay','speedGauge'])
  assert.equal(display.get(id),variables.get('Motor.ActualRPM'));
event('readySwitch','toggled',false);
assert.equal(callback,null,'onChange READY off shuts down simulation timer');
assert.match(variables.get('Motor.Status'),/READY switched off/);
event('readySwitch','toggled',true);
event('startButton','clicked',true);
assert.ok(callback,'restarting remains explicitly button-controlled');
event('stopButton','clicked',true);
assert.equal(callback,null);
assert.equal(display.get('speedDisplay'),0);
event('resetButton','clicked',true);
assert.equal(display.get('targetInput'),1200);
assert.equal(display.get('targetReadout'),20);
assert.equal(display.get('readySwitch'),false);
assert.equal(display.get('modeSelector'),'Auto');
context.onShutdown();
console.log('PASS: Panel Data 8.5 workbench uses production change subscriptions, immediate target label, operator READY interlock, two-way bindings, visual handlers and stop/reset');
