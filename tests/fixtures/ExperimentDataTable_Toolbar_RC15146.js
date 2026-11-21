// RC1.5.14.6 — Experiment DataTable toolbar + editable streaming stress-test Panel.
// Mimics batched TCP/UDP packets without a device. Export the TABLE CSV
// after the retained row count reaches 50,000 and has rolled for >=30 s.
// Timer timestamp is captured in JS, so the table can be checked for continuity.
// Table CSV has only a retained window, not an archival recording.
var streamTimers=[], flushTimer=0, statusTimer=0;
var running=false, seq=0, generated=0, sent=0, errors=0, runs=0;
var previous={id:0,id1:0,id2:0};
var queue=[];
var startedAt=0;

function requireTableReadApi() {
    if (typeof ui.tableRowCount === 'function' && typeof ui.tableSlice === 'function' && typeof ui.tableSelection === 'function' && typeof ui.tableSetStatus === 'function')
        return true;
    var message = 'Experiment DataTable Lab requires WFScript RC1.5.14.6 or newer: table APIs unavailable';
    print(message);
    ui.set('diagnostics',{append:{level:'ERROR',source:'DataTable',message:message}});
    return false;
}
function addDiagnostic(level, message) {
    ui.set('diagnostics',{append:{level:level,source:'DataTable',message:message}});
}
function makePacket(timerName, points) {
    if(!running)return;
    var now=Date.now();
    var delta=previous[timerName]?now-previous[timerName]:0;
    previous[timerName]=now;
    for(var k=0;k<points;++k){
        seq++;
        var v=3.300 + 0.135*Math.sin(seq/31) + 0.025*Math.sin(seq/9);
        queue.push([seq,now,timerName,Number(v.toFixed(5)),delta]);
    }
    generated+=points;
}
function flushQueue() {
    if(!queue.length)return;
    // Bounded GUI packets; no snapshots of all existing rows.
    var rows=queue.splice(0,Math.min(1000,queue.length));
    if(!ui.set('experimentTable',{appendRows:rows})) {
        errors++;
        addDiagnostic('ERROR','appendRows rejected, size='+rows.length);
        stopStream();
        return;
    }
    sent+=rows.length;
    if(sent%10000<rows.length)
        addDiagnostic('INFO','sent='+sent+' retained='+ui.tableRowCount('experimentTable')+' callbackEpochMs='+Date.now());
}
function refreshStatus() {
    var retained=ui.tableRowCount('experimentTable');
    var elapsed=startedAt?((Date.now()-startedAt)/1000).toFixed(1):'0.0';
    ui.tableSetStatus('experimentTable',(running?'STREAMING':'STOPPED')+' | '+retained+'/50000 rows | errors='+errors);
    ui.set('status',(running?'RUNNING':'STOPPED')+' | sent='+sent+' | retained='+retained+
        '/50000 | queued='+queue.length+' | errors='+errors+' | elapsed='+elapsed+' s');
}
function startStream() {
    if(!requireTableReadApi())return;
    if(running)return;
    runs++;running=true;startedAt=Date.now();
    previous={id:0,id1:0,id2:0};
    streamTimers.push(setInterval(function(){makePacket('id',20);},10));
    streamTimers.push(setInterval(function(){makePacket('id1',20);},20));
    streamTimers.push(setInterval(function(){makePacket('id2',12);},30));
    flushTimer=setInterval(flushQueue,50);
    statusTimer=setInterval(refreshStatus,250);
    addDiagnostic('INFO','RUN '+runs+' START | callbackEpochMs='+Date.now());
    refreshStatus();
}
function stopStream() {
    if(!running)return;
    running=false;
    for(var i=0;i<streamTimers.length;i++)clearInterval(streamTimers[i]);
    streamTimers=[];
    if(flushTimer){clearInterval(flushTimer);flushTimer=0;}
    if(statusTimer){clearInterval(statusTimer);statusTimer=0;}
    while(queue.length)flushQueue();
    addDiagnostic('PASS','STOP | sent='+sent+' retained='+ui.tableRowCount('experimentTable')+
        ' | callbackEpochMs='+Date.now());
    refreshStatus();
}
function clearTable() {
    if(!requireTableReadApi())return;
    stopStream();
    if(!ui.set('experimentTable',{clear:true})){
        addDiagnostic('ERROR','Cannot clear data table');return;
    }
    seq=0;sent=0;generated=0;errors=0;queue=[];
    addDiagnostic('INFO','CLEAR all retained data');
    refreshStatus();
}
function injectSample(){
    if(!requireTableReadApi())return;
    // Drain older queued rows first so global sequence order stays monotonic.
    while(queue.length)flushQueue();
    // Manual burst remains safe while running; appendRows has a 2048-row cap.
    var batch=[];var now=Date.now();
    for(var i=0;i<1000;i++){
        seq++;
        batch.push([seq,now,'manual',Number((3.2+Math.sin(seq/30)*0.1).toFixed(5)),0]);
    }
    if(ui.set('experimentTable',{appendRows:batch}))sent+=batch.length;
    else {errors++;addDiagnostic('ERROR','Manual burst rejected');}
    refreshStatus();
}
function checkWindow() {
    if(!requireTableReadApi())return;
    var n=ui.tableRowCount('experimentTable');
    var first=ui.tableSlice('experimentTable',0,1);
    var last=ui.tableSlice('experimentTable',Math.max(0,n-1),1);
    var ok=n>=1 && first && last && first.length==1 && last.length==1;
    var message='n='+n+' firstSeq='+(ok?first[0][0]:'none')+
        ' lastSeq='+(ok?last[0][0]:'none')+' sourceRuns='+runs;
    addDiagnostic(ok?'PASS':'WARN','Window check: '+message);
    print('Experiment window: '+message);
}
function onStartup(){
    if(!requireTableReadApi())return;
    ui.tableSetTitle('experimentTable','Experiment Measurements  |  Rolling 50,000');
    ui.set('experimentTable',{clear:true});
    addDiagnostic('INFO','Ready: stream ~3300 rows/s; 50k capacity in ~15 seconds');
    refreshStatus();
}
function onShutdown(){stopStream();}

// Lightweight events: do not request the full 50,000-row table on each edit.
function onExperimentCellEdited(edit) {
    addDiagnostic('PASS','Edited cell ['+edit.row+','+edit.column+'] from "'+edit.oldValue+'" to "'+edit.value+'"');
    if(!running)ui.tableSetStatus('experimentTable','Edited row '+edit.row+' column '+edit.column+'; press Export CSV to save');
}
function onExperimentSelectionChanged(selection) {
    // Selection changes can be frequent. Do not log every arrow-key movement.
    if(!running && selection.row>=0)
        ui.tableSetStatus('experimentTable','Selected cell ['+selection.row+','+selection.column+']  |  type to edit');
}
function onExperimentImportCompleted(result) {
    addDiagnostic(result.ok?'PASS':'ERROR','CSV import '+(result.ok?'OK':'FAILED')+
        ' rows='+result.rows+' path='+result.path+' '+result.error);
    if(result.ok){seq=0;sent=ui.tableRowCount('experimentTable');generated=sent;}
    refreshStatus();
}
function onExperimentExportCompleted(result) {
    addDiagnostic(result.ok?'PASS':'ERROR','CSV export '+(result.ok?'OK':'FAILED')+
        ' rows='+result.rows+' path='+result.path+' '+result.error);
}
function onExperimentCleared(info) {
    addDiagnostic('INFO','Cleared table from built-in toolbar (rowCount='+ui.tableRowCount('experimentTable')+')');
}
function showSelection() {
    var state=ui.tableSelection('experimentTable');
    var row=state && state.row>=0?ui.tableRow('experimentTable',state.row):null;
    var message='Selection '+JSON.stringify(state)+'  row='+JSON.stringify(row);
    addDiagnostic('INFO',message);
    print(message);
}
