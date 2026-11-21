// ┌─ WFScript LEARNING LAB: Panel JavaScript Timers — Interval & Timeout
// │ Level: Beginner
// │
// │ GOAL: Start, stop and cancel nonblocking timers without freezing the user interface.
// │ BEFORE RUN: Simulation-only; no hardware needed.
// │ TRY FIRST: Press START 250ms and watch the count increase.
// │ TRACE: `timerStartFast` → `startFast()` → `setInterval()`; `timerStopFast` →
// │    `stopFast()` → `clearInterval()`.
// │ EDIT SAFELY: Change the 250 ms interval to 500 ms, then compare how quickly the counter
// │    updates.
// │ REMEMBER: Widget events start/stop the timer; the timer callback is a separate
// │    asynchronous operation.
// │
// │ EVENT TIP: Select `timerStartFast` in Design mode → Properties → Events → Go to Handler.
// │ Dynamic UI events can use `ui.on` — do NOT duplicate a saved designer binding.
// │ FULL LAB GUIDE: examples/guides/PanelTimers.md (in the complete source ZIP).
// └────────────────────────────────────────────────────────────────────────────

/**
 * WFScript v0.2-dev.2 — Panel Timers Lab (simulation-safe)
 * Browser-style setInterval/clearInterval/setTimeout/clearTimeout using native
 * Qt QTimers on PanelRuntime's QJSEngine thread. No AD3 pins are accessed.
 *
 * The TWO Pages share ONE engine. Timers continue when the other Page is shown.
 * Open Panel Settings to see onInitialize/onShutdown enabled; this example is
 * intentionally safe to run in either Simulation or Physical Device mode.
 */
const TIMER_SOURCE = 'Timers';
let uptimeTimer = 0;
let fastTimer = 0;
let pendingTimeout = 0;
let clockTick = 0;
let fastTick = 0;
let startedAt = 0;
let pulse = false;

function record(message, level) {
    ui.set('timerLog', {
        append: {
            level: level || 'INFO',
            source: TIMER_SOURCE,
            message: String(message)
        }
    });
    print('[Panel Timers] ' + String(message));
}
function publish() {
    const elapsed = (Date.now() - startedAt) / 1000;
    ui.set('timerUptime', elapsed);
    ui.set('timerFastCount', fastTick);
    ui.set('timerClockCount', clockTick);
    ui.set('timerPulse', pulse);
    ui.set('timerState', (fastTimer ? '250 ms interval RUNNING' : '250 ms interval STOPPED') +
        ' | ' + (pendingTimeout ? '2 s timeout PENDING' : 'No timeout pending'));
    ui.set('guideState', 'Uptime: ' + elapsed.toFixed(1) + ' s  |  Fast ticks: ' + fastTick +
        '  |  One second ticks: ' + clockTick);
}
/**
 * CORE LESSON — Starts one repeating Qt-backed JavaScript interval. The timer is not a widget
 * event; Stop clears its ID to avoid duplicate callbacks after another Start.
 */
function startFast() {
    if (fastTimer) return;
    // no duplicate interval when button clicked twice
    fastTimer = setInterval(function () {
        fastTick++;
        pulse = !pulse;
        publish();
    }, 250);
    record('setInterval(250 ms) created timer #' + fastTimer);
    publish();
}
function stopFast() {
    if (!fastTimer) return;
    clearInterval(fastTimer);
    record('clearInterval(' + fastTimer + ') stopped the fast timer');
    fastTimer = 0;
    pulse = false;
    publish();
}
function scheduleDemo() {
    if (pendingTimeout) {
        clearTimeout(pendingTimeout);
        record('Previous one-shot cancelled before replacement', 'WARN');
    }
    pendingTimeout = setTimeout(function (message) {
        pendingTimeout = 0;
        // one-shot removed by C++ before callback
        record(message, 'PASS');
        ui.set('timerNotice', message);
        publish();
    }, 2000, '2-second one-shot fired (argument passed into callback)');
    ui.set('timerNotice', 'Waiting 2 seconds...');
    record('setTimeout(2000 ms) created timer #' + pendingTimeout);
    publish();
}
function cancelDemo() {
    if (!pendingTimeout) {
        record('No outstanding one-shot to cancel');
        return;
    }
    clearTimeout(pendingTimeout);
    record('clearTimeout(' + pendingTimeout + ') cancelled the pending callback', 'WARN');
    pendingTimeout = 0;
    ui.set('timerNotice', 'One-shot cancelled; no callback will execute');
    publish();
}

// Buttons on BOTH Pages are bound through Property Editor > Events.
// The timer callbacks themselves are still created with setInterval/setTimeout.
function resetTimerCounters() {
    fastTick = 0;
    clockTick = 0;
    record('Display counters reset (active timers unaffected)');
    publish();
}
function clearTimerLog() {
    ui.set('timerLog', { clear: true });
}

function onInitialize() {
    startedAt = Date.now();
    // Startup uses setInterval without an explicit operator action.
    uptimeTimer = setInterval(function () {
        clockTick++;
        publish();
    }, 1000);
    ui.set('timerNotice', 'Timer APIs available. Try Start 250ms and Schedule once.');
    record('onInitialize: started independent 1-second uptime timer #' + uptimeTimer);
    publish();
}
function onShutdown() {
    // Not strictly necessary (native PanelRuntime cancels every timer), but
    // explicit cleanup is good practice for reusable script modules.
    if (fastTimer) clearInterval(fastTimer);
    if (uptimeTimer) clearInterval(uptimeTimer);
    if (pendingTimeout) clearTimeout(pendingTimeout);
    fastTimer = 0;
    uptimeTimer = 0;
    pendingTimeout = 0;
    print('[Panel Timers] onShutdown: all application timers cancelled');
}
