// Offline regression of NativeBinaryPerformanceLab's localhost TCP lifecycle.
// Simulates QTcpSocket RemoteHostClosedError after intentional client closure.
// This is NOT a real native Qt TCP test; Windows testing remains required.
'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm');
const source=fs.readFileSync(path.join(__dirname,'../development/binary_lab/NativeBinaryPerformanceLab.js'),'utf8');
const pause=t=>new Promise(resolve=>setTimeout(resolve,t));
class Buf {
    constructor(bytes){this.data=Uint8Array.from(bytes);this.length=this.data.length;}
    slice(offset,len){return new Buf(this.data.slice(offset,offset+len));}
    readU16LE(pos){return this.data[pos]+(this.data[pos+1]<<8);}
    release(){}
}
function makeContext(earlyClose=false){
    const logs=[],values={}; let server=null, clients=0;
    const ctx={print:msg=>logs.push(msg),ui:{get:()=>undefined,set:(k,v)=>{values[k]=v;}},isFinite,Math,Number,String,Error,Date,Array,parseFloat};
    ctx.Binary={
        clockUs:()=>Number(process.hrtime.bigint()/1000n),
        wordPattern:(count)=>{
            const b=new Uint8Array(count*2);
            for(let i=0;i<count;i++){const word=(i*73+1519)&65535;b[2*i]=word&255;b[2*i+1]=word>>>8;}
            return new Buf(b);
        },
        wrapIEEE:(buf)=>{
            const length=String(buf.length),prefix='#'+length.length+length;
            return new Buf(Buffer.concat([Buffer.from(prefix),Buffer.from(buf.data),Buffer.from('\n')]));
        },
        stream:(max)=>{
            const parts=[];let all=[];
            return {
                lastError:'',pending:0,
                append(chunk){all.push(...chunk.data);if(all.length>max){this.lastError='overflow';return false;}return true;},
                readIEEE(){
                    if(all.length<2)return null;
                    const lenDigits=all[1]-48;if(all.length<2+lenDigits)return null;
                    const length=Number(String.fromCharCode(...all.slice(2,2+lenDigits)));
                    if(all.length<2+lenDigits+length)return null;
                    const result=new Buf(all.slice(2+lenDigits,2+lenDigits+length));all=all.slice(2+lenDigits+length+1);return result;
                },clear(){all=[];}
            };
        }
    };
    class Endpoint{
        constructor(kind,host=null){this.kind=kind;this.host=host;this.events={};this.valid=true;this.legacyEvents=0;this.nativeEvents=0;this.bytesWritten=0;}
        on(ev,fn){this.events[ev]=fn;return this;}
        emit(ev,arg){if(this.events[ev])this.events[ev](arg);}
        info(){return {valid:this.valid,localPort:45234,legacyEvents:this.legacyEvents,nativeEvents:this.nativeEvents};}
        close(){
            if(!this.valid)return;this.valid=false;
            if(this.kind==='client'){
                const peer=this.peer;
                setImmediate(()=>{if(peer && peer.valid){peer.emit('error','The remote host closed the connection');peer.emit('close');}});
            }
            this.emit('close');
        }
        write(payload){
            const b=payload instanceof Buf?payload.data:Uint8Array.from(payload);
            this.bytesWritten+=b.length;
            if(this.kind==='client'){
                const peer=this.peer;
                setImmediate(()=>{
                    if(!this.valid || !peer.valid)return;
                    peer.emit('binary',new Buf(b));
                    if(earlyClose && clients===1 && peer.bytesWritten<200009){
                        this.close();
                    }
                });
            }else if(this.kind==='peer'){
                const client=this.host;
                setImmediate(()=>{
                    if(!this.valid||!client.valid)return;
                    if(client.events.data){client.legacyEvents++;client.emit('data',Array.from(b));}
                    if(client.events.binary){client.nativeEvents++;client.emit('binary',new Buf(b));}
                });
            }
            return b.length;
        }
        connect(){
            const client=this;
            setImmediate(()=>{
                if(!client.valid)return;
                const peer=new Endpoint('peer',client);client.peer=peer;
                server.emit('connection',peer);
                client.emit('connect');
            });
        }
        listen(){setImmediate(()=>this.emit('listening'));}
    }
    ctx.TCP={
        createServer:()=>{server=new Endpoint('server');return server;},
        create:()=>{clients++;return new Endpoint('client');}
    };
    vm.createContext(ctx);vm.runInContext(source,ctx,{filename:'NativeBinaryPerformanceLab.js'});
    return {ctx,logs,values};
}
async function run(){
    let c=makeContext();c.ctx.runTcpLoopback();await pause(120);
    if(!c.values.tcpResults?.includes('Both paths verified full payload')){
        throw Error('normal close regression: '+c.logs.join('\n'));
    }
    if(c.logs.some(x=>x.includes('TCP test ERROR'))){
        throw Error('intentional close wrongly flagged as error: '+c.logs.join('\n'));
    }
    c.ctx.runTcpLoopback();await pause(120);
    if(c.logs.filter(x=>x.includes('Local TCP loopback PASS')).length!==2){
        throw Error('repeat regression: '+c.logs.join('\n'));
    }
    c=makeContext(true);c.ctx.runTcpLoopback();await pause(120);
    if(c.logs.filter(x=>x.includes('Local TCP test ERROR')).length!==1){
        throw Error('premature close not flagged exactly once: '+c.logs.join('\n'));
    }
    if(c.logs.some(x=>x.includes('Local TCP loopback PASS'))){
        throw Error('premature close produced false pass: '+c.logs.join('\n'));
    }
    console.log('PASS native binary Panel TCP lifecycle mock: intentional close ignored, two repeated successes, premature close fails exactly once');
}
run().catch(err=>{console.error(err);process.exitCode=1;});
