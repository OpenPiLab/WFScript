// ┌─ WFScript LEARNING LAB: CAN Protocol — Classical CAN
// │ Level: Advanced  |  Mode: hardware-capable
// │
// │ GOAL: Configure classical CAN, transmit a frame and poll the receive side.
// │ BEFORE RUN: Use Simulation first. Physical CAN requires an appropriate external CAN
// │    transceiver, wiring and bus termination.
// │ TRY FIRST: Press CONFIGURE / START RX.
// │ TRACE: `canCfg` → `onCanCfgClicked()` → `Protocol1.CAN.configure({tx:0,rx:1,...})`.
// │ EDIT SAFELY: Change the standard CAN identifier in the Panel and observe the formatting
// │    of the transmitted ID.
// │ REMEMBER: CAN traffic, acknowledgement and frame reception are bus-level phenomena, not
// │    guaranteed by script execution.
// │
// │ EVENT TIP: Select `canCfg` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/ProtocolCAN.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

// Classical CAN only. Requires proper physical CAN transceiver and bus.
function canLog(t) {
    ui.set('canStatus', t);
    ui.set('canLog', {
        append: {
            level: t.indexOf('ERROR') >= 0 ? 'ERROR' : (t.indexOf('NACK') >= 0 || t.indexOf('UNCONFIRMED') >= 0 ? 'WARN' : 'INFO'),
            source: 'CAN',
            message: t
        }
    });
    print('CAN: ' + t);
}
function canTry(fn) {
    try {
        fn();
    } catch (e) {
        canLog('ERROR: ' + String(e));
    }
}
/**
 * CORE LESSON — Initializes native classical CAN and receive mode. Physical CAN requires a real
 * transceiver; AD3 DIO pins must never connect to CANH/CANL directly.
 */
function canConfigure() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    const rate = Number(ui.get('canRate'));
    if (!Protocol1.CAN.configure({
        tx: 0,
        rx: 1,
        rateHz: rate
    })) throw Error(Protocol1.CAN.lastError);
    if (!Protocol1.CAN.startRx()) throw Error(Protocol1.CAN.lastError);
    canLog((Protocol1.simulation ? 'SIMULATED' : 'READY') + ' / ' + rate + ' bit/s (classical CAN)');
}
function canSend() {
    const id = parseInt(String(ui.get('canId')), 16);
    const r = Protocol1.CAN.send(id, false, false, [0x12, 0x34, 0x56]);
    if (!r.ok) throw Error(r.error);
    canLog((r.simulated ? 'SIMULATION — no transmitted CAN frame' : 'TX COMPLETE') + ' / ID 0x' + id.toString(16));
}
function canRead() {
    const r = Protocol1.CAN.receive();
    if (!r.ok) throw Error(r.error);
    if (r.simulated) {
        canLog('SIMULATION — no bus traffic');
        return;
    }
    if (!r.received) {
        canLog('NO NEW FRAME');
        return;
    }
    canLog('CAN ID 0x' + Number(r.id).toString(16) + ' DLC ' + r.dlc + ' STATUS ' + r.status + ' DATA ' + JSON.stringify(r.data) +
        (r.status === 1 ? ' OK' : ' — DECODE ERROR'));
}
function canStop() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    canLog('STOPPED / pins released');
}
function onCanCfgClicked() {
    return canTry(canConfigure);
}
function onCanSendClicked() {
    return canTry(canSend);
}
function onCanReadClicked() {
    return canTry(canRead);
}
function onCanStopClicked() {
    return canTry(canStop);
}
canLog('IDLE — a CAN TRANSCEIVER is mandatory; NEVER connect CANH/CANL to DIO pins');
