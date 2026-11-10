/**
 * WFScript Example v5.6.61 — REUSABLE TEST DEFINITIONS
 *
 * Start with Page 00 Test Overview. Then run the six scenarios described there.
 * Wiring: None. Simulation only; AD3 and other hardware are not required.
 *
 * C++ TestRunnerBridge owns step order/state, real elapsed-time clocks,
 * deadlines, Cleanup, measured limits and a structured per-run report.
 * The data-only sequence lives in TestStationSequence.mjs, while executable
 * callbacks live in TestStationSteps.mjs (one shared QJSEngine).
 * TestRunner.loadDefinition() validates/binds the entire sequence in C++;
 * this Panel script controls operator buttons and renders native reports.
 *
 * C++ API guide: TestRunner.loadDefinition/definition/loadDefinitionFile/run/next/reset/abort/
 * terminate/measure/summary/report/setRunInfo/runInfo/setVariable/variable.
 */
const steps = modules.load('TestStationSteps.mjs');
const sequence = modules.load('TestStationSequence.mjs');
function requireRunner(ok) {
    if (!ok) throw Error('TestRunner: ' + TestRunner.lastError);
}
function safe(text) {
    return String(text===undefined || text===null ? '' : text)
      .replace(/&/g,'&amp;').replace(/</g,'&lt;')
      .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
const css = "font-family:'Segoe UI',sans-serif;font-size:12px;color:#d5e5f1;";
function statusColor(status) {
    return status==='pass'||status==='passed'?'#45e0a7':
           status==='done'?'#6ad4f5':
           status==='fail'||status==='failed'||status==='error'?'#ff7889':
           status==='aborted'||status==='skipped'?'#f4c978':'#a8c1d5';
}
function renderReport() {
    const r=TestRunner.report(), info=r.runInfo||{}, vars=r.variables||{};
    const valid=Boolean(r.executionId);
    const head=valid?'<span style="color:'+statusColor(r.state)+'"><b>'+safe(String(r.state).toUpperCase())+'</b></span>':'<span style="color:#a8c1d5">NO EXECUTION YET</span>';
    const fields=Object.keys(info).map(k=>safe(k)+': <b>'+safe(info[k])+'</b>').join(' &nbsp; | &nbsp; ');
    const rows=(r.steps||[]).map((s,i)=>'<tr><td>'+String(i+1)+'</td><td>'+safe(s.group).toUpperCase()+
        '</td><td>'+safe(s.name)+'</td><td>'+safe(s.mode)+' / '+safe(s.type)+'</td><td><b style="color:'+statusColor(s.status)+'">'+
        safe(s.status).toUpperCase()+'</b></td><td align="right">'+(s.elapsedMs>=0?s.elapsedMs+' ms':'—')+'</td></tr>').join('');
    const measurements=(r.measurements||[]).map(m=>'<tr><td>'+safe(m.name)+'</td><td>'+safe(m.value)+' '+safe(m.unit)+
        '</td><td>'+safe(m.low)+' ... '+safe(m.high)+'</td><td><b style="color:'+statusColor(m.status==='PASS'?'pass':'fail')+'">'+safe(m.status)+'</b></td></tr>').join('');
    const shared=Object.keys(vars).map(k=>safe(k)+' = '+safe(vars[k])).join('; ')||'No shared variables yet';
    const h='<div align="left" style="'+css+'"><h3 style="color:#e9f5ff">Native execution record &nbsp; '+head+'</h3>'+
        '<p><b>UUT / run metadata:</b> '+(fields||'Not configured')+'</p>'+
        '<p><b>Definition:</b> '+safe(r.definitionId||'Manual steps')+'<br><b>Run ID:</b> '+safe(r.executionId||'—')+'<br><b>Started (UTC):</b> '+safe(r.startedAtUtc||'—')+
        ' &nbsp; <b>Finished (UTC):</b> '+safe(r.finishedAtUtc||'—')+
        ' &nbsp; <b>Total time:</b> '+String(Math.round(r.elapsedMs||0))+' ms</p>'+
        '<p style="color:#94abc0"><b>Shared C++ variables:</b> '+shared+'</p>'+
        '<h4 style="color:#8bcbff">STEP RECORDS (Setup → Main → Cleanup)</h4>'+
        '<table width="100%" cellpadding="5" cellspacing="0" border="1" bordercolor="#36526c">'+
        '<tr bgcolor="#20354b"><th>#</th><th>Group</th><th>Step</th><th>Mode / type</th><th>Status</th><th>Time</th></tr>'+
        (rows||'<tr><td colspan="6">Run a test to populate the record.</td></tr>')+'</table>'+
        '<h4 style="color:#8bcbff">MEASUREMENT RESULTS</h4>'+
        '<table width="100%" cellpadding="5" cellspacing="0" border="1" bordercolor="#36526c">'+
        '<tr bgcolor="#20354b"><th>Measurement</th><th>Actual</th><th>Acceptance</th><th>Result</th></tr>'+
        (measurements||'<tr><td colspan="4">No measurements collected yet.</td></tr>')+'</table>'+
        '<p style="color:#92aabe"><i>Native execution record. Use the buttons above to export a completed run as JSON or CSV.</i></p></div>';
    ui.set('stationReport',h);
}
function refreshDiagnostics() {
    const s=TestRunner.summary();
    const message='<div align="left" style="'+css+'"><h3 style="color:'+statusColor(s.state)+'">'+safe(s.state).toUpperCase()+'</h3>'+
        '<p>Phase: <b>'+safe(s.phase)+'</b> &nbsp; Active: <b>'+safe(s.currentStepId||'none')+'</b></p>'+
        '<p>Elapsed: <b>'+Math.round(s.elapsedMs||0)+' ms</b> &nbsp; Step: <b>'+Math.round(s.stepElapsedMs||0)+' ms</b> &nbsp; Polls: <b>'+safe(s.pollCount||0)+'</b></p>'+
        '<p>Completed: '+safe(s.completedSteps)+' / '+safe(s.totalSteps)+' &nbsp; Skipped: '+safe(s.skippedSteps||0)+' &nbsp; Actions DONE: '+safe(s.completedActions||0)+'</p>'+
        '<p>Total deadline: '+safe(s.totalTimeoutMs||0)+' ms &nbsp; '+safe(s.lastError||'No executive error')+'</p>'+
        '</div>';
    ui.set('execDiagnostics',message);
}
requireRunner(TestRunner.loadDefinition(sequence.definition,steps));
requireRunner(TestRunner.onFinished(function(info) {
    renderReport();refreshDiagnostics();
    const report=TestRunner.report();
    print('C++ TEST EXECUTIVE:',info.state,'| UUT:',(report.runInfo||{}).uutId,
        '|',info.completedSteps+'/'+info.totalSteps,'steps |',info.elapsedMs,'ms',
        '| run ID:',report.executionId);
}));
// C++ loads and validates this complete definition as one transaction.
// No hand-written TestRunner.add() calls are needed in the operator Panel.
function registerSteps(settleMs) {
    requireRunner(TestRunner.loadDefinition(sequence.withSettle(settleMs),steps));
}
let simulatedUutCounter=0;
function prepareRun(fail,continueOnFailure,totalTimeoutMs,settleMs,scriptError) {
    if(TestRunner.state()==='running') throw Error('Finish, terminate or abort the active run first');
    // Rebuild the native step list so the long-delay demonstration uses the
    // same step procedures but a different Qt-owned settling duration.
    registerSteps(settleMs||150);
    requireRunner(TestRunner.configure({
        stopOnFailure:!continueOnFailure,totalTimeoutMs:totalTimeoutMs||5000
    }));
    simulatedUutCounter++;
    requireRunner(TestRunner.setRunInfo({
        uutId:'SIM-UUT-'+String(simulatedUutCounter).padStart(3,'0'),
        stationId:'WFScript-DEMO',operatorId:'DEMO',
        scenario:scriptError?'script-error':settleMs&&settleMs>=1500?'long-delay':
                 totalTimeoutMs&&totalTimeoutMs<150?'timeout':
                 continueOnFailure?'continue':fail?'pwm-fail':'nominal'
    }));
    steps.setPwmFailure(Boolean(fail));
    steps.setScriptError(Boolean(scriptError));
    renderReport();refreshDiagnostics();
}
function runStation(fail,continueOnFailure,totalTimeoutMs,settleMs,scriptError) {
    const current=TestRunner.summary();
    // Continue a sequence already advanced with NEXT STEP instead of
    // discarding its UUT identity and results. Only nominal continuation fits.
    if(current.state==='running' && !current.currentStepId &&
       !fail && !continueOnFailure && !settleMs && !scriptError && totalTimeoutMs===5000) {
        requireRunner(TestRunner.run());
        print('Continuing current TestRunner sequence after NEXT STEP');
        return;
    }
    prepareRun(fail,continueOnFailure,totalTimeoutMs,settleMs,scriptError);
    requireRunner(TestRunner.run());
    print('C++ test executive started; monitor Page 01/03/05.');
}
function safelyRun(action) {
    try {action();} catch(e){print('Operator action:',String(e));}
    refreshDiagnostics();
}
ui.on('stationNext','clicked',function(){safelyRun(function(){
    const s=TestRunner.summary();
    if(s.currentStepId) {print('Wait: timed step still active');return;}
    if(s.state==='idle')prepareRun(false,false,5000);
    requireRunner(TestRunner.next());
});});
ui.on('stationPass','clicked',function(){safelyRun(function(){runStation(false,false,5000);});});
ui.on('stationFail','clicked',function(){safelyRun(function(){runStation(true,false,5000);});});
ui.on('stationReset','clicked',function(){safelyRun(function(){
    requireRunner(TestRunner.reset());steps.setPwmFailure(false);steps.setScriptError(false);renderReport();
});});
ui.on('execContinue','clicked',function(){safelyRun(function(){runStation(true,true,5000);});});
ui.on('execTimeout','clicked',function(){safelyRun(function(){runStation(false,false,80);});});
ui.on('execLong','clicked',function(){safelyRun(function(){runStation(false,false,5000,2500);});});
ui.on('execError','clicked',function(){safelyRun(function(){runStation(false,false,5000,150,true);});});
ui.on('execTerminate','clicked',function(){safelyRun(function(){requireRunner(TestRunner.terminate());});});
ui.on('execAbort','clicked',function(){safelyRun(function(){requireRunner(TestRunner.abort());});});
ui.on('execRefresh','clicked',function(){renderReport();refreshDiagnostics();});
ui.on('reportRefresh','clicked',function(){renderReport();refreshDiagnostics();});
function exportStationReport(format) {
    const path=TestRunner.exportReport(format);
    if (!path) {
        ui.set('reportFooter','<div style="color:#ff899a;font-family:Segoe UI">Export ' +
            safe(format.toUpperCase()) + ' failed: '+safe(TestRunner.lastError)+'</div>');
        print('Report export failed:',TestRunner.lastError);
        return;
    }
    ui.set('reportFooter','<div style="color:#45e0a7;font-family:Segoe UI"><b>Saved '+
        safe(format.toUpperCase())+' report:</b> '+safe(path)+'</div>');
    print('TestRunner report saved:',path);
}
ui.on('reportExportJson','clicked',function(){safelyRun(function(){exportStationReport('json');});});
ui.on('reportExportCsv','clicked',function(){safelyRun(function(){exportStationReport('csv');});});

// A Panel never starts hardware or a test simply by loading the project.
renderReport();refreshDiagnostics();
