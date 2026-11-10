// PORTABLE SUBSET: avoids assumptions about repeated-run scheduling.
// HARDWARE: W1 to CH1 loopback (shared reference), with a known safe load.
Wavegen1.Channel1.enable = true;
Wavegen1.Channel1.Mode.text = "Simple";
Wavegen1.Channel1.Simple.Type.text = "Sine";
Wavegen1.Channel1.Simple.Frequency.value = 1000;
Wavegen1.Channel1.Simple.Amplitude.value = 0.25;
Wavegen1.Channel1.Simple.Offset.value = 0.50;
Scope1.Trigger.Trigger.text = "None";
Wavegen1.run(); // Command return is intentionally ignored for portability.
if (!wait(0.15)) throw new Error("Cancelled");
for (var i = 0; i < 5; ++i) {
    Scope1.single();
    if (!Scope1.wait(5)) throw new Error("Scope capture failed or timed out");
    var dc = Scope1.Channel1.measureCapture("Average");
    print("Step", i, "offset", Wavegen1.Channel1.Simple.Offset.value, "mean", dc);
    Wavegen1.Channel1.Simple.Offset.value -= 0.25 * dc;
    Wavegen1.config(); // Apply offset for both runtimes; no return-value dependency.
}
Wavegen1.stop();
