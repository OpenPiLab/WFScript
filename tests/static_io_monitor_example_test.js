// Script source contract test. This does not claim physical AD3 operation.
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const root=path.resolve(__dirname,'..');
const project=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/DigitalInputMonitor.wfpanel'),'utf8'));
const source=fs.readFileSync(path.join(root,'examples/source/DigitalInputMonitor.js'),'utf8');
const lib=fs.readFileSync(path.join(root,'examples/libraries/DigitalMonitorTestSteps.mjs'),'utf8');
assert.equal(project.pages.length,3);
assert.equal(project.script,source);
assert.equal(project.libraries[0].source,lib);
assert.equal(project.testLibrary,'DigitalMonitorTestSteps.mjs');
assert.equal(project.testDefinition.steps.length,3);
assert.deepEqual(project.testDefinition.steps.map(s=>s.group),['setup','main','cleanup']);
const controls=project.pages.flatMap(p=>p.controls);
const ids=new Set(controls.map(c=>c.id));
assert.equal(ids.size,controls.length);
for (const [,id] of source.matchAll(/ui\.on\('([^']+)'/g)) assert(ids.has(id),'Missing event control: '+id);
for (const [,id] of source.matchAll(/ui\.set\('([^']+)'/g)) assert(ids.has(id),'Missing bound UI control: '+id);
for (const p of project.pages) {
 for(const fld of ['id','name','width','height','controls'])assert(Object.hasOwn(p,fld),'Missing Page field: '+fld);
}
assert(!source.includes('onInitialize()')); // no implicit monitor startup
assert(source.includes('function armMonitor()'));
assert(source.includes('StaticIO.startMonitor('));
assert(source.includes('StaticIO.stopMonitor('));
assert(source.includes('StaticIO.simulation'));
assert(source.includes('TestRunner.run()'));
assert(!lib.includes('StaticIO.setPin('));
console.log('PASS: digital monitor three-page project and project-owned Test Executive contracts');
