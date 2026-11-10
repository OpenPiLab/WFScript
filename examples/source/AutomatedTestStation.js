/**
 * OPERATOR PANEL — four jobs only: Run, Next, Stop/Reset, Display.
 *
 * The C++ runtime has ALREADY loaded the saved project test definition
 * and resolved TestSteps.mjs BEFORE this script is evaluated.
 *
 * To edit timing, limits, step order or handlers: Panel Designer > Test Sequence.
 * To change what a measurement does: edit TestSteps.mjs.
 * There is no TestRunner.loadDefinition(), add() or configure() here.
 */

function operatorAction(action) {
    if (!action()) {
        const message='Executive: '+TestRunner.lastError;
        print(message);
        ui.set('uutFooter',message);
    } else {
        ui.set('uutFooter','Test action accepted — inspect the Test Station and Run Report pages.');
    }
}

// Button IDs correspond directly to buttons on Page 01.
ui.on('stationPass', 'clicked', function() { operatorAction(function() { return TestRunner.run(); }); });
// Native Run Information widget supplies the fields to C++ automatically.
// This button runs the very same project-owned sequence, with no metadata script.
ui.on('uutRun', 'clicked', function() { operatorAction(function() { return TestRunner.run(); }); });
ui.on('uutReset','clicked', function() { operatorAction(function() { return TestRunner.reset(); }); refreshReport(); });
ui.on('stationNext', 'clicked', function() { operatorAction(function() { return TestRunner.next(); }); });
ui.on('stationFail', 'clicked', function() { operatorAction(function() { return TestRunner.terminate(); }); });
ui.on('stationReset', 'clicked', function() { operatorAction(function() { return TestRunner.reset(); }); refreshReport(); });

function escapeText(value) {
    return String(value === undefined || value === null ? '' : value)
        .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function refreshReport() {
    const r = TestRunner.report();
    const s = TestRunner.summary();
    const steps = r.steps || [];
    const measurements = r.measurements || [];
    const runInfo = r.runInfo || {};
    const archive = r.executionId ? TestRunner.archiveInfo() : {status:'idle'};
    const archiveText = archive.status === 'saved'
        ? 'AUTO-ARCHIVED JSON: ' + archive.path
        : archive.status === 'error'
            ? 'ARCHIVE FAILED: ' + archive.error
            : archive.status === 'disabled'
                ? 'Automatic archiving is disabled in Test Sequence > Report Storage.'
                : 'Archive status: ' + archive.status;
    if (r.executionId) ui.set('reportFooter', archiveText);
    const metadata = Object.keys(runInfo).map(function(key) {
        return '<tr><td>' + escapeText(key) + '</td><td>' + escapeText(runInfo[key]) + '</td></tr>';
    }).join('');
    const rows = steps.map(function(step) {
        return '<tr><td>' + escapeText(step.name) + '</td><td>' +
            escapeText(step.status) + '</td><td>' + escapeText(step.elapsedMs) + ' ms</td></tr>';
    }).join('');
    const data = measurements.map(function(m) {
        return '<tr><td>' + escapeText(m.name) + '</td><td>' +
            escapeText(m.value) + ' ' + escapeText(m.unit) + '</td><td>' +
            escapeText(m.low) + ' – ' + escapeText(m.high) + '</td><td>' +
            escapeText(m.status) + '</td></tr>';
    }).join('');
    ui.set('stationReport', '<div style="font-family:Segoe UI;color:#d9e8f4;font-size:12px">' +
        '<h2>Native Test Report</h2><p><b>Status:</b> ' + escapeText(s.state) +
        ' &nbsp; <b>Configured deadline:</b> ' + escapeText(s.totalTimeoutMs) + ' ms' +
        ' &nbsp; <b>Elapsed:</b> ' + escapeText(s.elapsedMs) + ' ms</p>' +
        '<p><b>Execution ID:</b> ' + escapeText(r.executionId || 'Not yet started') + '</p>' +
        '<p><b>Archive:</b> ' + escapeText(archiveText) + '</p>' +
        '<h3>Run Information</h3><table width="100%" cellpadding="4"><tr><th align="left">Field</th><th align="left">Value</th></tr>' + metadata + '</table>' +
        '<h3>Steps</h3><table width="100%" cellpadding="4"><tr><th align="left">Step</th><th>Status</th><th>Elapsed</th></tr>' + rows + '</table>' +
        '<h3>Measurements</h3><table width="100%" cellpadding="4"><tr><th align="left">Measurement</th><th>Actual</th><th>Limits</th><th>Verdict</th></tr>' + data + '</table></div>');
}

function saveReport(format) {
    const file = TestRunner.exportReport(format);
    const message = file ? 'Saved ' + format.toUpperCase() + ': ' + file : 'Cannot export: ' + TestRunner.lastError;
    ui.set('reportFooter', escapeText(message));
    print(message);
}
ui.on('reportRefresh','clicked', refreshReport);
ui.on('reportExportJson','clicked',function() { saveReport('json'); });
ui.on('reportExportCsv','clicked',function() { saveReport('csv'); });


// Page 06 demonstrates the project-owned pending model and two synchronized
// form widgets. This is READ-ONLY JavaScript; it never configures or copies
// UUT data. The C++ model owns both forms and the TestRunner start snapshot.
ui.on('mirrorRead','clicked',function() {
    const pending=TestRunner.pendingRunInfo();
    const mainView=ui.get('uutFields');
    const mirrorView=ui.get('uutFieldsMirror');
    const completed=TestRunner.runInfo();
    const message='Pending UUT: '+JSON.stringify(pending)+
        '\nPage 01 view: '+JSON.stringify(mainView)+
        '\nPage 06 view: '+JSON.stringify(mirrorView)+
        '\nFrozen executed UUT: '+JSON.stringify(completed);
    print(message);
    ui.set('mirrorStatus',message);
});

TestRunner.onFinished(function() { refreshReport(); });
refreshReport(); // Reading state is safe; no test runs at Panel startup.
