// ┌─ WFScript LEARNING LAB: Automated Test Bench — Measurement Results
// │ Level: Beginner–Intermediate
// │
// │ GOAL: Display measurement PASS/FAIL verdicts and maintain a compact table of results.
// │ BEFORE RUN: Simulation-only; no native instrument operations.
// │ TRY FIRST: Click RUN DEMO TEST.
// │ TRACE: `runDemo` → `onRunDemoClicked()` → simulated measurements → result/table widgets.
// │ EDIT SAFELY: Change one tolerance limit in the demonstration and rerun to see whether
// │    the result still passes.
// │ REMEMBER: A measurement value and its acceptance verdict should be distinguishable and
// │    traceable.
// │
// │ EVENT TIP: Select `runDemo` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/AutomatedTestBench.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * WFScript Example: Automated Test Bench (simulation only)
 *
 * Demonstrates two new widgets in v5.6.53:
 *   Measurement Result: numeric ui.set() evaluates inclusive limits.
 *   Measurement Table: ui.set() accepts an array of test-result objects.
 *
 * This project has TWO Pages using ONE shared JavaScript runtime.
 * Clicking a button on the Summary Page updates the Result Table on
 * the other Page, even while that Page is hidden.
 *
 * Simulation: no SDK calls, no voltage outputs, no DigitalOut activity.
 * Wiring: None. No AD3 or external connection is required.
 * All readings below are illustrative simulated data, not AD3 data.
 */

// CONFIGURATION ----------------------------------------------------------
// Keep limits explicit, together with the scenario numbers. The widget
// Property Inspector separately owns the same thresholds for its badge.
const limits = {
    positive: {
        name: 'V+ SUPPLY',
        low: 3.25,
        high: 3.35,
        unit: 'V'
    },
    negative: {
        name: 'V− SUPPLY',
        low: -3.35,
        high: -3.25,
        unit: 'V'
    },
    clock: {
        name: 'CLOCK',
        low: 990,
        high: 1010,
        unit: 'Hz'
    },
    pwm: {
        name: 'PWM DUTY',
        low: 49,
        high: 51,
        unit: '%'
    }
};

const targets = [
    {
        id: 'resultPositive',
        key: 'positive'
    },
    {
        id: 'resultNegative',
        key: 'negative'
    },
    {
        id: 'resultClock',
        key: 'clock'
    },
    {
        id: 'resultPwm',
        key: 'pwm'
    }
];

// DATA / PRESENTATION ----------------------------------------------------
function evaluateRow(name, cfg, actual) {
    // Pass the limits to the Table, whose native widget evaluates PASS/FAIL.
    return {
        name: name,
        value: actual,
        low: cfg.low,
        high: cfg.high,
        unit: cfg.unit
    };
}

function showReadings(readings, description) {
    const rows = [];
    let failures = 0;
    targets.forEach(function (target) {
        const cfg = limits[target.key];
        const actual = readings[target.key];
        ui.set(target.id, actual);
        // native Measurement Result checks limits
        rows.push(evaluateRow(cfg.name, cfg, actual));
        if (actual < cfg.low || actual > cfg.high) failures++;
    });
    // This is a complete replacement, not an append operation. Maximum row
    // count is configurable in the Table's designer properties (up to 100).
    ui.set('measurements', rows);
    const passed = failures === 0;
    ui.set('summaryStatus', '<b>' + (passed ?
        '<span style="color:#43dd9d;">ALL TESTS PASS</span>' :
        '<span style="color:#ff6970;">TEST FAILED</span>') +
        '</b><br>' + description + ' — ' + (4 - failures) + '/4 passed');
    print('SIMULATED TEST: ' + (passed ? 'PASS' : 'FAIL') +
        ' | ' + (4 - failures) + '/4 limits satisfied');
}

function resetResults() {
    targets.forEach(function (target) {
        ui.set(target.id, { reset: true });
    });
    ui.set('measurements', { reset: true });
    ui.set('summaryStatus', 'NOT TESTED<br>Choose RUN DEMO or INJECT FAILURE.');
    print('SIMULATED TEST: results cleared; no instrument outputs touched.');
}

// PANEL EVENT HANDLERS ---------------------------------------------------
// Designer event: select runDemo -> Properties -> Events -> clicked
/**
 * CORE LESSON — Runs a synthetic measurement demonstration and populates PASS/FAIL widgets. This
 * is distinct from the native TestRunner sequence engine.
 */
function onRunDemoClicked() {
    showReadings({
        positive: 3.301,
        negative: -3.298,
        clock: 999.8,
        pwm: 50.1
    }, 'Nominal simulation');
}

// Designer event: select injectFailure -> Properties -> Events -> clicked
function onInjectFailureClicked() {
    showReadings({
        positive: 3.301,
        negative: -3.298,
        clock: 999.8,
        pwm: 47.5
    }, 'Injected PWM failure');
}

// Designer event: select resetDemo -> Properties -> Events -> clicked
function onResetDemoClicked() {
    resetResults();
}

// Deliberately do not run a test on project load. The user initiates tests.
