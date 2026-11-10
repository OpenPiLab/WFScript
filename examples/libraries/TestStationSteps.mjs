/**
 * Embedded library for WFScript v5.6.59 Automated Test Station.
 * This is a real ECMAScript module imported with modules.load(), NOT a
 * second QJSEngine. The TestRunner holds these exported function handles.
 * All values are simulated; nothing drives a physical instrument.
 */
const nominal={positive:3.301,negative:-3.298,clock:999.8,pwm:50.1};
let injectedPwmFailure=false;
let injectedScriptError=false;

export function setPwmFailure(enable) {
    injectedPwmFailure=Boolean(enable);
}
export function setScriptError(enable) { injectedScriptError=Boolean(enable); }

export function verifyPositive() {
    if(!TestRunner.setVariable('positiveVoltage',nominal.positive)) throw Error(TestRunner.lastError);
    return TestRunner.measure('V+ SUPPLY',nominal.positive,
                              3.25,3.35,'V','resultPositive');
}
export function verifyNegative() {
    if(injectedScriptError) throw Error('Simulated instrument read error: V− unavailable');
    if(!TestRunner.setVariable('negativeVoltage',nominal.negative)) throw Error(TestRunner.lastError);
    return TestRunner.measure('V− SUPPLY',nominal.negative,
                              -3.35,-3.25,'V','resultNegative');
}

// Called once on entering the periodic clock step.
// This could start a real acquisition through Scope1/Logic1 in a hardware test.
export function beginClock() {
    print('[Library] Clock verification started');
}

// Called by native Qt/C++ every requested 10ms (best effort).
// Undefined means NOT DONE, not PASS. Only the final call creates the result.
export function pollClock(stepElapsedMs,totalElapsedMs) {
    if(stepElapsedMs < 180) return undefined;
    return TestRunner.measure('CLOCK',nominal.clock,990,1010,'Hz','resultClock');
}
export function endClock(status,elapsedMs) {
    print('[Library] Clock verification:',status,elapsedMs+'ms');
}
export function verifyPwm() {
    const duty=injectedPwmFailure?47.5:nominal.pwm;
    return TestRunner.measure('PWM DUTY',duty,49,51,'%','resultPwm');
}
export function finalize() {
    // Demonstrates cross-step data owned by C++, not a module global.
    return TestRunner.results().length===4 &&
           TestRunner.variable('positiveVoltage')===nominal.positive &&
           TestRunner.variable('negativeVoltage')===nominal.negative;
}

// TestStand-inspired Cleanup group: C++ calls this even when PWM fails.
// Future physical station handlers must release any outputs they own here.
export function restoreStation() {
    print('[Library] Cleanup: simulated fixture resources restored');
    return true;
}
