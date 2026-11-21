// ┌─ WFScript LEARNING LAB: Digital Input Test Station — Native Monitor
// │ Level: Advanced
// │
// │ GOAL: Monitor debounced DIO input changes and optionally trigger the Test Executive.
// │ BEFORE RUN: No automatic polling or output at startup. Hardware inputs are active-low
// │    with 10 kΩ pull-ups to 3.3 V, switches to ground.
// │ TRY FIRST: Press START MONITOR and inspect stats.
// │ TRACE: `monitorStart` → `onMonitorStartClicked()` → `StaticIO.onChange()` +
// │    `StaticIO.startMonitor()`.
// │ EDIT SAFELY: Change the debounceMs value in `registerInputs()` and compare callback
// │    sensitivity in Simulation.
// │ REMEMBER: Registering callbacks and starting input polling are separate operations;
// │    edges are host-observed.
// │
// │ EVENT TIP: Select `monitorStart` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/DigitalInputMonitor.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/** Digital Input Test Station — v0.2-dev.4
 * All monitoring is OPT-IN. Loading/starting this Panel does not poll or drive DIO.
 * Physical wiring: 3.3V -> 10k pull-up -> DIO0 (START) / DIO1 (fixture closed);
 * each switch connects its pin to GND when pressed/closed. Common GND.
 * Do not connect unprotected industrial voltages or use this for a safety E-stop.
 */
let autoStartArmed = false;
function note(target, message, level) {
    print('[Digital Input] ' + message);
    ui.set(target, {
        append: {
            source: 'DIO',
            level: level || 'INFO',
            message: String(message)
        }
    });
}
function action(fn) {
    try {
        fn();
    } catch (e) {
        const message = String(e);
        ui.set('monitorState', 'ERROR: ' + message);
        note('inputLog', message, 'ERROR');
        note('stationLog', message, 'ERROR');
    }
}
function inputStart(value, pin, event) {
    ui.set('inputState', 'DIO0 START: PRESSED (stable ' + event.stableMs + ' ms)');
    note('inputLog', 'DIO0 ' + event.edge + ' at host ' + event.timestampUtc + '; ' + event.timestampSource);
    if (!autoStartArmed) return;
    // Do not reconstruct or reconfigure test definitions here: project owns them.
    if (TestRunner.run()) {
        ui.set('stationStatus', 'Physical START received; test running');
        note('stationLog', 'START press launched Test Executive');
    } else {
        note('stationLog', 'START ignored: ' + TestRunner.lastError, 'WARN');
    }
}
function inputFixture(value, pin, event) {
    const state = value === 0 ? 'CLOSED' : 'OPEN';
    ui.set('fixtureState', 'Fixture DIO1: ' + state);
    ui.set('stationFixture', 'Fixture: ' + state);
    note('callbackLog', 'DIO1 ' + event.edge + ' / ' + state + ' / host-observed');
}
/**
 * CORE LESSON — Registers edge-triggered JavaScript callbacks; it does NOT begin polling. The
 * distinct startMonitor action explicitly enables native input monitoring.
 */
function registerInputs() {
    if (!StaticIO.onChange(0, inputStart, {
        edge: 'falling',
        debounceMs: 30
    }))
        throw Error(StaticIO.lastError);
    if (!StaticIO.onChange(1, inputFixture, {
        edge: 'change',
        debounceMs: 30
    }))
        throw Error(StaticIO.lastError);
}
function armMonitor() {
    if (StaticIO.monitoring) throw Error('Already polling; use STOP MONITOR to pause');
    registerInputs();
    if (!StaticIO.startMonitor({ pollIntervalMs: 20 })) throw Error(StaticIO.lastError);
    ui.set('monitorState', 'RUNNING / 20ms read-only polling / 30ms debounce');
    note('inputLog', 'Started native input monitor; no DIO outputs configured');
}
function stopMonitor() {
    StaticIO.stopMonitor();
    autoStartArmed = false;
    ui.set('monitorState', 'STOPPED / zero SDK monitor polls');
    ui.set('stationStatus', 'AUTO START DISARMED — monitor stopped');
    note('inputLog', 'C++ input monitor stopped; callbacks retained for restart');
}
function showStats() {
    const s = StaticIO.monitorStats();
    const msg = 'monitoring=' + s.monitoring + ' polls=' + s.polls + ' callbacks=' + s.callbacks +
        ' changes=' + s.changes + ' last SDK poll=' + s.lastPollDurationUs + 'us watches=' + s.watchCount;
    ui.set('monitorStatsDisplay', msg);
    note('inputLog', msg);
}
function simulateInput(pin, level) {
    if (!StaticIO.simulation) throw Error('SIM buttons are disabled on real hardware; use physical switches');
    // Simulation only: StaticIO's existing ideal-output echo model provides input changes.
    if (!StaticIO.setPin(pin, level)) throw Error(StaticIO.lastError);
    note('inputLog', 'SIM DIO' + pin + ' = ' + level + ' (no physical outputs)');
}
// Page 01 — explicit lifecycle, manual reads, one native snapshot per poll.
// Designer event: select monitorStart -> Properties -> Events -> clicked
function onMonitorStartClicked() {
    return action(armMonitor);
}
// Designer event: select monitorStop -> Properties -> Events -> clicked
function onMonitorStopClicked() {
    return action(stopMonitor);
}
// Designer event: select monitorStatsButton -> Properties -> Events -> clicked
function onMonitorStatsButtonClicked() {
    return action(showStats);
}
// Designer event: select monitorRead -> Properties -> Events -> clicked
function onMonitorReadClicked() {
    return action(() => {
        const r = StaticIO.status();
        if (!r.valid) throw Error(r.lastError);
        ui.set('inputState', 'Manual snapshot DIO0=' + ((r.inputMask >> 0) & 1) +
            '  DIO1=' + ((r.inputMask >> 1) & 1));
        note('inputLog', 'Manual status: 0x' + r.inputMask.toString(16));
    });
}
// Designer event: select monitorClear -> Properties -> Events -> clicked
function onMonitorClearClicked() {
    return action(() => {
        stopMonitor();
        StaticIO.clearAllOnChange();
        note('inputLog', 'All input callbacks unregistered');
    });
}
// Designer event: select simStartHigh -> Properties -> Events -> clicked
function onSimStartHighClicked() {
    return action(() => simulateInput(0, 1));
}
// Designer event: select simStartLow -> Properties -> Events -> clicked
function onSimStartLowClicked() {
    return action(() => simulateInput(0, 0));
}
// Designer event: select simFixtureHigh -> Properties -> Events -> clicked
function onSimFixtureHighClicked() {
    return action(() => simulateInput(1, 1));
}
// Designer event: select simFixtureLow -> Properties -> Events -> clicked
function onSimFixtureLowClicked() {
    return action(() => simulateInput(1, 0));
}

// Page 02 — callbacks are registered separately from native polling.
// Designer event: select callbacksRegister -> Properties -> Events -> clicked
function onCallbacksRegisterClicked() {
    return action(() => {
        registerInputs();
        note('callbackLog', 'Callbacks registered — polling state is ' + StaticIO.monitoring);
    });
}
// Designer event: select callbacksClearOne -> Properties -> Events -> clicked
function onCallbacksClearOneClicked() {
    return action(() => {
        StaticIO.clearOnChange(1);
        note('callbackLog', 'DIO1 callback removed; DIO0 remains');
    });
}
// Designer event: select callbacksClearAll -> Properties -> Events -> clicked
function onCallbacksClearAllClicked() {
    return action(() => {
        StaticIO.clearAllOnChange();
        note('callbackLog', 'All JS callbacks removed; native polling not changed');
    });
}
// Designer event: select callbacksStart -> Properties -> Events -> clicked
function onCallbacksStartClicked() {
    return action(armMonitor);
}
// Designer event: select callbacksStop -> Properties -> Events -> clicked
function onCallbacksStopClicked() {
    return action(stopMonitor);
}

// Page 03 — optional physical START event invokes the native, project-owned sequence.
// Designer event: select stationArm -> Properties -> Events -> clicked
function onStationArmClicked() {
    return action(() => {
        if (!StaticIO.monitoring) armMonitor();
        autoStartArmed = true;
        ui.set('stationStatus', 'ARMED — DIO0 falling edge starts TestRunner');
        note('stationLog', 'Physical START auto-run armed; DIO1 interlock checked in test');
    });
}
// Designer event: select stationDisarm -> Properties -> Events -> clicked
function onStationDisarmClicked() {
    return action(() => {
        autoStartArmed = false;
        ui.set('stationStatus', 'DISARMED — DIO0 events no longer start a test');
        note('stationLog', 'Auto-start disabled; monitor left in its current state');
    });
}
// Designer event: select stationRun -> Properties -> Events -> clicked
function onStationRunClicked() {
    return action(() => {
        if (!TestRunner.run()) throw Error(TestRunner.lastError);
        note('stationLog', 'Operator requested one test run');
    });
}
// Designer event: select stationTerminate -> Properties -> Events -> clicked
function onStationTerminateClicked() {
    return action(() => {
        if (!TestRunner.terminate()) throw Error(TestRunner.lastError);
    });
}
// Designer event: select stationReset -> Properties -> Events -> clicked
function onStationResetClicked() {
    return action(() => {
        if (!TestRunner.reset()) throw Error(TestRunner.lastError);
    });
}
TestRunner.onFinished(function () {
    const s = TestRunner.summary();
    ui.set('stationStatus', 'Last result: ' + s.state + ' / ' + s.passed + ' passed / ' + s.failed + ' failed');
});
// Panel startup: register ordinary ui.on handlers ONLY. No SDK polling or output change.
ui.set('monitorState', 'IDLE / monitor OFF; press START MONITOR');
ui.set('inputState', 'No digital samples acquired by monitor');
ui.set('fixtureState', 'Fixture state not yet observed');
ui.set('stationFixture', 'DIO1 fixture interlock: not yet observed');
ui.set('stationStatus', 'DISARMED — explicit ARM required');
