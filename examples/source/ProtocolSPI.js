// ┌─ WFScript LEARNING LAB: SPI Protocol — Master Transactions
// │ Level: Intermediate  |  Mode: hardware-capable
// │
// │ GOAL: Configure an AD3 SPI master and inspect a command/response transfer.
// │ BEFORE RUN: Use Simulation for structure; physical use needs a voltage-compatible SPI
// │    target, a shared ground and valid wiring.
// │ TRY FIRST: Press CONFIGURE.
// │ TRACE: `spiCfg` → `onSpiCfgClicked()` →
// │    `Protocol1.SPI.configure({clock:0,mosi:1,miso:2,select:3,...})`.
// │ EDIT SAFELY: Change the configured SPI mode only when your connected target supports it.
// │ REMEMBER: Clock, MOSI, MISO and chip-select are separate signals; receiving data
// │    requires a responding target.
// │
// │ EVENT TIP: Select `spiCfg` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/ProtocolSPI.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

// SPI master transaction example; DIO pins remain untouched until CONFIGURE.
function spiLog(t) {
    ui.set('spiStatus', t);
    ui.set('spiLog', {
        append: {
            level: t.indexOf('ERROR') >= 0 ? 'ERROR' : (t.indexOf('NACK') >= 0 || t.indexOf('UNCONFIRMED') >= 0 ? 'WARN' : 'INFO'),
            source: 'SPI',
            message: t
        }
    });
    print('SPI: ' + t);
}
function spiTry(fn) {
    try {
        fn();
    } catch (e) {
        spiLog('ERROR: ' + String(e));
    }
}
/**
 * CORE LESSON — Makes SPI pin assignments and mode/rate explicit. Read-only simulation does not
 * prove a target device or flash ID exists.
 */
function spiSetup() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    const cfg = {
        clock: 0,
        mosi: 1,
        miso: 2,
        select: 3,
        frequencyHz: Number(ui.get('spiRate')),
        mode: Number(ui.get('spiMode')),
        msbFirst: true
    };
    if (!Protocol1.SPI.configure(cfg)) throw Error(Protocol1.SPI.lastError);
    spiLog((Protocol1.simulation ? 'SIMULATION' : 'READY') + ' / ' + cfg.frequencyHz + ' Hz / mode ' + cfg.mode);
}
function spiTransfer() {
    const r = Protocol1.SPI.transfer([0x9F, 0, 0, 0]);
    if (!r.ok) throw Error(r.error);
    spiLog((r.simulated ? 'SIMULATED — NOT a real device ID' : 'TRANSFER COMPLETE') + ' / RX ' + JSON.stringify(r.rx));
}
function spiFlashId() {
    // JEDEC ID: command byte followed by three RX bytes via native command+dummy API.
    const r = Protocol1.SPI.command(8, 0x9F, 0, [], 3, 8, 1);
    if (!r.ok) throw Error(r.error);
    spiLog((r.simulated ? 'SIMULATED — no flash identified' : 'FLASH RESPONSE') + ' / ' + JSON.stringify(r.rx));
}
function spiStop() {
    if (!Protocol1.stop()) throw Error(Protocol1.lastError);
    spiLog('STOPPED / pins released');
}
function onSpiCfgClicked() {
    return spiTry(spiSetup);
}
function onSpiXferClicked() {
    return spiTry(spiTransfer);
}
function onSpiReadIdClicked() {
    return spiTry(spiFlashId);
}
function onSpiStopClicked() {
    return spiTry(spiStop);
}
spiLog('IDLE — configure an SPI master before transfers');
