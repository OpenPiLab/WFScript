// ┌─ WFScript LEARNING LAB: Widget Playground
// │ Level: Beginner
// │
// │ GOAL: Understand input widgets, simulated calculations, and displays shared across
// │    Pages.
// │ BEFORE RUN: No equipment; all readings are synthetic.
// │ TRY FIRST: Run the Panel and open the Controls Page.
// │ TRACE: `wbLoad` → `wbRefresh()` → read current settings with `ui.get()` → update gauges
// │    with `ui.set()`.
// │ EDIT SAFELY: Change the simulated droop coefficient in `wbRefresh()` and observe how the
// │    voltage follows load.
// │ REMEMBER: One calculation function can serve several input events, without a separate
// │    calculation for every widget.
// │
// │ EVENT TIP: Select `wbFrequency` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/WidgetPlayground.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * WFScript Example: Widget Playground
 * A two-Page simulation of professional widgets and shared UI state.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: None. All values are simulated.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// 01 CONTROLS — A SIMULATED BENCH WITH REAL WIDGET EVENT CALLBACKS
// ============================================================================
// This page DOES NOT connect to the AD3 or manipulate hardware. Its values are
// synthetic and demonstrate how controls and displays remain connected through
// one shared JavaScript runtime, even while other Pages are visible.
//
// Types shown: rotary_input, numeric_stepper, option_selector, switch_button,
// slider, momentary_button, precision_dial, digital_display, segmented_bar,
// status_banner, status_beacon, label, and push button.
//
// Every ui.get/ui.set address refers to a unique saved widget ID.
let wbPulseDown = false;
function wbNumber(id) {
    const n = Number(ui.get(id));
    if (!isFinite(n)) throw new Error('Invalid numeric input from ' + id);
    return n;
}
/**
 * CORE LESSON — Recomputes ALL simulated displays from the present input values. Multiple controls
 * bind to this one function, so changing any input produces a coherent new state.
 */
function wbRefresh() {
    const target = wbNumber('wbTarget');
    // 0 .. 120 V (simulated)
    const loadPct = wbNumber('wbLoad');
    // 0 .. 100 %
    const freqKhz = wbNumber('wbFrequency');
    // rotary input: 1 .. 50
    const mode = String(ui.get('wbMode'));
    // option_selector returns TEXT
    const enabled = Boolean(ui.get('wbEnable'));
    // switch_button returns boolean

    // A tiny educational 'power stage' model — NOT measured AD3 data.
    // Regulation droop increases with load; an active test pulse adds a transient.
    const droop = 1 - 0.025 * (loadPct / 100);
    const actual = enabled ? target * droop - (wbPulseDown ? Math.min(2.5, target * .05) : 0) : 0;
    const current = enabled ? 8 * (loadPct / 100) : 0;
    const power = Math.max(0, actual) * current;
    const alarm = mode === 'Alarm' ? 2 : (mode === 'Warning' || loadPct >= 85 ? 1 : 0);

    ui.set('wbGauge', actual);
    ui.set('wbVoltageReadout', actual);
    ui.set('wbCurrentReadout', current);
    ui.set('wbMeter', loadPct);
    ui.set('wbHealth', alarm);
    ui.set('wbBeacon', enabled && alarm !== 2);
    ui.set('wbStatus',
        'SIMULATED ONLY  |  ' + (enabled ? 'ENABLED' : 'DISABLED') +
        '  •  ' + actual.toFixed(2) + ' V  •  ' + current.toFixed(2) + ' A  •  ' +
        power.toFixed(1) + ' W  •  ' + freqKhz.toFixed(1) + ' kHz');

    // Page 00 intentionally shares the same state and updates when 01 changes.
    ui.set('dialVoltage', actual);
    ui.set('livePower', power);
    ui.set('voltageBar', actual);
    ui.set('supplyStatus', enabled && alarm !== 2);
}
// Every user-interface event is bound through Property Editor > Events.
// Five controls share wbRefresh; a momentary button has two named handlers.
function wbPressStarted() {
    wbPulseDown = true;
    wbRefresh();
}
function wbPressEnded() {
    wbPulseDown = false;
    wbRefresh();
}
function wbResetControls() {
    ui.set('wbTarget', 48.0);
    ui.set('wbFrequency', 25.0);
    ui.set('wbLoad', 65.0);
    ui.set('wbMode', 'Nominal');
    ui.set('wbEnable', true);
    wbPulseDown = false;
    wbRefresh();
}

// ============================================================================

// Initialize the model without generating any hardware output.
try {
    wbRefresh();
} catch (e) {
    print('Widget Playground: ' + String(e));
}
