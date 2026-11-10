import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const source=fs.readFileSync(path.join(root,'examples/source/AD3HardwareTestStation.js'),'utf8');
const handlers=new Map(); const fields=new Map();
let runs=0,resets=0,terminations=0,exports=0,finished;
let status='idle';
const ui={on(id,event,handler){handlers.set(id+':'+event,handler)},set(id,val){fields.set(id,val)},get(){return null}};
const TestRunner={lastError:'',run(){runs++;status='running';return true},
 terminate(){terminations++;status='aborted';return true},
 reset(){resets++;status='idle';return true},
 onFinished(fn){finished=fn},
 summary(){return {state:status,elapsedMs:1245}},
 report(){return {executionId:runs>0?'run-001':'',instrument:{mode:'hardware'},steps:[],measurements:[],runInfo:{uutId:'TEST1'}}},
 archiveInfo(){return {status:'saved',path:'C:/test/reports/run-001.json'}},
 exportReport(kind){exports++;return 'C:/test/reports/export.'+kind}}
vm.runInNewContext(source,{ui,TestRunner,print(){},String,Object}, {filename:'AD3HardwareTestStation.js'});
for(const id of ['uutRun','stationRun','stationStop','uutReset','stationReset','stationReportButton','reportRefresh','reportExportJson','reportExportCsv'])assert.ok(handlers.has(id+':clicked'),id);
assert.equal(runs,0,'Startup must not automatically run a test');
handlers.get('uutRun:clicked')();assert.equal(runs,1);
status='passed';finished();
assert.match(fields.get('stationReport'),/run-001/);
assert.match(fields.get('stationReport'),/C:\/test\/reports/);
handlers.get('stationStop:clicked')();assert.equal(terminations,1);
handlers.get('stationReset:clicked')();assert.equal(resets,1);
handlers.get('reportExportJson:clicked')();handlers.get('reportExportCsv:clicked')();assert.equal(exports,2);
console.log('PASS v5.6.68 operator event smoke: no startup run, native run/stop/reset, report/archiving, JSON/CSV');
