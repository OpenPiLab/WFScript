// Hardware-free demonstration of the RC1.5.9 native callback contract.
// The editor defines the step parameters, result limits, timing and sequence.
export function setup(step) {
    // Simulate asynchronous preparation; completing an action with true.
    setTimeout(function(){
        if (!TestRunner.isActive(step.token)) return;
        TestRunner.complete(step.token, true);
    }, 50);
}

export function readVoltage(step) {
    var voltage=Number(step.params.voltage);
    setTimeout(function(){
        // Never issue follow-up commands from an old or canceled callback.
        if (!TestRunner.isActive(step.token)) return;
        // Here a real driver would return its measured voltage.
        TestRunner.complete(step.token, voltage - 0.001);
    }, 120);
}

export function cleanup(step) {
    // In physical tests: send output OFF and verify the actual OFF readback.
    setTimeout(function(){
        if (!TestRunner.isActive(step.token)) return;
        TestRunner.complete(step.token, true);
    }, 60);
}
