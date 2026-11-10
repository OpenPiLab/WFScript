/**
 * TestSteps.mjs — the ONLY test implementation file.
 *
 * Each exported function does ONE thing and returns the measurement or result.
 * The editor owns order, timing, acceptance limits and cleanup policy.
 * Qt/C++ owns scheduling, comparisons, statuses, and the test report.
 * You can edit these functions without changing the operator Panel.
 */

export function verifyPositive() {
    return 3.301;        // In hardware: read the actual supply voltage.
}

export function verifyNegative() {
    return -3.298;
}

export function beginClock() {
    print('Clock acquisition started');
}

export function pollClock(stepElapsedMs) {
    if (stepElapsedMs < 180) return undefined; // Still measuring.
    return 999.8;      // Numeric result: C++ checks saved limits.
}

export function endClock(status, elapsedMs) {
    print('Clock verification:', status, elapsedMs + ' ms');
}

export function verifyPwm() {
    return 50.1;       // Change to 47.5 to demonstrate a real FAIL.
}

export function finalize() {
    print('All Main tests finished');
    return true;
}

export function restoreStation() {
    print('Cleanup: restore instruments and release fixture');
    return true;
}
