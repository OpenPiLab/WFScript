// v5.6.5 — EXERCISE THE EDITOR; this fixture does not need to be run.
// F1 on a known member: Wavegen.Channel1.Simple.Frequency.value
// Type wgs + Ctrl+Space: wgsine snippet; Enter inserts; Tab changes values.
// Type scope + Ctrl+Space: choose scopecapture or scopefft.
// Completion documentation: put caret after Scope.Channel1. and press Ctrl+Space.

function testDocs() {
    var ch = Wavegen.Channel1;
    ch.Simple.Frequency.value = 1000; // F1 shows Frequency units.
    var rms = Scope.Channel1.measure("RMS"); // F1 on measure.
    print(rms);
}
