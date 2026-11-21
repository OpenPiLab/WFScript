// RC1.5.10: one operation may report many separately evaluated measurements.
// All values here are deterministic; no instruments are used.
function later(step, work) {
    setTimeout(function () {
        if (!TestRunner.isActive(step.token)) return;
        try { TestRunner.complete(step.token, work()); }
        catch (e) { TestRunner.fail(step.token, String(e)); }
    }, 90);
}
export function setup(step) { later(step, function () { return true; }); }

// This single result is evaluated against THREE named measurement rules.
// Additional metadata such as samples and rms is retained in the JSON report.
export function waveform(step) {
    later(step, function () {
        return {
            vpp: Number(step.params.amplitude) * 1.02,
            frequency: Number(step.params.frequency) + 0.2,
            shape: 0.352,
            rms: 0.719,
            sampleCount: 1000,
            metadata: {source: "SIMULATED", waveform: step.params.shape}
        };
    });
}

// channels[0], channels[1], and temperatures[*] are separate checks.
// The wildcard expands to one Measurement Table entry per array element.
export function channels(step) {
    later(step, function () {
        return {
            channels: step.params.channels.map(function (v) { return Number(v) - 0.001; }),
            temperatures: [24.1, 25.2, 26.3],
            note: "Array elements are evaluated by native C++, not JavaScript"
        };
    });
}

// Immediate Once handlers can also return JSON objects.
export function calculate(step) { return {gain:Number(step.params.gain), note:"Once result"}; }

// Old scalar callback behavior is unchanged.
export function legacyVoltage(step) {
    later(step, function () { return Number(step.params.voltage) - 0.001; });
}
export function cleanup(step) { later(step, function () { return true; }); }
