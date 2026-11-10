// WFScript v5.6.4: open in Script Editor and experiment (do not run on hardware).
// Ctrl+/ toggles line comments. Tab indents selection, Shift+Tab outdents.
// Brackets, quotes and Enter between {} should pair/indent automatically.
// Use Ctrl+Shift+Space while typing function arguments for signature help.

function setup56(f1, amp1, f2, amp2) {
    for (var i = 1; i <= 2; i++) {
        var ch = i === 1 ? Wavegen.Channel1 : Wavegen.Channel2;
        ch.enable = true;
        ch.Simple.Type.text = "Sine";
        ch.Simple.Frequency.value = i === 1 ? f1 : f2;
        ch.Simple.Amplitude.value = i === 1 ? amp1 : amp2;
    }
}

// Try deleting Sine and typing a quote after = to see allowed waveform shapes.
// Try Scope.Channel1.measure(" to see supported measurement names.
// Try typing Scope.Channel1.measureWindow( for signature help.
function measurementExample() {
    var rms = Scope.Channel1.measure("RMS");
    var fft = Scope.Channel1.spectrum(16384, "Hann");
    return { rms: rms, peak: fft.peakFrequencyHz };
}
