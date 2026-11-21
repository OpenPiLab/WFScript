// Node-only harness for Panel SCRIPT correctness. NOT a native performance
// measurement and does not execute Qt C++ or physical TCP code.
'use strict';
const fs=require('fs');const vm=require('vm');const assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../development/binary_lab/NativeBinaryPerformanceLab.js'),'utf8');
class BufferStub {
    constructor(bytes,offset=0,len=-1){this.data=bytes;this.offset=offset;this.length=len<0?bytes.length-offset:len;this.lastError='';}
    readU8(i){return this.data[this.offset+i];}
    readU16LE(i){return this.readU8(i)+256*this.readU8(i+1);}
    readU16BE(i){return this.readU8(i)*256+this.readU8(i+1);}
    readF32LE(i){let b=Buffer.from(this.data.slice(this.offset+i,this.offset+i+4));return b.readFloatLE(0);}
    toHex(i,n){return Buffer.from(this.data.slice(this.offset+i,this.offset+i+n)).toString('hex');}
    slice(i,n){return new BufferStub(this.data,this.offset+i,n);}
    ieeePayload(){const d=this.data;const o=this.offset;assert(d[o]===35);let digits=d[o+1]-48,len=0;for(let i=0;i<digits;i++)len=len*10+d[o+i+2]-48;return this.slice(2+digits,len);}
    statsU16LE(o,n){let min=65535,max=0,sum=0,sum2=0;for(let i=0;i<n;i++){let v=this.readU16LE(o+2*i);min=Math.min(min,v);max=Math.max(max,v);sum+=v;sum2+=v*v;}return{count:n,min,max,sum,mean:sum/n,rms:Math.sqrt(sum2/n)};}
    sum32(){let n=0;for(let i=0;i<this.length;i++)n=(n+this.readU8(i))>>>0;return n;}
    crc16(){let c=65535;for(let i=0;i<this.length;i++){c^=this.readU8(i);for(let j=0;j<8;j++)c=(c&1)?((c>>>1)^0xA001):(c>>>1);}return c;}
    release(){}
}
const Native={
    clockUs:()=>Number(process.hrtime.bigint()/1000n),
    pattern:(n)=>new BufferStub(Uint8Array.from({length:n},(_,i)=>(i*73+19)&255)),
    wordPattern:(n)=>{let b=new Uint8Array(n*2);for(let i=0;i<n;i++){let x=(i*73+1519)&65535;b[2*i]=x&255;b[2*i+1]=x>>>8;}return new BufferStub(b);},
    fromHex:(text)=>new BufferStub(Uint8Array.from(Buffer.from(text.replace(/\s+/g,''),'hex'))),
    wrapIEEE:(payload)=>{let d=String(payload.length),h='#'+d.length+d,b=new Uint8Array(h.length+payload.length+1);for(let i=0;i<h.length;i++)b[i]=h.charCodeAt(i);for(let i=0;i<payload.length;i++)b[h.length+i]=payload.readU8(i);b[b.length-1]=10;return new BufferStub(b);},
    stream:(max)=>{let chunks=[];return {lastError:'',append(buf){chunks.push(...buf.data.slice(buf.offset,buf.offset+buf.length));return chunks.length<=max;},readIEEE(){if(!chunks.length)return null;let n=chunks[1]-48;if(chunks.length<2+n)return null;let size=0;for(let i=0;i<n;i++)size=size*10+chunks[2+i]-48;if(chunks.length<2+n+size)return null;return new BufferStub(Uint8Array.from(chunks.slice(2+n,2+n+size)));},clear(){chunks=[];}};}
};
const labels={};let size='100,000';
const ctx={Binary:Native,ui:{get(id){return id==='pointSize'?size:'';},set(id,v){labels[id]=v;}},print(){},console:{log(){},error(){}}};
vm.createContext(ctx);vm.runInContext(source,ctx,{filename:'NativeBinaryPerformanceLab.js'});
ctx.onInitialize();
for(const newSize of ['100,000','1,000,000']){
    size=newSize;ctx.runScpi();
    assert.match(labels.scpiResults,/PASS/);
    assert(!labels.scpiResults.includes('FAILED'),labels.scpiResults);
    assert.match(labels.scpiResults,/SCPI WORD/);
    console.log('PASS: synthetic SCPI '+size+' including chunk assembly and WORD stats');
}
ctx.runProtocols();assert.match(labels.packetResults,/PASS/);assert(!labels.packetResults.includes('FAILED'));
console.log('PASS: checksum, CRC16, endian and float32 packet benchmarks');
ctx.clearResults();assert(labels.scpiResults==='Not run'&&labels.packetResults==='Not run');
console.log('PASS: result reset');
