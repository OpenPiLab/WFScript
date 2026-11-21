/**
 * v0.2-dev standalone Node mock regression for five self-contained protocol
 * Panels. No Qt SDK, `dwf.dll`, or physical instrument required.
 * Run: node tests/protocol_panel_examples_test.js
 */
const fs=require('fs'), path=require('path'), vm=require('vm'), assert=require('assert');
const root=path.resolve(__dirname,'..');
const catalog=JSON.parse(fs.readFileSync(path.join(root,'examples/catalog.json'),'utf8'));
const exampleIds=['ProtocolUART','ProtocolSPI','ProtocolI2C','ProtocolCAN','ProtocolSWD'];
function buildProtocol(calls) {
  const make=(name)=>new Proxy({
    configure(settings){calls.push([name,'configure',settings]);return true;},
    startRx(){calls.push([name,'startRx']);return true;},
    Receiver(){calls.push([name,'Receiver']);return true;},
    writeText(text){calls.push([name,'writeText',text]);return {ok:true,simulated:true};},
    read(){calls.push([name,'read']);return {ok:true,data:[],count:0,simulated:true};},
    transfer(tx,n){calls.push([name,'transfer']);return {ok:true,rx:Array.from({length:n||tx.length},()=>0),simulated:true};},
    command(cb,val,db,tx,n){calls.push([name,'command']);return {ok:true,rx:Array(n).fill(0),simulated:true};},
    writeRead(addr,tx,n){calls.push([name,'writeRead']);return {ok:true,data:Array(n).fill(0),nackIndex:-1,acknowledged:false,simulated:true};},
    spyPoll(){calls.push([name,'spyPoll']);return {ok:true,data:[],start:0,stop:false,nackIndex:-1,simulated:true};},
    send(){calls.push([name,'send']);return {ok:true,simulated:true};},
    receive(){calls.push([name,'receive']);return {ok:true,received:false,simulated:true};},
    clear(){calls.push([name,'clear']);return true;},
    readRegister(){calls.push([name,'readRegister']);return {ok:true,simulated:true,ack:0,ackName:'UNKNOWN',value:0,parityError:false};}
  },{get(obj,k){return k in obj?obj[k]:(k==='lastError'?'mock error':undefined);}});
  return {UART:make('UART'),SPI:make('SPI'),I2C:make('I2C'),CAN:make('CAN'),SWD:make('SWD'),
          simulation:true,stop(){calls.push(['root','stop']);return true;},lastError:''};
}
for(const id of exampleIds) {
  assert(catalog.some(e=>e.file===id),`not in catalog ${id}`);
  const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels',id+'.wfpanel'),'utf8'));
  const script=fs.readFileSync(path.join(root,'examples/source',id+'.js'),'utf8');
  assert.strictEqual(panel.script,script);
  assert.strictEqual(panel.pages.length,1);
  const controls=panel.pages[0].controls;
  assert(controls.some(x=>x.type==='event_log'));
  const callbacks=new Map(),values=new Map(),printed=[],calls=[];
  for(const ctrl of controls) {
    if(ctrl.type==='option_selector') {
      const options=ctrl.properties.options.split('|');
      values.set(ctrl.id,options[ctrl.properties.defaultIndex||0]);
    } else values.set(ctrl.id,ctrl.text);
  }
  const ui={on(id,event,cb){assert(controls.find(c=>c.id===id),`missing control ${id}`);callbacks.set(id,cb);},
            get(id){return values.get(id);},
            set(id,val){assert(controls.find(c=>c.id===id),`ui.set missing ${id}`);values.set(id,val);return true;}};
  const Protocol1=buildProtocol(calls);
  const context={ui,Protocol1,Protocol:Protocol1,print:(...args)=>printed.push(args.join(' ')),JSON,Number,String,Error,parseInt,isFinite};
  vm.runInNewContext(script,context,{filename:id+'.js',timeout:2000});
  for(const control of controls) for(const [event,binding] of Object.entries(control.events||{})) {
    assert.strictEqual(event,'clicked');
    assert.strictEqual(binding.enabled,true);
    assert(!callbacks.has(control.id),id+' duplicate ui.on for '+control.id);
    assert.strictEqual(typeof context[binding.handler],'function',id+' '+binding.handler);
    callbacks.set(control.id,context[binding.handler]);
  }
  assert.strictEqual(calls.length,0,id+' drove hardware before operator action');
  const buttons=controls.filter(c=>c.type==='button');
  assert(buttons.length>=4,id+' missing buttons');
  for(const button of buttons) {
    const cb=callbacks.get(button.id);
    assert.strictEqual(typeof cb,'function',id+' missing button event '+button.id);
    cb();
  }
  assert(calls.some(c=>c[1]==='configure'),id+' missing configuration');
  assert(calls.some(c=>c[0]==='root'&&c[1]==='stop'),id+' missing release');
  assert(printed.length>=4,id+' no operation log');
  const log=controls.find(c=>c.type==='event_log');
  assert(values.get(log.id).append.message,id+' failed EventLog append');
  console.log('PASS',id,buttons.length+' events',printed.length+' logs');
}
console.log('PASS: 5 protocol example scripts and Panel UI bindings with mock native API');
