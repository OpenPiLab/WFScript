/**
 * WFScript Example: v5.6.56 — C++ Test Executive (simulation only)
 * Simulation: software only; no device connection is required.
 * Wiring: None. The example never operates AD3 outputs.
 *
 * The TestRunner QObject lives on the Panel's Qt worker thread. C++ OWNS
 * execution order, step state, elapsed time, measurement limits, failure
 * policy, results, and updates to the four visual test widgets.
 * JavaScript only DECLARES individual step functions and supplies readings.
 *
 * RUN NOMINAL         -> all six steps PASS, four accepted measurements.
 * INJECT PWM FAILURE  -> 5/6 steps processed, PWM fails, summary stays pending.
 * NEXT STEP           -> execute ONE step on each click (C++ controlled).
 * RESET               -> reset the C++ executive without losing definitions.
 *
 * No AD3 hardware operations are called by this demonstration.
 * JS callbacks execute synchronously on the existing worker thread. Native
 * GUI Stop interrupts long-running JavaScript via QJSEngine. Deadlines,
 * retries, non-blocking asynchronous step APIs and reports are future work.
 */

const nominalReadings={positive:3.301,negative:-3.298,clock:999.8,pwm:50.1};
let injectPwmFailure=false;

function requireRunner(ok) {
    if(!ok)throw new Error('TestRunner: '+TestRunner.lastError);
}

// Connections between native executive outputs and existing Panel widgets.
requireRunner(TestRunner.configure({
    title:'Controller Functional Test',
    sequenceId:'stationSequence',
    tableId:'stationTable',
    logId:'stationLog',
    summaryId:'stationSummary',
    stopOnFailure:true
}));

requireRunner(TestRunner.add({id:'initialize',name:'Initialize test station',
    description:'Prepare test state'},function() {
    // This callback would initialize any attached instruments in a real test.
    return true;
}));

requireRunner(TestRunner.add({id:'positive',name:'Verify V+ supply',
    description:'Expected +3.25 to +3.35 V'},function() {
    return TestRunner.measure('V+ SUPPLY',nominalReadings.positive,
                              3.25,3.35,'V','resultPositive');
}));

requireRunner(TestRunner.add({id:'negative',name:'Verify V− supply',
    description:'Expected −3.35 to −3.25 V'},function() {
    return TestRunner.measure('V− SUPPLY',nominalReadings.negative,
                              -3.35,-3.25,'V','resultNegative');
}));

requireRunner(TestRunner.add({id:'clock',name:'Verify clock frequency',
    description:'Expected 990 to 1010 Hz'},function() {
    return TestRunner.measure('CLOCK',nominalReadings.clock,
                              990,1010,'Hz','resultClock');
}));

requireRunner(TestRunner.add({id:'pwm',name:'Verify PWM duty cycle',
    description:'Expected 49 to 51%'},function() {
    const duty=injectPwmFailure?47.5:nominalReadings.pwm;
    return TestRunner.measure('PWM DUTY',duty,49,51,'%','resultPwm');
}));

requireRunner(TestRunner.add({id:'report',name:'Finalize test summary',
    description:'Review all measurements'},function() {
    const rows=TestRunner.results();
    if(rows.length!==4)return false;
    return true;
}));

function runStation(injectFailure) {
    // A full-run button deliberately starts from step one, even after NEXT.
    requireRunner(TestRunner.reset());
    injectPwmFailure=injectFailure;
    const ok=TestRunner.run();
    const info=TestRunner.summary();
    print('C++ TEST EXECUTIVE:',info.state,
          '| steps',info.completedSteps+'/'+info.totalSteps,
          '| measurements',info.passedMeasurements+' PASS / '+info.failedMeasurements+' FAIL');
    return ok;
}

ui.on('stationNext','clicked',function() {
    injectPwmFailure=false;
    TestRunner.next();
});
ui.on('stationPass','clicked',function() { runStation(false); });
ui.on('stationFail','clicked',function() { runStation(true); });
ui.on('stationReset','clicked',function() {
    requireRunner(TestRunner.reset());
    injectPwmFailure=false;
});

// Registration publishes PENDING steps, but never starts a test on startup.
