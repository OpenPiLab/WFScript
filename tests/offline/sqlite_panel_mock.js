// Offline JavaScript regression for the actual SQLiteDataLab panel script.
// Runs with Node's embedded SQLite to validate asynchronous callback chaining.
const fs=require('fs'), vm=require('vm'), os=require('os'), path=require('path');
const {DatabaseSync}=require('node:sqlite');
const root=path.resolve(__dirname,'../..');
const script=fs.readFileSync(path.join(root,'examples/source/SQLiteDataLab.js'),'utf8');
const temp=fs.mkdtempSync(path.join(os.tmpdir(),'wfs-sqlite-'));
const files=new Map(), connections=new Map(),events=new Map(), values=new Map(), timers=new Map();
let handleId=1, timerId=1, err=null;const logs=[];
const ui={get:id=>values.get(id),set:(id,value)=>{if(value&&value.append)logs.push(value.append.message);else values.set(id,value);return true;},on:(id,name,fn)=>events.set(id+'::'+name,fn)};
values.set('filterMinVoltage',3.31);
const respond=(callback,value)=>setImmediate(()=>{try{callback(value)}catch(e){err=e;}});
const Database={
  open:(name,cb)=>{try { const file=path.join(temp,name); let db=new DatabaseSync(file);let id=handleId++; connections.set(id,db);files.set(name,file);respond(cb,{ok:true,db:id,path:file});}catch(e){respond(cb,{ok:false,error:e.message})}return 1;},
  exec:(h,sql,params,cb)=>{if(typeof params==='function'){cb=params;params=[];}try{if(/^\s*ATTACH/i.test(sql))throw Error('Database.exec restricted SQL');let result=connections.get(h).prepare(sql).run(...params);respond(cb,{ok:true,changes:Number(result.changes),insertId:Number(result.lastInsertRowid)});}catch(e){respond(cb,{ok:false,error:e.message})}return 1;},
  query:(h,sql,params,cb)=>{if(typeof params==='function'){cb=params;params=[];}try{const stmt=connections.get(h).prepare(sql);const columns=stmt.columns().map(x=>x.name);const objects=stmt.all(...params);const rows=objects.map(r=>columns.map(c=>typeof r[c]==='bigint'?Number(r[c]):r[c]));respond(cb,{ok:true,columns,rows,rowCount:rows.length});}catch(e){respond(cb,{ok:false,error:e.message})}return 1;},
  close:(h,cb)=>{try{connections.get(h).close();connections.delete(h);respond(cb,{ok:true});}catch(e){respond(cb,{ok:false,error:e.message})}return 1;}
};
const context={ui,Database,Date,Math,Number,JSON,Error,String,Number,print:(...x)=>logs.push(x.join(' ')),
  setInterval:fn=>{const id=timerId++;timers.set(id,fn);return id},clearInterval:id=>timers.delete(id)};
vm.createContext(context);
vm.runInContext(script,context,{filename:'SQLiteDataLab.js'});
// Saved Property Editor bindings are dispatched by PanelRuntime, not ui.on().
const panel=JSON.parse(fs.readFileSync(path.join(root,'examples/panels/SQLiteDataLab.wfpanel'),'utf8'));
for(const page of panel.pages)for(const control of page.controls){
 for(const [event,b] of Object.entries(control.events||{})){
  const key=control.id+'::'+event;
  if(events.has(key))throw Error('Duplicate manual and designer event '+key);
  if(!b.enabled || typeof context[b.handler]!=='function')throw Error('Invalid handler '+key);
  events.set(key,value=>context[b.handler](value));
 }
}
if(events.size!==8)throw Error('Expected 8 visual SQLite events');

function assert(cond,msg){if(!cond)throw Error(msg);}
const sleep=ms=>new Promise(ok=>setTimeout(ok,ms));
async function flush(){for(let i=0;i<5;i++)await sleep(10);if(err)throw err;}
async function main(){
 context.onInitialize();
 assert(connections.size===0,'opened a database automatically on startup');
 events.get('openSqlDb::clicked')();await flush();
 assert(connections.size===1,'cannot open/create table');
 events.get('startSqlLog::clicked')();
 assert(timers.size===1,'start logging did not schedule timer');
 for(let i=0;i<12;i++){[...timers.values()][0]();await flush()}
 assert(values.get('sampleCount')===12,'insert callbacks did not complete');
 events.get('querySql::clicked')();await flush();
 const rows=values.get('recentSamplesTable');
 assert(Array.isArray(rows)&&rows.length===12&&Array.isArray(rows[0])&&rows[0].length===3,'query rows not compatible with Experiment Data Table');
 events.get('queryStats::clicked')();await flush();
 assert(values.get('totalRecords')===12,'aggregate count wrong');
 events.get('filterSql::clicked')();await flush();
 assert(Array.isArray(values.get('recentSamplesTable')),'filter query failed');
 events.get('invalidSql::clicked')();await flush();
 assert(logs.some(x=>x.includes('Expected restriction')),'rejected unsafe SQL not reported');
 events.get('stopSqlLog::clicked')();assert(timers.size===0,'timer not stopped');
 events.get('closeSqlDb::clicked')();await flush();assert(connections.size===0,'database not closed');
 events.get('openSqlDb::clicked')();await flush();
 assert(values.get('totalRecords')===12,'SQLite data not persistent after reopen');
 context.onShutdown();
 for(const db of connections.values())db.close();
 fs.rmSync(temp,{recursive:true,force:true});
 console.log('PASS: SQLiteDataLab JavaScript, button events, 12 asynchronous inserts, persistence, table rows, aggregates, filter, SQL rejection and timer stop');
}
main().catch(e=>{console.error('FAIL',e,logs.slice(-10));process.exitCode=1});
