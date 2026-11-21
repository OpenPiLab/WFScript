// ┌─ WFScript LEARNING LAB: SWD Protocol — Target Registers
// │ Level: Advanced  |  Mode: hardware-capable
// │
// │ GOAL: Read a debug-port identification register from a compatible ARM SWD target.
// │ BEFORE RUN: Use Simulation for API structure. Physical use requires voltage-matched
// │    SWCLK, SWDIO and common ground.
// │ TRY FIRST: Press CONFIGURE.
// │ TRACE: `swdCfg` → `onSwdCfgClicked()` → configure clock/data; `swdRead` → read DP
// │    register.
// │ EDIT SAFELY: Change the clock rate only within the capabilities of your actual SWD
// │    target.
// │ REMEMBER: SWD requires protocol acknowledgements and turnaround timing; a simulated read
// │    does not identify hardware.
// │
// │ EVENT TIP: Select `swdCfg` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/ProtocolSWD.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

// ARM SWD target register reading. No writes on startup or in this demo.
function swdLog(t) {
    ui.set('swdStatus', t);
    ui.set('swdLog', {
        append: {
            level: t.indexOf('ERROR') >= 0 ? 'ERROR' : (t.indexOf('NACK') >= 0 || t.indexOf('UNCONFIRMED') >= 0 ? 'WARN' : 'INFO'),
            source: 'SWD',
            message: t
        }
    });
    print('SWD: ' + t);
}
function swdTry(fn) {
    try {
        fn();
    } catch (e) {
        swdLog('ERROR: ' + String(e));
    }
}
/**
 * CORE LESSON — Configures a clock/data debug connection on explicit operator request; a target ID
 * read requires a voltage-compatible powered target.
 */
function swdConfigure() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    const rate = Number(ui.get('swdRate'));
    if (!Protocol1.SWD.configure({
        clock: 0,
        data: 1,
        rateHz: rate,
        turnaround: 1
    })) throw Error(Protocol1.SWD.lastError);
    swdLog((Protocol1.simulation ? 'SIMULATED' : 'READY') + ' / ' + rate + ' Hz / SWCLK0 SWDIO1');
}
function swdReset() {
    if (!Protocol1.SWD.clear(52, 16)) throw Error(Protocol1.SWD.lastError);
    swdLog('SWD RESET SENT');
}
function swdRead() {
    const r = Protocol1.SWD.readRegister(false, 0);
    if (!r.ok) throw Error(r.error);
    if (r.simulated) {
        swdLog('SIMULATED — no real target ACK or ID');
        return;
    }
    swdLog('DP IDCODE / ACK ' + r.ackName + ' (' + r.ack + ') / 0x' + Number(r.value || 0).toString(16) +
        ' / parityError ' + String(!!r.parityError));
}
function swdStop() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    swdLog('STOPPED / pins released');
}
function onSwdCfgClicked() {
    return swdTry(swdConfigure);
}
function onSwdResetClicked() {
    return swdTry(swdReset);
}
function onSwdReadClicked() {
    return swdTry(swdRead);
}
function onSwdStopClicked() {
    return swdTry(swdStop);
}
swdLog('IDLE — connect only voltage-compatible SWD target with common ground');
