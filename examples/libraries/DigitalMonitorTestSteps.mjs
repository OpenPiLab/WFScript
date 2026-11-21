// LEARNING TIP: verifyFixture() returns 1 for a closed active-low DIO1. It does not operate an interlock or configure digital outputs.

/** DigitalMonitorTestSteps.mjs — data-only test definition functions.
 * No outputs are configured; this is a fixture interlock demonstration.
 * A physical test fixture should use hardware safety interlocks independently.
 */
export function setupFixture() {
    print('[Digital Test] Setup: verify fixture input without changing DIO directions');
    return true;
}
export function verifyFixture() {
    const state = StaticIO.readPin(1);
    // separate manual SDK snapshot
    if (state < 0) throw Error(StaticIO.lastError);
    // Active-low switch: CLOSED -> 0 -> result 1 (PASS).
    const closed = state === 0 ? 1 : 0;
    print('[Digital Test] Main: fixture closed=' + !!closed + ' (DIO1=' + state + ')');
    return closed;
}
export function cleanupFixture() {
    // No Wavegen, Scope, Protocol or outputs were driven in this example.
    print('[Digital Test] Cleanup complete; monitor lifecycle stays with operator');
    return true;
}
