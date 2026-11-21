// Pure Node control-flow harness for NEW RC1.3 Panel pages. Not C++/Qt validation.
'use strict';
const fs=require('fs'),os=require('os'),path=require('path'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(path.join(__dirname,'../development/binary_lab/NativeBinaryPerformanceLab.js'),'utf8');
const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'wfscript-binary-rc13-'));
function signed(x,bits){const mask=(1n<<BigInt(bits))-1n;x&=mask;return x&(1n<<BigInt(bits-1))?x-(1n<<BigInt(bits)):x;}
function bitIndex(start,step,order){if(order==='intel'||order==='little'||order==='le')return start+step;let v=start;for(let i=0;i<step;i++)v=(v%8===0)?v+15:v-1;return v;}
class Buf {
 constructor(data){this.data=Buffer.from(data);this.length=this.data.length;this.lastError='';}
 get(name,offset){if(offset<0 || offset+name[1]>this.length)throw Error('out of range');return this.data['read'+name[0]](offset);}
 write(name,offset,value){if(offset<0 || offset+name[1]>this.length){this.lastError='out of range';return false;}this.data['write'+name[0]](value,offset);return true;}
 readU8(o){return this.data.readUInt8(o);}writeU8(o,v){if(v<0||v>255||!Number.isInteger(v)){this.lastError='range';return false;}return this.write(['UInt8',1],o,v);}
 readI8(o){return this.data.readInt8(o);}writeI8(o,v){return this.write(['Int8',1],o,v);}
 readU16LE(o){return this.data.readUInt16LE(o);}readU16BE(o){return this.data.readUInt16BE(o);}
 readI16LE(o){return this.data.readInt16LE(o);}readI16BE(o){return this.data.readInt16BE(o);}
 readU32LE(o){return this.data.readUInt32LE(o);}readU32BE(o){return this.data.readUInt32BE(o);}
 readI32LE(o){return this.data.readInt32LE(o);}readI32BE(o){return this.data.readInt32BE(o);}
 readF32LE(o){return this.data.readFloatLE(o);}readF32BE(o){return this.data.readFloatBE(o);}
 readF64LE(o){return this.data.readDoubleLE(o);}readF64BE(o){return this.data.readDoubleBE(o);}
 writeU16LE(o,v){return this.write(['UInt16LE',2],o,v);}writeU16BE(o,v){return this.write(['UInt16BE',2],o,v);}
 writeI16LE(o,v){return this.write(['Int16LE',2],o,v);}writeI16BE(o,v){return this.write(['Int16BE',2],o,v);}
 writeU32LE(o,v){return this.write(['UInt32LE',4],o,v);}writeU32BE(o,v){return this.write(['UInt32BE',4],o,v);}
 writeI32LE(o,v){return this.write(['Int32LE',4],o,v);}writeI32BE(o,v){return this.write(['Int32BE',4],o,v);}
 writeF32LE(o,v){return this.write(['FloatLE',4],o,v);}writeF32BE(o,v){return this.write(['FloatBE',4],o,v);}
 writeF64LE(o,v){return this.write(['DoubleLE',8],o,v);}writeF64BE(o,v){return this.write(['DoubleBE',8],o,v);}
 writeU64LE(o,v){return this.write(['BigUInt64LE',8],o,BigInt(v));}writeU64BE(o,v){return this.write(['BigUInt64BE',8],o,BigInt(v));}
 writeI64LE(o,v){return this.write(['BigInt64LE',8],o,BigInt(v));}writeI64BE(o,v){return this.write(['BigInt64BE',8],o,BigInt(v));}
 readU64LE(o){return this.data.readBigUInt64LE(o).toString();}readU64BE(o){return this.data.readBigUInt64BE(o).toString();}
 readI64LE(o){return this.data.readBigInt64LE(o).toString();}readI64BE(o){return this.data.readBigInt64BE(o).toString();}
 writeUtf8(o,s){let buf=Buffer.from(s,'utf8');if(o+buf.length>this.length)return false;buf.copy(this.data,o);return true;}
 toUtf8(o,n){return this.data.subarray(o,o+n).toString('utf8');}
 toHex(o,n){return this.data.subarray(o,o+n).toString('hex');}
 toBase64(){return this.data.toString('base64');}
 slice(o,n){return new Buf(this.data.subarray(o,o+n));}
 fill(v,o,n){this.data.fill(v,o,o+n);return true;}
 sum32(){let sum=0;for(let v of this.data)sum=(sum+v)>>>0;return sum;}
 sum32Range(o,n){let s=0;for(let i=o;i<o+n;i++)s=(s+this.data[i])>>>0;return s;}
 crc16(){let c=65535;for(let v of this.data){c^=v;for(let j=0;j<8;j++)c=(c&1)?((c>>>1)^0xA001):(c>>>1);}return c;}
 crc32(){let c=0xffffffff;for(let v of this.data){c^=v;for(let j=0;j<8;j++)c=(c&1)?((c>>>1)^0xEDB88320):(c>>>1);}return (c^0xffffffff)>>>0;}
 indexOf(other,start){return this.data.indexOf(other.data,start);}
 equals(other){return this.data.equals(other.data);}
 readBits64(start,width,order,isSigned){return this.rawBits(start,width,order,isSigned).toString();}
 writeBits64(start,width,order,isSigned,decimal){return this.writeBits(start,width,order,isSigned,BigInt(decimal));}
 readBits(start,width,order,isSigned){return Number(this.rawBits(start,width,order,isSigned));}
 rawBits(start,width,order,isSigned){let x=0n;for(let k=0;k<width;k++){let bit=bitIndex(start,k,order);let v=BigInt((this.data[bit>>3]>>(bit&7))&1);if(order==='intel'||order==='little'||order==='le')x|=v<<BigInt(k);else x=(x<<1n)|v;}return isSigned?signed(x,width):x;}
 writeBits(start,width,order,isSigned,v){let x=BigInt(v),mask=(1n<<BigInt(width))-1n;x&=mask;for(let k=0;k<width;k++){let bit=bitIndex(start,k,order),v=(x>>BigInt((order==='intel'||order==='little'||order==='le')?k:width-1-k))&1n;this.data[bit>>3]=(this.data[bit>>3]&~(1<<(bit&7)))|(Number(v)<<(bit&7));}return true;}
 writeSignal(start,width,order,signed,factor,bias,physical){return this.writeBits(start,width,order,signed,Math.round((physical-bias)/factor));}
 readSignal(start,width,order,signed,factor,bias){return this.readBits(start,width,order,signed)*factor+bias;}
 statsU16LE(o,n){const val=[];for(let i=0;i<n;i++)val.push(this.readU16LE(o+i*2));return new Series(val).stats();}
 decodeSamples(type,o,n,scale,bias){const values=new Array(n);for(let i=0;i<n;i++)values[i]=this.readU16LE(o+2*i)*scale+bias;return new Series(values);}
 release(){}
}
class Series{
 constructor(v){this.values=v;this.length=v.length;}
 at(i){return this.values[i];}
 stats(){let min=Infinity,max=-Infinity,sum=0,sq=0;for(let v of this.values){min=Math.min(min,v);max=Math.max(max,v);sum+=v;sq+=v*v;}return{count:this.length,min,max,sum,mean:sum/this.length,rms:Math.sqrt(sq/this.length)};}
 minMaxBins(bins){const out=[];for(let i=0;i<bins;i++){const st=Math.floor(i*this.length/bins),en=Math.floor((i+1)*this.length/bins);let min=Infinity,max=-Infinity;for(let j=st;j<en;j++){min=Math.min(min,this.values[j]);max=Math.max(max,this.values[j]);}out.push({start:st,end:en,min,max});}return out;}
 release(){}
}
class Stream{
 constructor(limit){this.limit=limit;this.bytes=Buffer.alloc(0);this.lastError='';this.optional=false;}
 get pending(){return this.bytes.length;}
 append(chunk){this.bytes=Buffer.concat([this.bytes,chunk.data]);if(this.optional){if(this.bytes[0]===10)this.bytes=this.bytes.subarray(1);else if(this.bytes[0]===13&&this.bytes[1]===10)this.bytes=this.bytes.subarray(2);this.optional=false;}return this.bytes.length<=this.limit;}
 readLine(){let idx=this.bytes.indexOf(10);if(idx<0)return null;let v=this.bytes.subarray(0,idx);this.bytes=this.bytes.subarray(idx+1);if(v[v.length-1]===13)v=v.subarray(0,v.length-1);return v.toString('utf8');}
 readIEEE(){if(this.bytes.length<2)return null;let nd=this.bytes[1]-48;if(this.bytes.length<2+nd)return null;let len=Number(this.bytes.subarray(2,2+nd).toString('ascii'));if(this.bytes.length<2+nd+len)return null;let ret=new Buf(this.bytes.subarray(2+nd,2+nd+len));this.bytes=this.bytes.subarray(2+nd+len);this.optional=true;if(this.bytes[0]===10){this.bytes=this.bytes.subarray(1);this.optional=false;}return ret;}
 readLengthPrefixed(n,order,includePrefix){if(this.bytes.length<n)return null;let len=n===2?(order==='little'?this.bytes.readUInt16LE(0):this.bytes.readUInt16BE(0)):this.bytes[0];if(this.bytes.length<n+len)return null;let ret=new Buf(this.bytes.subarray(includePrefix?0:n,n+len));this.bytes=this.bytes.subarray(n+len);return ret;}
 readUntil(delimiter,includeDelimiter){let idx=this.bytes.indexOf(delimiter.data);if(idx<0)return null;let end=idx+delimiter.length;let ret=new Buf(this.bytes.subarray(0,includeDelimiter?end:idx));this.bytes=this.bytes.subarray(end);return ret;}
 readFixed(n){if(this.bytes.length<n)return null;let ret=new Buf(this.bytes.subarray(0,n));this.bytes=this.bytes.subarray(n);return ret;}
 clear(){this.bytes=Buffer.alloc(0);this.optional=false;}
}
const Binary={
 clockUs:()=>Number(process.hrtime.bigint()/1000n),create:n=>new Buf(Buffer.alloc(n)),
 pattern:n=>new Buf(Buffer.from(Array.from({length:n},(_,i)=>(i*73+19)&255))),
 wordPattern:n=>{let b=Buffer.alloc(n*2);for(let i=0;i<n;i++)b.writeUInt16LE((i*73+1519)&65535,2*i);return new Buf(b);},
 fromHex:s=>new Buf(Buffer.from(s.replace(/\s/g,''),'hex')),
 fromUtf8:s=>new Buf(Buffer.from(s,'utf8')),
 fromBase64:s=>new Buf(Buffer.from(s,'base64')),
 concat:arr=>new Buf(Buffer.concat(arr.map(b=>b.data))),
 stream:n=>new Stream(n),
 wrapIEEE:b=>new Buf(Buffer.concat([Buffer.from('#'+String(String(b.length).length)+b.length),b.data,Buffer.from('\n')]))
};
const uiValues={};const ui={get(id){return uiValues[id];},set(id,value){uiValues[id]=value;}};
const FileIO={dataPath(name){return path.join(tmp,name);},writeBuffer(p,b){fs.writeFileSync(p,b.data);return true;},appendBuffer(p,b){fs.appendFileSync(p,b.data);return true;},readBuffer(p){return new Buf(fs.readFileSync(p));}};
const ctx={Binary,FileIO,ui,print:()=>{},console:{log:()=>{}}};
vm.createContext(ctx);vm.runInContext(source,ctx,{filename:'NativeBinaryPerformanceLab.js'});
try{
 ctx.onInitialize();
 for(const [f,key] of [['runCodec','codecResults'],['runFileBuffer','fileBufferResults'],['runFrames','frameResults'],['runCan','canResults'],['runSampleSeries','sampleResults']]){
     ctx[f]();const result=uiValues[key];assert(result.startsWith('PASS:'),f+': '+result);
     process.stdout.write('PASS RC1.3 Panel '+f+'\n');
 }
 assert(fs.existsSync(path.join(tmp,'binary_rc13_roundtrip.bin')),'actual disk fixture exists');
} finally {fs.rmSync(tmp,{recursive:true,force:true});}
