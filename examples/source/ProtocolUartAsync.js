// ┌─ WFScript LEARNING LAB: UART Asynchronous Lab — Native Terminal, Callbacks and Test
// │ Level: Advanced  |  Mode: hardware-capable
// │
// │ GOAL: Compare native UART Terminal display, JavaScript receive callbacks and automated
// │    echo testing.
// │ BEFORE RUN: No DIO is driven on startup. Physical echo test requires DIO0→1 kΩ→DIO1;
// │    simulation must not PASS the hardware test.
// │ TRY FIRST: On Native Terminal configure auto reception, then send and read text.
// │ TRACE: `uartView` sendRequested → `onUartViewSendRequested(text)`;
// │    `Protocol1.UART.onData()` remains a device callback.
// │ EDIT SAFELY: Change the displayed receive message without changing the native buffer
// │    management.
// │ REMEMBER: The widget can display native receive data without manually polling from a
// │    JavaScript timer; TestRunner owns its scheduler.
// │
// │ EVENT TIP: Select `asyncConfigure` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/ProtocolUartAsync.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * UART Async Lab (three Pages), v0.2-dev.1
 * One QJSEngine, one native receive poller, no JS polling timer.
 * Native UART Terminal subscribes automatically to RX chunks, on any Page.
 * DIO0 -> 1 kOhm -> DIO1 for local loopback. NEVER drive external wiring until checked.
 * Opening the Panel does not configure hardware or start a receiver.
 */
function note(target, message, level) {
    ui.set(target, {
        append: {
            source: 'UART',
            level: level || 'INFO',
            message: String(message)
        }
    });
    print('[UART Async] ' + message);
}
function checked(action) {
    try {
        action();
    } catch (e) {
        note('asyncLog', String(e), 'ERROR');
        note('callbackLog', String(e), 'ERROR');
        ui.set('testStatus', String(e));
    }
}
function configure(mode) {
    // Explicit operator action: no automatic DIO drive on Panel load.
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    const baud = 115200;
    if (!Protocol1.UART.configure({
        tx: 0,
        rx: 1,
        baudRate: baud,
        dataBits: 8,
        parity: 'none',
        stopBits: 1
    }))
        throw Error(Protocol1.UART.lastError);
    if (mode === 'auto') {
        if (!Protocol1.UART.startAutoRx({
            pollIntervalMs: 10,
            bufferSize: 65536
        }))
            throw Error(Protocol1.UART.lastError);
    } else if (!Protocol1.UART.startRx()) throw Error(Protocol1.UART.lastError);
    note('asyncLog', 'CONFIGURED / ' + baud + ' baud / RX mode ' + mode +
        (Protocol1.simulation ? ' / SIMULATION (no real bytes)' : ' / REAL AD3'));
}
function transmit(text) {
    const tx = String(text) + '\n';
    const r = Protocol1.UART.writeText(tx);
    if (!r.ok) throw Error(r.error);
    note('asyncLog', 'TX ' + JSON.stringify(tx) + (r.simulated ? ' SIMULATION' : ''));
}
function showStats(log) {
    const s = Protocol1.UART.stats();
    const message = 'auto=' + s.autoReceiving + ' queued=' + s.available + ' RX=' + s.receivedBytes +
        ' SDK overflow=' + s.sdkOverflows + ' dropped=' + s.queueDroppedBytes;
    ui.set(log === 'callbackLog' ? 'callbackStatus' : 'asyncStatus', message);
    note(log, message);
}
// Page 01: C++ direct receive + optional manual read.
// Designer event: select asyncConfigure -> Properties -> Events -> clicked
/**
 * CORE LESSON — Enables the native asynchronous UART receiver when the user requests it. The
 * receive queue is not polled by a custom JavaScript timer.
 */
function onAsyncConfigureClicked() {
    return checked(() => configure('auto'));
}
// Designer event: select manualConfigure -> Properties -> Events -> clicked
function onManualConfigureClicked() {
    return checked(() => configure('manual'));
}
// Designer event: select uartView -> Properties -> Events -> sendRequested
function onUartViewSendRequested(text) {
    return checked(() => transmit(text));
}
// Designer event: select asyncSend -> Properties -> Events -> clicked
function onAsyncSendClicked() {
    return checked(() => transmit('HELLO'));
}
// Designer event: select asyncRead -> Properties -> Events -> clicked
function onAsyncReadClicked() {
    return checked(() => {
        const r = Protocol1.UART.read(4096);
        if (!r.ok) throw Error(r.error);
        note('asyncLog', 'read() ' + r.count + ' bytes / ' + JSON.stringify(String.fromCharCode.apply(null, r.data)) +
            ' / ' + (r.queued ? 'native queue' : 'direct SDK') + ' / overflow=' + !!r.overflow);
    });
}
// Designer event: select asyncStats -> Properties -> Events -> clicked
function onAsyncStatsClicked() {
    return checked(() => showStats('asyncLog'));
}
// Designer event: select asyncStopTimer -> Properties -> Events -> clicked
function onAsyncStopTimerClicked() {
    return checked(() => {
        Protocol1.UART.stopAutoRx();
        note('asyncLog', 'Native polling paused; read() preserves queued bytes');
    });
}
// Designer event: select asyncClear -> Properties -> Events -> clicked
function onAsyncClearClicked() {
    return ui.set('uartView', { clear: true });
}
// Designer event: select asyncRelease -> Properties -> Events -> clicked
function onAsyncReleaseClicked() {
    return checked(() => {
        if (!Protocol1.stop()) throw Error(Protocol1.lastError);
        note('asyncLog', 'UART fully released, output pins returned to safe state');
    });
}

// Page 02: JS callbacks see COPIES. They never consume the native queue.
// Callback exceptions go through PanelRuntime's source-aware diagnostics.
let callbackBytes = 0;
function handleUartChunk(chunk) {
    callbackBytes += chunk.count;
    ui.set('callbackCount', String(callbackBytes) + ' bytes observed by JS callback');
    note('callbackLog', 'onData ' + JSON.stringify(chunk.text) + ' / ' + chunk.count + ' bytes' +
        (chunk.overflow ? ' SDK OVERFLOW' : '') + ' / ' + chunk.timestampSource,
        chunk.overflow ? 'WARN' : 'INFO');
}
// Designer event: select callbackStart -> Properties -> Events -> clicked
function onCallbackStartClicked() {
    return checked(() => {
        configure('auto');
        callbackBytes = 0;
        if (!Protocol1.UART.onData(handleUartChunk)) throw Error(Protocol1.UART.lastError);
        note('callbackLog', 'onData callback registered; no JavaScript timer');
    });
}
// Designer event: select callbackDisable -> Properties -> Events -> clicked
function onCallbackDisableClicked() {
    return checked(() => {
        Protocol1.UART.clearOnData();
        note('callbackLog', 'Callback removed; native RX and queue still active');
    });
}
// Designer event: select callbackSend -> Properties -> Events -> clicked
function onCallbackSendClicked() {
    return checked(() => transmit('CALLBACK'));
}
// Designer event: select callbackDrain -> Properties -> Events -> clicked
function onCallbackDrainClicked() {
    return checked(() => {
        const r = Protocol1.UART.read(4096);
        if (!r.ok) throw Error(r.error);
        note('callbackLog', 'Queue read: ' + r.count + ' bytes; JS callback count stays at ' + callbackBytes);
    });
}
// Designer event: select callbackStats -> Properties -> Events -> clicked
function onCallbackStatsClicked() {
    return checked(() => showStats('callbackLog'));
}
// Designer event: select callbackRelease -> Properties -> Events -> clicked
function onCallbackReleaseClicked() {
    return checked(() => {
        if (!Protocol1.stop()) throw Error(Protocol1.lastError);
        note('callbackLog', 'RX stopped, callback cleared, hardware released');
    });
}

// Page 03: the Test Executive uses a project-owned definition and module.
// Its native polling step (20 ms) consumes the UART queue without busy-waiting.
// Designer event: select testRun -> Properties -> Events -> clicked
function onTestRunClicked() {
    if (!TestRunner.run()) ui.set('testStatus', 'Run rejected: ' + TestRunner.lastError);
    else ui.set('testStatus', 'TEST RUNNING: expected echo is TEST\n');
}
// Designer event: select testStop -> Properties -> Events -> clicked
function onTestStopClicked() {
    if (!TestRunner.terminate()) ui.set('testStatus', 'Terminate rejected: ' + TestRunner.lastError);
}
// Designer event: select testReset -> Properties -> Events -> clicked
function onTestResetClicked() {
    if (!TestRunner.reset()) ui.set('testStatus', 'Reset rejected: ' + TestRunner.lastError);
    else ui.set('testStatus', 'READY — press RUN TEST to start');
}
// Designer event: select testReport -> Properties -> Events -> clicked
function onTestReportClicked() {
    const sum = TestRunner.summary();
    const report = TestRunner.report();
    ui.set('testStatus', 'State ' + sum.state + ' / measurements ' + (report.measurements || []).length +
        ' / execution ' + (report.executionId || 'none'));
}
TestRunner.onFinished(function () {
    const s = TestRunner.summary();
    ui.set('testStatus', 'FINISHED / ' + s.state + ' / ' + s.passed + ' passed / ' + s.failed + ' failed');
});
ui.set('testStatus', 'READY — press RUN TEST to start');
ui.set('asyncStatus', 'IDLE — select a receive mode; no pins driven');
ui.set('callbackStatus', 'IDLE — explicit callback registration required');
