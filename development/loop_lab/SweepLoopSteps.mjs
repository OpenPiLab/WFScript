// RC1.5.11: one readable procedure per operation. The native executive
// expands setpoints/iterations and passes them via step.params.
// No hardware is required, and asynchronous callbacks carry step.token.
function later(step, produce) {
    setTimeout(function () {
        if (!TestRunner.isActive(step.token)) return;
        try { TestRunner.complete(step.token, produce()); }
        catch (err) { TestRunner.fail(step.token, String(err)); }
    }, 55);
}
export function setup(step) { later(step, function () { return true; }); }
export function readVoltage(step) {
    later(step, function () {
        return {voltage: Number(step.params.voltage)-0.001,
                current: 0.100, source: 'SYNTHETIC'};
    });
}
export function waveform(step) {
    later(step, function () {
        return {vpp: Number(step.params.amplitude)*1.02,
                frequency: Number(step.params.frequency)+0.2,
                source: 'SYNTHETIC'};
    });
}
export function diagnostic(step) {
    later(step, function () { return Number(step.params.trial); });
}
export function cleanup(step) { later(step, function () { return true; }); }
