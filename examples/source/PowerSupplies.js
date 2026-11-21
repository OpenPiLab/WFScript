// ┌─ WFScript LEARNING LAB: V+ / V− Power Supplies
// │ Level: Intermediate  |  Mode: hardware-capable
// │
// │ GOAL: Set separate V+/V− rails and watch actual readback follow changes over time.
// │ BEFORE RUN: Simulation first. Physical mode can energize the AD3 supply pins; verify
// │    wiring, load and safe voltage.
// │ TRY FIRST: Set V+ and V− values and enable at least one rail.
// │ TRACE: `psPowerOn` → `psStart()`; 250 ms read-only timer → `psShowStatus()` →
// │    psPlusReadout/psMinusReadout.
// │ EDIT SAFELY: Change a setpoint while powered in Simulation and watch how the readout
// │    updates without pressing READ STATUS.
// │ REMEMBER: Actual readback must not be replaced by the requested setpoint. The polling
// │    timer never configures outputs.
// │
// │ EVENT TIP: Select `psVPlus` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/PowerSupplies.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * WFScript Example: V+ / V− Power Supplies
 * Set and enable independent programmable supplies with live voltage readback.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: Measure V+/V− relative to AD3 GND; verify the circuit and safe load.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// 11 SUPPLIES — INDEPENDENT AD3 V+ / V- PROGRAMMABLE POWER
// ============================================================================
// The documented WaveForms nested property names are used below. WFScript
// deliberately stages values until Supplies.run()/config() is called. POWER ON
// is the explicit authorization to energize physical outputs. Hardware status
// is read from AnalogIO; simulation status is clearly labeled as an ideal model.
// AD3 AnalogIO voltage status can lag immediately after a configuration commit.
// Keep polling readback while power is active; do NOT substitute a setpoint for
// a physical measurement and never configure the hardware from a timer tick.
let psIsOn = false;
let psReadbackTimer = 0;
let psOffSettleReads = 0;

// bounded follow-up after confirmed POWER OFF
let psStatusPrefix = 'READY / POWER OFF';
const PS_READBACK_INTERVAL_MS = 250;

function psStopReadbackMonitor() {
    if (!psReadbackTimer) return;
    clearInterval(psReadbackTimer);
    psReadbackTimer = 0;
}
function psUpdateReadbackMonitor() {
    if (!psIsOn && !psOffSettleReads) {
        psStopReadbackMonitor();
        return;
    }
    if (psReadbackTimer) return;
    // Changing setpoints must not create another timer.
    psReadbackTimer = setInterval(function () {
        try {
            // Read-only refresh. AnalogIO can report the previous voltage immediately
            // after run()/config(), so successive status samples are intentional.
            psShowStatus();
        } catch (e) {
            psStopReadbackMonitor();
            ui.set('psStatus', 'READBACK MONITOR ERROR: ' + String(e));
            print('Supplies readback monitor: ' + e);
            // A readback failure must NOT change the physical power state.
        }
    }, PS_READBACK_INTERVAL_MS);
}

function psSettings() {
    const vp = Number(ui.get('psVPlus'));
    const vn = Number(ui.get('psVMinus'));
    const ep = !!ui.get('psEnablePlus');
    const en = !!ui.get('psEnableMinus');
    if (!isFinite(vp) || vp < 0.5 || vp > 5 || !isFinite(vn) || vn < -5 || vn > -0.5)
        throw Error('AD3 range V+ 0.5..5 V, V- -5..-0.5 V');
    Supplies.Output.PositiveSupply.Voltage.value = vp;
    Supplies.Output.NegativeSupply.Voltage.value = vn;
    Supplies.Output.PositiveSupply.Enable.value = ep ? 1 : 0;
    Supplies.Output.NegativeSupply.Enable.value = en ? 1 : 0;
    return {
        vp,
        vn,
        ep,
        en
    };
}
/**
 * CORE LESSON — Reads SDK-reported supply state and updates both readouts; never substitutes the
 * requested setpoint for measured voltage. It also manages bounded post-off settling refresh.
 */
function psShowStatus(prefix) {
    if (prefix !== undefined) psStatusPrefix = String(prefix);
    const s = Supplies.status();
    // Use the bridge state, not just the last button click. An SDK stop failure
    // must never be presented as a confirmed power-off.
    const live = !!s.running;
    psIsOn = live;
    ui.set('psMaster', live ? 1 : 0);
    ui.set('psState', !s.simulation && !s.measuredAvailable ? 1 : (live ? 2 : 0));
    // Use the actual SDK readback (or the explicitly simulated ideal value).
    // In physical mode without readback, do NOT overwrite a display with a
    // fabricated zero or copy the requested setpoint into the measurement.
    if (s.simulation || s.measuredAvailable) {
        if (s.positiveVoltage != null) ui.set('psPlusReadout', s.positiveVoltage);
        if (s.negativeVoltage != null) ui.set('psMinusReadout', s.negativeVoltage);
    }
    const readback = s.simulation ? 'SIMULATED ideal voltage' :
        (s.measuredAvailable ? 'AD3 voltage readback' : 'AD3 readback unavailable (last valid values held)');
    ui.set('psStatus', psStatusPrefix + ' | ' + readback + ' | V+ ' + s.positiveSetpoint.toFixed(2) +
        ' V / V- ' + s.negativeSetpoint.toFixed(2) + ' V | master ' + (live ? 'ON' : 'OFF'));
    // After a confirmed OFF, AnalogIO may still report the preceding sample.
    // Follow the measured decay briefly rather than immediately showing a
    // fabricated 0 V. End the follow-up once both rails are near zero or after
    // at most eight reads; this timer never energizes or configures outputs.
    if (!live && psOffSettleReads > 0) {
        --psOffSettleReads;
        if ((s.simulation || s.measuredAvailable) && s.positiveVoltage != null &&
            s.negativeVoltage != null && Math.abs(s.positiveVoltage) < 0.05 &&
            Math.abs(s.negativeVoltage) < 0.05) psOffSettleReads = 0;
    }
    psUpdateReadbackMonitor();
}
function psStart() {
    try {
        const s = psSettings();
        if (!s.ep && !s.en) throw Error('Enable at least one V+ or V- rail before POWER ON');
        if (!Supplies.run()) throw Error(Supplies.lastError);
        psOffSettleReads = 0;
        psIsOn = true;
        psShowStatus('POWER ON');
    } catch (e) {
        const stopped = Supplies.stop();
        psShowStatus((stopped ? 'POWER OFF / ERROR: ' : 'STOP NOT CONFIRMED / ERROR: ') + String(e));
        print('Supplies: ' + e);
    }
}
function psStop() {
    const wasLive = psIsOn;
    const ok = Supplies.stop();
    if (ok && wasLive) psOffSettleReads = 8;
    psShowStatus(ok ? 'POWER OFF' : 'STOP NOT CONFIRMED — CHECK HARDWARE: ' + Supplies.lastError);
}
function psChanged() {
    try {
        psSettings();
        if (psIsOn) {
            if (!Supplies.config()) throw Error(Supplies.lastError);
            psIsOn = Supplies.running;
            psShowStatus('LIVE APPLIED');
        } else psShowStatus('STAGED / POWER OFF');
    } catch (e) {
        if (psIsOn && !Supplies.stop())
            print('WARNING: Supply shutdown not confirmed: ' + Supplies.lastError);
        psShowStatus('SETTINGS ERROR: ' + String(e));
    }
}


function onPsRefreshClicked() {
    psShowStatus('STATUS REFRESH');
}




// Initialization is strictly software-only; no physical supply output commands.
psShowStatus('READY / POWER OFF');
