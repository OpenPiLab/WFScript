// Run with node. Exercises the actual embedded example source with a fake
// native communications host. No Qt compilation or physical transport implied.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.join(__dirname,'..');
const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/CommunicationsTerminalLab.wfpanel'),'utf8'));
const source=fs.readFileSync(path.join(root,'examples/source/CommunicationsTerminalLab.js'),'utf8');
assert.equal(panel.script,source);
assert.equal(panel.pages.length,3);
for(const page of panel.pages){
 assert.equal(typeof page.width,'number');assert.equal(typeof page.height,'number');
 assert.equal(page.controls.filter(x=>x.type==='communications_terminal').length,1);
}
const bridge=fs.readFileSync(path.join(root,'src/CommunicationBridge.cpp'),'utf8');
const match=bridge.match(/QString CommunicationBridge::bootstrapSource\(\)\{return QStringLiteral\(R"JS\(([\s\S]*?)\)JS"\);\}/);
assert(match);new vm.Script(match[1]);
let next=1,opened=0,closed=0;
const objects=new Map(),handlers=new Map(),updates=new Map(),writes=[];
const entry=(kind,options)=>{const id=next++;objects.set(id,{id,kind,options,listeners:new Map(),open:false});return id;};
function fire(e,event,...args){if(e.listeners.has(event))e.listeners.get(event)(...args);}
const host={
 lastError:'Fake error',
 ports(){return [{port:'COM42',description:'Simulated COM',manufacturer:'Test',systemLocation:'COM42',serialNumber:''}];},
 createSerial:o=>entry('serial',o),createTcp:o=>entry('tcp',o),createServer:o=>entry('server',o),createUdp:o=>entry('udp',o),
 on(id,event,cb){assert(objects.has(id));objects.get(id).listeners.set(event,cb);return true;},
 off(id,event){objects.get(id).listeners.delete(event);return true;},
 start(id){const e=objects.get(id);assert(e);opened++;e.open=true;return true;},
 close(id){if(!objects.has(id))return false;closed++;objects.delete(id);return true;},
 info(id){const e=objects.get(id);return {valid:!!e,open:!!e?.open,localPort:20000+id,bytesReceived:0,bytesSent:0};},
 write(id,value){assert(objects.get(id)?.open);writes.push({id,value});const bytes=typeof value==='string'?[...Buffer.from(value)]:value;
   const sender=objects.get(id);
   if(sender.kind==='tcp'){
      const remote=[...objects.values()].find(x=>x.kind==='accepted');
      if(remote)fire(remote,'data',bytes);
   } else if(sender.kind==='accepted'){
      const client=[...objects.values()].find(x=>x.kind==='tcp');
      if(client)fire(client,'data',bytes);
   }
   return bytes.length;
 },
 send(id,value,hostName,port){assert(hostName==='127.0.0.1' && port>0);writes.push({id,value,hostName,port});
   const bytes=typeof value==='string'?[...Buffer.from(value)]:value;
   const recv=[...objects.values()].find(x=>x.kind==='udp' && 20000+x.id===port);
   if(recv){fire(recv,'message',bytes,{address:'127.0.0.1',port:20000+id});}
   return bytes.length;
 }
};
const ui={
 on(id,event,cb){handlers.set(id+'::'+event,cb);},
 set(id,value){if(!updates.has(id))updates.set(id,[]);updates.get(id).push(value);return true;}
};
const context=vm.createContext({__commHost:host,ui,Tool:{getText:(title,def,cb)=>cb({accepted:true,value:def})},
 print(){},console,Date,Array,Number,String,JSON,Error});
vm.runInContext(match[1],context);vm.runInContext(source,context);
// Simulate PanelRuntime's saved control.events mapping, including payloads.
for(const page of panel.pages)for(const control of page.controls){
 for(const [event,binding] of Object.entries(control.events||{})){
  assert(binding.enabled && typeof context[binding.handler]==='function',control.id);
  const key=control.id+'::'+event;
  assert(!handlers.has(key),'duplicate event registration '+key);
  handlers.set(key,payload=>context[binding.handler](payload));
 }
}
assert.equal(handlers.size,14,'all fourteen UI events are designer-managed');
assert.equal(opened,0);context.onInitialize();assert.equal(opened,0);
const press=id=>{assert(handlers.has(id+'::clicked'),id);handlers.get(id+'::clicked')();};
const send=(id,payload)=>{assert(handlers.has(id+'::sendRequested'),id);handlers.get(id+'::sendRequested')(payload);};
const textPayload={mode:'text',text:'HELLO TEST\n',bytes:[...Buffer.from('HELLO TEST\n')]};
const binaryPayload={mode:'hex',bytes:[0,1,2,127,128,254,255,0]};
press('tcpStartTerm');
const server=[...objects.values()].find(e=>e.kind==='server');assert(server);
// Qt emits listening asynchronously. The server then accepts a real connection.
fire(server,'listening');
const client=[...objects.values()].find(e=>e.kind==='tcp');assert(client);
const peerId=entry('accepted',{host:'127.0.0.1',port:20000+server.id});
const peer=objects.get(peerId);peer.open=true;
fire(server,'connection',peerId);fire(client,'connect');
send('termTcp',textPayload);send('termTcp',binaryPayload);
const tcp=updates.get('termTcp').map(x=>x.append).filter(Boolean);
assert(tcp.some(x=>x.direction==='TX'&&x.transport==='TCP'&&x.data.includes(255)));
assert(tcp.some(x=>x.direction==='RX'&&x.transport==='TCP'&&x.data.includes(255)));
press('tcpStopTerm');
press('udpStartTerm');
send('termUdp',textPayload);send('termUdp',binaryPayload);
const udp=updates.get('termUdp').map(x=>x.append).filter(Boolean);
assert(udp.some(x=>x.direction==='TX'&&x.transport==='UDP'&&x.data.includes(255)));
assert(udp.some(x=>x.direction==='RX'&&x.transport==='UDP'&&x.data.includes(255)));
press('udpStopTerm');
press('serialDiscoverTerm');press('serialOpenTerm');
const serial=[...objects.values()].find(e=>e.kind==='serial');assert(serial);
fire(serial,'open');
send('termSerial',textPayload);send('termSerial',binaryPayload);
fire(serial,'data',[0,127,128,255]);
const ser=updates.get('termSerial').map(x=>x.append).filter(Boolean);
assert(ser.some(x=>x.direction==='TX'&&x.data.includes(255)));
assert(ser.some(x=>x.direction==='RX'&&x.data.includes(255)));
press('serialCloseTerm');
assert(opened>=4 && closed>=5);context.onShutdown();
console.log('PASS CommunicationsTerminalLab: three Pages, opt-in I/O, TCP/UDP loopback, serial hooks, text/binary TX/RX');
