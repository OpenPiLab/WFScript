// ┌─ WFScript LEARNING LAB: I²C Protocol — Master and Spy
// │ Level: Intermediate  |  Mode: hardware-capable
// │
// │ GOAL: Configure a native I²C master, issue a register read and inspect Spy traffic.
// │ BEFORE RUN: Use Simulation for API exploration. Physical I²C needs voltage-compatible
// │    pull-ups, SDA/SCL and common ground.
// │ TRY FIRST: Press CONFIGURE.
// │ TRACE: `i2cCfg` → `onI2cCfgClicked()` → `Protocol1.I2C.configure({scl:0,sda:1,...})`.
// │ EDIT SAFELY: Change the address only to one used by your own I²C target; consult its
// │    actual register map.
// │ REMEMBER: I²C acknowledgements depend on an external slave; a generic read cannot
// │    predict every sensor’s data format.
// │
// │ EVENT TIP: Select `i2cCfg` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/ProtocolI2C.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

// Native I2C master + SDK I2C Spy. External pullups and common GND required.
function i2cLog(t) {
    ui.set('i2cStatus', t);
    ui.set('i2cLog', {
        append: {
            level: t.indexOf('ERROR') >= 0 ? 'ERROR' : (t.indexOf('NACK') >= 0 || t.indexOf('UNCONFIRMED') >= 0 ? 'WARN' : 'INFO'),
            source: 'I2C',
            message: t
        }
    });
    print('I2C: ' + t);
}
function i2cTry(fn) {
    try {
        fn();
    } catch (e) {
        i2cLog('ERROR: ' + String(e));
    }
}
function i2cAddress() {
    return parseInt(String(ui.get('i2cAddr')), 16);
}
/**
 * CORE LESSON — Configures the native I²C bus. Real responses require an externally powered,
 * correctly addressed target and appropriate pull-ups.
 */
function i2cSetup() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    if (!Protocol1.I2C.configure({
        scl: 0,
        sda: 1,
        rateHz: Number(ui.get('i2cRate')),
        timeoutSeconds: 1,
        clockStretching: true
    }))
        throw Error(Protocol1.I2C.lastError);
    i2cLog((Protocol1.simulation ? 'SIMULATED' : 'READY') + ' / address 0x' + i2cAddress().toString(16));
}
function i2cReadReg() {
    // This illustrates a common register-style sensor; the actual register
    // layout and returned units depend on YOUR connected I2C device.
    const r = Protocol1.I2C.writeRead(i2cAddress(), [0x00], 2);
    if (!r.ok) throw Error(r.error);
    if (!r.acknowledged) {
        i2cLog((r.simulated ? 'SIMULATED — no real ACK' : 'DEVICE NACK/ERROR') + ' / index ' + r.nackIndex);
        return;
    }
    i2cLog('REGISTER READ / bytes ' + JSON.stringify(r.data));
}
function i2cSpy() {
    if (!Protocol1.I2C.Receiver()) throw Error(Protocol1.I2C.lastError);
    i2cLog('SPY INITIALIZED — monitor an external bus');
}
function i2cPoll() {
    const r = Protocol1.I2C.spyPoll(64);
    if (!r.ok) throw Error(r.error);
    i2cLog((r.simulated ? 'SIMULATION — no observed frames' : 'SPY') + ' / start ' + (r.start || 0) +
        ' stop ' + String(!!r.stop) + ' bytes ' + JSON.stringify(r.data) + ' NAK ' + String(r.nackIndex));
}
function i2cStop() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    i2cLog('STOPPED / pins released');
}
function onI2cCfgClicked() {
    return i2cTry(i2cSetup);
}
function onI2cReadClicked() {
    return i2cTry(i2cReadReg);
}
function onI2cSpyClicked() {
    return i2cTry(i2cSpy);
}
function onI2cPollClicked() {
    return i2cTry(i2cPoll);
}
function onI2cStopClicked() {
    return i2cTry(i2cStop);
}
i2cLog('IDLE — do not connect I2C without external pull-ups');
