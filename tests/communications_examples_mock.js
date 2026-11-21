// Run: node tests/communications_examples_mock.js
// Executes the real C++ bootstrap JavaScript and embedded example scripts
// against a deterministic fake native transport. Does NOT claim Qt validation.
const fs=require('node:fs');
const vm=require('node:vm');
const path=require('node:path');
const assert=require('node:assert/strict');
const root=path.join(__dirname,'..');
const bridge=fs.readFileSync(path.join(root,'src/CommunicationBridge.cpp'),'utf8');
const match=bridge.match(/QString CommunicationBridge::bootstrapSource\(\)\{return QStringLiteral\(R"JS\(([\s\S]*?)\)JS"\);\}/);
assert(match,'C++ bootstrap source not found');
new vm.Script(match[1]);
const cases=[
 ['SerialCommsLab','serDiscover','serOpen','serText','serBinary','serClose'],
 ['TcpCommsLab','tcpStart','tcpText','tcpBinary','tcpStop'],
 ['UdpCommsLab','udpStart','udpText','udpBinary','udpStop']
];
for(const row of cases){
 const [name,...actions]=row;
 let next=1,opened=0,closed=0;
 const objects=new Map();const handlers=new Map(), sent=[];
 const ui={
    set(id,value){return true;},get(id){return null;},
    on(id,event,cb){assert.equal(event,'clicked');handlers.set(id,cb);}
 };
 function entry(kind,opts){let id=next++;objects.set(id,{id,kind,opts,callbacks:new Map(),open:false});return id;}
 const host={
    lastError:'fake failure',
    ports(){return [{port:'COM55',description:'Fake Device',manufacturer:'WFScript',systemLocation:'COM55',serialNumber:'1'}];},
    createSerial:options=>entry('serial',options),
    createTcp:options=>entry('tcp',options),
    createServer:options=>entry('server',options),
    createUdp:options=>entry('udp',options),
    on(id,event,cb){let e=objects.get(id);assert(e);e.callbacks.set(event,cb);return true;},
    off(id,event){objects.get(id).callbacks.delete(event);return true;},
    start(id){let e=objects.get(id);assert(e);opened++;e.open=true;return true;},
    write(id,value){assert(objects.get(id).open);sent.push({id,value});return typeof value==='string'?Buffer.byteLength(value):value.length;},
    send(id,value,host,port){assert(host && port>0);sent.push({id,value,host,port});return typeof value==='string'?Buffer.byteLength(value):value.length;},
    close(id){let e=objects.get(id);if(!e)return false;closed++;objects.delete(id);return true;},
    info(id){let e=objects.get(id);return {valid:!!e,open:!!e?.open,localPort:10200+id,bytesReceived:0,bytesSent:0};}
 };
 const context=vm.createContext({__commHost:host,ui,Tool:{getText:(title,chosen,cb)=>cb({accepted:true,value:chosen})},
   print(){},console,Date,Array,Number,String,JSON,Error});
 vm.runInContext(match[1],context,{filename:'comm-bootstrap.js'});
 const source=fs.readFileSync(path.join(root,'examples/source',name+'.js'),'utf8');
 vm.runInContext(source,context,{filename:name+'.js'});
 // Load saved Property Editor event metadata as PanelRuntime does on startup.
 const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels',name+'.wfpanel'),'utf8'));
 for(const page of panel.pages)for(const widget of page.controls)
   for(const [event,binding] of Object.entries(widget.events||{})){
     assert.equal(event,'clicked');assert.equal(binding.enabled,true);
     const fn=context[binding.handler];assert.equal(typeof fn,'function',binding.handler);
     assert(!handlers.has(widget.id),'duplicate manual UI registration: '+widget.id);
     handlers.set(widget.id,value=>fn(value));
   }
 assert.equal(handlers.size,name==='SerialCommsLab'?6:4,`${name}: expected designer bindings`);

 assert.equal(opened,0,`${name}: no I/O before explicit start`);
 context.onInitialize();assert.equal(opened,0,`${name}: no I/O on onInitialize`);
 for(const action of actions){
   assert(handlers.has(action),`${name}: missing button handler ${action}`);
   handlers.get(action)();
   if(name==='TcpCommsLab' && action==='tcpStart'){
      const serv=[...objects.values()].find(e=>e.kind==='server');
      assert(serv && serv.callbacks.has('listening'));
      serv.callbacks.get('listening')();
      for(const c of objects.values()) if(c.kind==='tcp' && c.callbacks.has('connect'))c.callbacks.get('connect')();
   }
 }
 assert(opened>0,`${name}: expected explicit I/O start`);
 assert(closed>0,`${name}: expected cleanup`);
 if(name==='SerialCommsLab') {
  const raw=sent.map(x=>x.value);assert(raw.some(v=>typeof v==='string'&&v.includes('PING')));
  assert(raw.some(v=>Array.isArray(v)&&v.includes(255)));
 } else if(name==='TcpCommsLab') {
  assert(sent.some(x=>typeof x.value==='string'&&x.value.endsWith('\n')));
  assert(sent.some(x=>Array.isArray(x.value)&&x.value.includes(255)));
 } else {
  assert(sent.some(x=>typeof x.value==='string'));
  assert(sent.some(x=>Array.isArray(x.value)&&x.value.includes(255)));
 }
 context.onShutdown();
 console.log(`PASS ${name}: no automatic I/O, buttons, text and binary sends, explicit close`);
}
