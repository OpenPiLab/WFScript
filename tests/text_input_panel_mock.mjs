// Offline example-script smoke. Native GUI itself requires the Qt CTest.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
const root = new URL('../development/text_inputs/TextInputLab.wfpanel',import.meta.url);
const panel=JSON.parse(fs.readFileSync(fileURLToPath(root),'utf8'));
const controls=panel.pages.flatMap(p=>p.controls);
const state=Object.fromEntries(controls.map(c=>[c.id,c.properties?.defaultText??c.text]));
const vars=Object.fromEntries(panel.panelData.map(v=>[v.name,v.initial]));
const bindings=controls.filter(c=>c.bindings?.value);
const logs=[];
const mock={
    ui:{get(id){return state[id];},set(id,value){state[id]=String(value);return true;}},
    Data:{get(name){return vars[name];},set(name,value){vars[name]=value;for(const c of bindings)
        if(c.bindings.value===name)state[c.id]=value;return true;}},
    print(text){logs.push(text);}
};
vm.createContext(mock);
vm.runInContext(panel.script,mock,{filename:panel.scriptName});
mock.onSetExamples();
assert.equal(state.scopeIp,'192.168.6.2');
assert.equal(state.operatorName,'Bench Operator');
mock.onOperatorNameChanged('Sam');
assert.match(state.inputStatus,/Accepted name: Sam/);
mock.onScopeAddressInvalid('999.2.3.4');
assert.match(state.inputStatus,/Invalid IPv4/);
mock.onPanelDataDemo();
assert.equal(state.dataIp1,'192.168.6.2');
assert.equal(state.dataIp2,'192.168.6.2');
assert.equal(state.dataAlias1,'Oscilloscope A');
assert.equal(state.dataAlias2,'Oscilloscope A');
mock.onReadDataDemo();
assert.match(state.dataStatus,/Oscilloscope A/);
assert.ok(logs.length>0);
console.log('PASS: TextInputLab script handlers, status updates and Panel Data mock synchronization');
