// ┌─ WFScript LEARNING LAB: AD3 Hardware Test Station — W1 to CH1
// │ Level: Advanced  |  Mode: hardware-capable
// │
// │ GOAL: Run a real W1→Scope CH1 loopback test with native limit checking and reports.
// │ BEFORE RUN: Physical wiring: W1→CH1+ and AD3 GND→CH1−. Verify voltage range and no other
// │    connected circuitry.
// │ TRY FIRST: Review Wiring & Guide in Design mode.
// │ TRACE: `uutRun` → `onUutRunClicked()` → `TestRunner.run()`; module
// │    `AD3LoopbackSteps.mjs` configures W1, captures CH1 and cleans up.
// │ EDIT SAFELY: Change only a saved acceptance limit in Test Sequence, then compare the
// │    reported verdict after a safe capture.
// │ REMEMBER: Test execution order, timeouts and limits belong to the project test
// │    definition, not to duplicate JavaScript configuration.
// │
// │ EVENT TIP: Select `uutRun` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/AD3HardwareTestStation.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * AD3 Hardware Test Station — operator script.
 * No test definition/configuration in this file. Native C++ reads project-owned
 * testDefinition, Run Information, Report Storage and AD3LoopbackSteps.mjs.
 * The MODULE operates the instrument; this script only handles UI/reporting.
 */
/**
 * CORE LESSON — Centralizes TestRunner operator actions and status messages. The project-owned
 * Test Sequence and AD3LoopbackSteps module—not this UI script—define measurement execution.
 */
function action(label, fn) {
    const ok = fn();
    const msg = ok ? label + ' accepted.' : label + ' rejected: ' + TestRunner.lastError;
    print('[AD3 operator] ' + msg);
    ui.set('uutFooter', msg);
}
// Designer event: select uutRun -> Properties -> Events -> clicked
function onUutRunClicked() {
    action('RUN TEST', function () {
        return TestRunner.run();
    });
}
// Designer event: select stationRun -> Properties -> Events -> clicked
function onStationRunClicked() {
    action('RUN TEST', function () {
        return TestRunner.run();
    });
}
// Designer event: select stationStop -> Properties -> Events -> clicked
function onStationStopClicked() {
    action('STOP (CLEANUP)', function () {
        return TestRunner.terminate();
    });
}
// Designer event: select uutReset -> Properties -> Events -> clicked
function onUutResetClicked() {
    action('RESET', function () {
        return TestRunner.reset();
    });
    refreshReport();
}
// Designer event: select stationReset -> Properties -> Events -> clicked
function onStationResetClicked() {
    action('RESET', function () {
        return TestRunner.reset();
    });
    refreshReport();
}
// Designer event: stationReportButton / clicked -> refreshReport (assigned in Properties -> Events).

function escapeHtml(value) {
    return String(value === undefined || value === null ? '' : value)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
function refreshReport() {
    const report = TestRunner.report();
    const summary = TestRunner.summary();
    const steps = report.steps || [],
        measurements = report.measurements || [];
    const archive = report.executionId ? TestRunner.archiveInfo() : { status: 'idle' };
    const mode = (report.instrument && report.instrument.mode) || 'not started';
    const archivetext = archive.status === 'saved' ? 'SAVED: ' + archive.path :
        archive.status === 'error' ? 'ARCHIVE ERROR: ' + archive.error :
            'Archive status: ' + archive.status;
    const uut = Object.keys(report.runInfo || {}).map(function (key) {
        return '<tr><td>' + escapeHtml(key) + '</td><td>' + escapeHtml(report.runInfo[key]) + '</td></tr>';
    }).join('');
    const stepRows = steps.map(function (s) {
        return '<tr><td>' + escapeHtml(s.name) + '</td><td>' + escapeHtml(s.status) + '</td><td>' + escapeHtml(s.elapsedMs) + ' ms</td></tr>';
    }).join('');
    const measureRows = measurements.map(function (m) {
        return '<tr><td>' + escapeHtml(m.name) + '</td><td>' + escapeHtml(m.value) + ' ' + escapeHtml(m.unit) +
            '</td><td>' + escapeHtml(m.low) + ' ... ' + escapeHtml(m.high) + '</td><td>' + escapeHtml(m.status) + '</td></tr>';
    }).join('');
    const html = '<div style="font-family:Segoe UI;color:#d9e8f4;font-size:12px">' +
        '<h2>AD3 Hardware Loopback — Native Test Report</h2>' +
        '<p><b>RESULT:</b> ' + escapeHtml(summary.state) + ' &nbsp; <b>Instrument:</b> ' + escapeHtml(mode) +
        ' &nbsp; <b>Elapsed:</b> ' + escapeHtml(summary.elapsedMs) + ' ms</p>' +
        '<p><b>Execution ID:</b> ' + escapeHtml(report.executionId || '(not run yet)') + '</p>' +
        '<p><b>Archive:</b> ' + escapeHtml(archivetext) + '</p>' +
        '<h3>Run Information</h3><table width="100%" cellpadding="4"><tr><th>Field</th><th>Value</th></tr>' + uut + '</table>' +
        '<h3>Setup / Main / Cleanup</h3><table width="100%" cellpadding="4"><tr><th>Step</th><th>Status</th><th>Time</th></tr>' + stepRows + '</table>' +
        '<h3>Actual Measurements</h3><table width="100%" cellpadding="4"><tr><th>Measurement</th><th>Value</th><th>Limits</th><th>Result</th></tr>' + measureRows + '</table></div>';
    ui.set('stationReport', html);
    if (report.executionId) ui.set('reportFooter', archivetext);
}
function exportResult(format) {
    const path = TestRunner.exportReport(format);
    const text = path ? 'Exported ' + format.toUpperCase() + ': ' + path :
        'Export failed: ' + TestRunner.lastError;
    ui.set('reportFooter', text);
    print(text);
}
// Designer event: reportRefresh / clicked -> refreshReport (assigned in Properties -> Events).
// Designer event: select reportExportJson -> Properties -> Events -> clicked
function onReportExportJsonClicked() {
    exportResult('json');
}
// Designer event: select reportExportCsv -> Properties -> Events -> clicked
function onReportExportCsvClicked() {
    exportResult('csv');
}
TestRunner.onFinished(function () {
    refreshReport();
});
// No test is started when the Panel starts. Reading the report is harmless.
refreshReport();
