// ┌─ WFScript LEARNING LAB: SHARED PANEL DATA
// │ Level: Beginner — no external hardware
// │
// │ GOAL: Share seven named variables between controls and displays without repeatedly
// │       calling ui.set() for every widget.
// │ BEFORE RUN: Choose Simulation. Nothing here connects to a motor or supply.
// │ TRY FIRST: Set READY, change Target RPM and click START while watching
// │            speedDial, speedDisplay and speedGauge change on the same Page.
// │ TRACE: Select speedDisplay → Properties → Value Binding (Motor.ActualRPM).
// │        Then find Data.onChange below: native inputs and Data.set both notify.
// │ EDIT SAFELY: Change the 0.12 response coefficient, then Run again.
// │ REMEMBER: Designer-managed button Events call the named functions below.
// │           Input value bindings update Panel Data BEFORE their event handler.
// └──────────────────────────────────────────────────────────────────────

/*
 * One .wfpanel holds one live workbench Page, one JavaScript runtime, and
 * typed Panel Data variable definitions saved under the top-level panelData.
 *
 * Project-owned variables (Properties → Panel Data):
 *   Motor.TargetRPM  number   1200    Set by targetInput (left side)
 *   Motor.ActualRPM  number   0       Set by simulationTick()
 *   Motor.TargetRPS  number   20      Computed from Motor.TargetRPM (read-only at runtime)
 *   Motor.LoadPct    number   40      Set by loadSlider (left side)
 *   Motor.Ready      boolean  false   Set by readySwitch (left side)
 *   Motor.Mode       text     Auto    Set by modeSelector (left side)
 *   Motor.Status     text     Stopped Set by our functions
 *
 * No physical motor commands or automatic hardware writes are involved.
 * Value bindings are optional. Existing unbound widgets still use ui.set().
 * Data.onChange subscribers react to both operator edits and Data.set calls.
 * Data.computed creates a read-only target that follows declared dependencies.
 */

// Motor.TargetRPS exists in Properties → Panel Data as a Number. The formula
// makes it read-only during Run and updates its bound targetReadout automatically.
// This is revolutions/second, not the output frequency of real motor hardware.
Data.computed('Motor.TargetRPS', ['Motor.TargetRPM'], function (rpm) {
    return Number((rpm / 60).toFixed(2));
});

let modelTimer = null;

// Live, observable behavior: this unbound instructional Label shows every
// target update, whether it came from the targetInput or Data.set() on RESET.
// immediate:true also shows the saved initial value when the Panel starts.
const targetWatcher = Data.onChange('Motor.TargetRPM', function (value, previousValue, name) {
    const origin = previousValue === undefined ? 'initial' : 'changed from ' + previousValue;
    ui.set('hintControls', 'LIVE RESULTS  |  TARGET ' + value + ' RPM = ' + Data.get('Motor.TargetRPS') + ' rev/s (' + origin + ')');
    print('Data event: ' + name + ' = ' + value + ' [' + origin + ']');
}, { immediate: true });

// A simple, simulation-only interlock: turning READY off while running
// stops the synthetic timer. This does NOT control any physical output.
const readyWatcher = Data.onChange('Motor.Ready', function (isReady) {
    if (!isReady && modelTimer !== null) {
        stopMotorSimulation();
        Data.set('Motor.Status', 'READY switched off — simulated motor stopped');
        print('Data event: READY off -> simulation stopped');
    }
});

/** Runs only after an explicit START button click. */
function startMotorSimulation() {
    if (modelTimer !== null) return;
    if (!Data.get('Motor.Ready')) {
        Data.set('Motor.Status', 'Turn READY on before starting.');
        return;
    }
    Data.set('Motor.Status', 'Running — simulated motor, no hardware output');
    modelTimer = setInterval(simulationTick, 250);
    print('Simulated motor started. Data variables: ' + Data.names().join(', '));
}

/** Apply a simple first-order response and publish ONE measured-speed value. */
function simulationTick() {
    const target = Data.get('Motor.TargetRPM');
    const load = Data.get('Motor.LoadPct');
    const previous = Data.get('Motor.ActualRPM');
    const achievable = target * (1 - 0.0012 * load);
    const actual = previous + 0.12 * (achievable - previous);

    // The speedDisplay, speedDial and speedGauge are ALL bound to this value.
    // Their appearance and units are still configured per-widget.
    Data.set('Motor.ActualRPM', Number(actual.toFixed(1)));
}

/** Cleanup always clears the interval; no hardware is configured. */
function stopMotorSimulation() {
    if (modelTimer !== null) {
        clearInterval(modelTimer);
        modelTimer = null;
    }
    Data.set('Motor.ActualRPM', 0);
    Data.set('Motor.Status', 'Stopped — simulation timer released');
}

/** One example of programmatically changing an *editable* binding. */
function resetMotorSimulation() {
    stopMotorSimulation();
    Data.set('Motor.TargetRPM', 1200);
    Data.set('Motor.LoadPct', 40);
    Data.set('Motor.Ready', false);
    Data.set('Motor.Mode', 'Auto');
    // targetInput, loadSlider, readySwitch and modeSelector refresh silently.
    // A programmatic update is NOT treated as an operator click.
}

/** Optional widget event: the mode selection changes the shared string. */
function onModeChanged(text) {
    // The two-way binding has already updated Motor.Mode at this point.
    print('Mode selected: ' + Data.get('Motor.Mode'));
}

/** Optional widget event: the operator changed the requested RPM. */
function onTargetChanged(value) {
    // This is the target, not the simulated actual speed; START remains explicit.
    print('Requested speed: ' + Data.get('Motor.TargetRPM') + ' RPM');
}

function onShutdown() {
    // Shutdown hook is enabled in this project's lifecycle configuration.
    // The native timer manager will also clean up timers on abrupt stop.
    if (modelTimer !== null) clearInterval(modelTimer);
    modelTimer = null;
    Data.offChange(targetWatcher);
    Data.offChange(readyWatcher);
}

print('Panel Data Lab ready. On Live Workbench: turn READY on, adjust target RPM, then START.');
