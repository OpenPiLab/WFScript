import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import * as Core from '../libraries/WFScpiCore.mjs';
import * as Scope from '../libraries/RigolMHO900.mjs';
import * as Gen from '../libraries/RigolDG900Pro.mjs';
import * as Meter from '../libraries/RigolDM858.mjs';
import * as Psu from '../libraries/RigolDP900.mjs';
const panel=JSON.parse(fs.readFileSync(new URL('../Rigol_Four_Instrument_Operator_Bench_v0_6_4_Native_Charts.wfpanel',import.meta.url)));
const state={};const plotted=[];const saved={};const logs=[];
for(const page of panel.pages)for(const c of page.controls){
  state[c.id]=c.type==='option_selector'?(c.properties.options||'').split('|')[c.properties.defaultIndex||0] :
    c.type==='toggle'?!!c.value : c.text;
}
const modules={load:name=>({'WFScpiCore.mjs':Core,'RigolMHO900.mjs':Scope,
 'RigolDG900Pro.mjs':Gen,'RigolDM858.mjs':Meter,'RigolDP900.mjs':Psu})[name]};
class NativeSamples{
 constructor(values){this.values=values;this.length=values.length;this.lastError='';this.released=false;}
 stats(){const a=this.values;let low=Infinity,hi=-Infinity,sum=0,sum2=0;
  for(const v of a){low=Math.min(low,v);hi=Math.max(hi,v);sum+=v;sum2+=v*v;}
  return {count:a.length,min:low,max:hi,rms:Math.sqrt(sum2/a.length)};}
 minMaxBins(bins){const a=this.values;return Array.from({length:Math.min(bins,a.length)},(_,i)=>{const n=Math.min(bins,a.length),start=Math.floor(i*a.length/n),end=Math.floor((i+1)*a.length/n);let lo=Infinity,hi=-Infinity;for(let j=start;j<end;j++){lo=Math.min(lo,a[j]);hi=Math.max(hi,a[j]);}return {start,end,min:lo,max:hi};});}
 release(){this.released=true;}
}
class BufferValue{
 constructor(bytes){this.b=Buffer.from(bytes);this.length=this.b.length;this.lastError='';}
 readU16LE(i){return this.b.readUInt16LE(i);}
 readU8(i){return this.b[i];}
 decodeSamples(type,off,count,scale,bias){
  const v=new Float64Array(count);
  for(let i=0;i<count;i++)v[i]=(type==='u16le'?this.b.readUInt16LE(off+i*2):this.b[off+i])*scale+bias;
  return new NativeSamples(v);
 }
 release(){}
}
const Binary={lastError:'',concat:chunks=>new BufferValue(Buffer.concat(chunks.map(c=>c.b)))};
const FileIO={lastError:'',dataPath:name=>'/test/'+name,writeBuffer:(path,b)=>{saved[path]=Buffer.from(b.b);return true;},
 writeJson:(path,json)=>{saved[path]=JSON.parse(JSON.stringify(json));return true;}};
const plots={setSeries:(id,name,x,y,opts)=>plotted.push({id,name,x,y,opts,native:false}),
 setNativeSamples:(id,name,samples,step,start,opts)=>{
   assert.equal(samples.released,false);
   plotted.push({id,name,len:samples.length,first:samples.values[0],last:samples.values.at(-1),
     step,start,opts,native:true});return true;
 },clear(){},configure(){}};
const ctx={ui:{get:id=>state[id],set:(id,v)=>{state[id]=v;}},Binary,FileIO,plots,modules,
 print:s=>logs.push(String(s)),TCP:{create:()=>{throw Error('no startup socket')}},
 Math,JSON,Number,Boolean,Array,Map,Set,String,Error,Date,Promise,setTimeout,clearTimeout,setInterval,clearInterval};
vm.createContext(ctx);vm.runInContext(panel.script,ctx);
assert.equal(ctx.lastFullRawRecord,null);
const p={xIncrement:2e-8,xOrigin:-.01,xReference:0,yIncrement:.001,yOrigin:32768,yReference:0};
const chunks=[];
for(let ch of [1,2,3,4]){
  const bytes=Buffer.alloc(4000);
  for(let i=0;i<2000;i++)bytes.writeUInt16LE(32768+ch*100+(i%80),i*2);
  chunks.push(bytes);
}
const record=ctx.makeFullRecord([1,2,3,4],2000,50000000,5);
for(let ch of [1,2,3,4]){
 const b=new BufferValue(chunks[ch-1]);
 const frame={points:2000,firstIndex:1,[`ch${ch}`]:{native:true,points:2000,rawBuffer:b,preamble:p}};
 record.addNative(ch,1,frame);
}
record.complete();record.plot();
assert.equal(plotted.length,4);assert(plotted.every(x=>x.native&&x.len===2000));
for(let ch of [1,2,3,4]){
  const row=plotted[ch-1];
  assert.equal(row.name,'CH'+ch);
  assert.equal(row.opts.color,['','#f5d23f','#40d6ef','#db6ad9','#70d68b'][ch]);
  assert.equal(row.step,2e-5);
  assert(Math.abs(row.start+10)<1e-8);
}
ctx.lastFullRawRecord=record;ctx.onScopeExportRaw();
for(let ch of [1,2,3,4]){
 const entries=Object.entries(saved).filter(([name])=>name.endsWith('_CH'+ch+'.bin'));
 assert.equal(entries.length,1);assert.deepEqual(entries[0][1],chunks[ch-1]);
}
// Fallback on older RC1.3.1 executable retains its 1700-point JS preview.
delete plots.setNativeSamples;
const fallback=ctx.makeFullRecord([1],2000,50000000,6);
fallback.addNative(1,1,{points:2000,firstIndex:1,ch1:{native:true,points:2000,rawBuffer:new BufferValue(chunks[0]),preamble:p}});
fallback.complete();fallback.plot();
assert(plotted.at(-1).native===false&&plotted.at(-1).x.length>0);
assert(logs.some(s=>s.includes('native-full-2000')));
console.log('PASS v0.6.4: full resolution native charts ×4, correct scale/time/color, lossless RAW, old runtime fallback');
