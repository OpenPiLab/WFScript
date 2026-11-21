// ┌─ WFScript LEARNING LAB: UART Protocol — Native SDK Terminal
// │ Level: Intermediate  |  Mode: hardware-capable
// │
// │ GOAL: Use native AD3 UART commands to transmit text and inspect received data.
// │ BEFORE RUN: Use Simulation for API exploration. For real UART use DIO0 TX→1 kΩ→DIO1 RX
// │    or a voltage-compatible device and common ground.
// │ TRY FIRST: Click CONFIGURE / START RX.
// │ TRACE: `uartCfg` → `onUartCfgClicked()` → Protocol1.UART configure/startRx; send and
// │    poll use separate native calls.
// │ EDIT SAFELY: Change the transmitted text and compare the UTF-8/text result with received
// │    bytes when physically looped back.
// │ REMEMBER: Native Digilent UART is different from PC Serial COM ports.
// │
// │ EVENT TIP: Select `uartCfg` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/ProtocolUART.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

// Native Digilent UART TX/RX. Simulation never produces actual received bytes.
// All DIO outputs are enabled only after the operator presses a button.
function uartLog(t) {
    ui.set('uartStatus', t);
    ui.set('uartLog', {
        append: {
            level: t.indexOf('ERROR') >= 0 ? 'ERROR' : (t.indexOf('NACK') >= 0 || t.indexOf('UNCONFIRMED') >= 0 ? 'WARN' : 'INFO'),
            source: 'UART',
            message: t
        }
    });
    print('UART: ' + t);
}
function uartTry(fn) {
    try {
        fn();
    } catch (e) {
        uartLog('ERROR: ' + String(e));
    }
}
/**
 * CORE LESSON — Releases old protocol ownership, then configures native AD3 UART TX/RX. The PC
 * COM-port Serial API is a separate system.
 */
function uartConfig() {
    const baud = Number(ui.get('uartBaud'));
    if (!Protocol1.stop()) throw Error('release previous session: ' + Protocol1.lastError);
    if (!Protocol1.UART.configure({
        tx: 0,
        rx: 1,
        baudRate: baud,
        dataBits: 8,
        parity: 'none',
        stopBits: 1
    }))
        throw Error(Protocol1.UART.lastError);
    if (!Protocol1.UART.startRx()) throw Error(Protocol1.UART.lastError);
    uartLog((Protocol1.simulation ? 'SIMULATED (no physical IO)' : 'UART READY') + ' / ' + baud + ' baud / TX0 RX1');
}
function uartSend() {
    const s = String(ui.get('uartText') || 'PING');
    const r = Protocol1.UART.writeText(s + '\n');
    if (!r.ok) throw Error(r.error);
    uartLog((r.simulated ? 'SIMULATION — no actual transmission' : 'TX SENT') + ' / ' + JSON.stringify(s + '\n'));
}
function uartRead() {
    const r = Protocol1.UART.read(1024);
    if (!r.ok) throw Error(r.error);
    const text = String.fromCharCode.apply(null, r.data);
    uartLog((r.simulated ? 'SIMULATED EMPTY RX' : 'RX ' + r.count + ' bytes') + ' / ' + JSON.stringify(text) +
        ' / parityIndex ' + String(r.parityErrorIndex || 0) + ' / overflow ' + String(!!r.overflow));
}
function uartStop() {
    if (!Protocol1.stop()) throw Error('OUTPUT SHUTDOWN UNCONFIRMED: ' + Protocol1.lastError);
    uartLog('STOPPED — digital resources released');
}
function onUartCfgClicked() {
    return uartTry(uartConfig);
}
function onUartSendClicked() {
    return uartTry(uartSend);
}
function onUartReadClicked() {
    return uartTry(uartRead);
}
function onUartStopClicked() {
    return uartTry(uartStop);
}
uartLog('IDLE — select CONFIGURE before sending; no pins driven');
