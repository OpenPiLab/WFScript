/**
 * v0.2-dev.1 offline integration fixture (Node, no Qt or SDK).
 * Checks project serialization, 3-page bindings, no hardware auto-start,
 * manual/auto/callback transitions and native Test Executive definition.
 */
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=path.resolve(__dirname,'..');
const id='ProtocolUartAsync';
const doc=JSON.parse(fs.readFileSync(path.join(root,'examples/panels',id+'.wfpanel'),'utf8'));
const script=fs.readFileSync(path.join(root,'examples/source',id+'.js'),'utf8');
const library=fs.readFileSync(path.join(root,'examples/libraries/UARTEchoTestSteps.mjs'),'utf8');
assert.strictEqual(doc.format,'WFScriptPanel');assert.strictEqual(doc.version,2);
assert.strictEqual(doc.pages.length,3);assert.strictEqual(doc.script,script);
assert.deepStrictEqual(doc.libraries,[{name:'UARTEchoTestSteps.mjs',source:library}]);
assert.strictEqual(doc.testLibrary,'UARTEchoTestSteps.mjs');
assert.strictEqual(doc.testDefinition.steps.length,3);
assert.deepStrictEqual(doc.testDefinition.steps.map(s=>s.group),['setup','main','cleanup']);
assert.strictEqual(doc.testDefinition.steps[1].mode,'poll');
assert.deepStrictEqual(doc.testDefinition.steps[1].handlers,{start:'sendEchoTest',poll:'pollEchoTest'});
assert.strictEqual(doc.testDefinition.steps[1].resultWidget,'testEchoBytes');
const controls=doc.pages.flatMap(x=>x.controls);
assert.strictEqual(new Set(controls.map(x=>x.id)).size,controls.length);
const term=controls.find(x=>x.type==='uart_terminal');assert(term);
const actions=new Map(),values=new Map(),callLog=[];
for(const ctrl of controls)values.set(ctrl.id,ctrl.text);
const native={configured:false,auto:false,queue:[],cb:null,stats:{receivedBytes:0,readBytes:0,queueDroppedBytes:0,sdkOverflows:0,parityErrors:0},
 configure(){callLog.push('configure');this.configured=true;return true;},
 startRx(){callLog.push('startRx');return true;},
 startAutoRx(){callLog.push('startAutoRx');this.auto=true;return true;},
 stopAutoRx(){callLog.push('stopAutoRx');this.auto=false;return true;},
 onData(cb){callLog.push('onData');this.cb=cb;return true;},
 clearOnData(){callLog.push('clearOnData');this.cb=null;return true;},
 writeText(s){callLog.push('writeText:'+s);return {ok:true};},
 available(){return this.queue.length;},
 read(n){callLog.push('read');const data=this.queue.splice(0,n);return {ok:true,data,count:data.length,overflow:false,queued:this.auto};},
 stats(){return {...this.stats,autoReceiving:this.auto,available:this.available()};},
 lastError:''};
const protocol={UART:native,simulation:false,lastError:'',stop(){callLog.push('stop');native.configured=false;native.auto=false;native.cb=null;native.queue=[];return true;}};
const ui={on(id,event,cb){assert(controls.find(x=>x.id===id),id);actions.set(id+'::'+event,cb);},
 set(id,value){assert(controls.find(x=>x.id===id),id);values.set(id,value);return true;},get(id){return values.get(id);}};
const TestRunner={lastError:'',run(){callLog.push('testRun');return true;},terminate(){callLog.push('terminate');return true;},reset(){callLog.push('testReset');return true;},summary(){return {state:'idle',passed:0,failed:0};},report(){return {measurements:[]};},onFinished(fn){this.fn=fn;}};
const env={ui,Protocol1:protocol,TestRunner,print:()=>{},JSON,Number,String,Error,isFinite};
vm.runInNewContext(script,env,{filename:id+'.js',timeout:2000});
assert.strictEqual(actions.size,0,'no manual UI handlers');
for(const ctrl of controls) for(const [event,b] of Object.entries(ctrl.events||{})) {
    assert(b.enabled && typeof env[b.handler]==='function',ctrl.id+'/'+event);
    const key=ctrl.id+'::'+event;
    assert(!actions.has(key),key+' duplicated');
    actions.set(key,env[b.handler]);
}
assert.strictEqual(actions.size,19,'all UART async widget actions converted');
assert.strictEqual(callLog.length,0,'no hardware activity on Panel startup');
const press=(id,event='clicked',value)=>{const cb=actions.get(id+'::'+event);assert.strictEqual(typeof cb,'function',id);cb(value);};
press('asyncConfigure');assert(native.auto);assert.deepStrictEqual(callLog.slice(0,3),['stop','configure','startAutoRx']);
press('uartView','sendRequested','HELLO');assert(callLog.includes('writeText:HELLO\n'));
press('asyncStopTimer');assert(!native.auto);press('manualConfigure');assert(callLog.includes('startRx'));
press('asyncRead');press('asyncRelease');
press('callbackStart');assert(native.auto && typeof native.cb==='function');
native.cb({count:4,text:'PING',overflow:false,timestampSource:'HostPoll'});
assert(String(values.get('callbackCount')).includes('4 bytes'));
press('callbackDisable');assert(native.cb===null);
press('callbackSend');press('callbackDrain');press('callbackRelease');
press('testRun');press('testStop');press('testReset');press('testReport');
assert(callLog.includes('testRun') && callLog.includes('terminate'));
const mod=library.replaceAll('export function ','function ');
const ctx={Protocol1:protocol,print:()=>{},Error,JSON,isFinite};
vm.createContext(ctx);vm.runInContext(mod,ctx,{timeout:2000});
for(const name of ['setupUartEcho','sendEchoTest','pollEchoTest','cleanupUartEcho'])
 assert.strictEqual(typeof ctx[name],'function','missing exported '+name);
ctx.setupUartEcho();ctx.sendEchoTest();assert.strictEqual(ctx.pollEchoTest(),undefined);
native.queue=[84,69,83,84,10];assert.strictEqual(ctx.pollEchoTest(),5);
ctx.cleanupUartEcho();
console.log('PASS: 3 pages, callback, widget binding, test definition, module and safe startup');
