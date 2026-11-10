// PORTABLE return-value contract; adapted from Digilent's feedback example.
// Requires a safe W1 -> CH1 loopback with shared reference.
// WaveForms run()/single()/config() need not return booleans; wait() does.
// The Scope.run() background scheduler is intentionally not relied upon:
// only the next completed acquisition is inspected after each wait().
Wavegen.Channel1.enable = true;
Wavegen.Channel1.Mode.text = "Simple";
Wavegen.Channel1.Simple.Type.text = "Sine";
Wavegen.Channel1.Simple.Frequency.value = 1000;
Wavegen.Channel1.Simple.Amplitude.value = 0.25;
Wavegen.Channel1.Simple.Offset.value = 0.5;
Scope.Trigger.Trigger.text = "None";

Wavegen.run();
if (!wait(0.15)) throw new Error("Stopped during output settling");
Scope.run();
for (var idx = 0; idx < 10 && wait(0.1); idx++) {
    if (!Scope.wait(5)) throw new Error("Stopped or acquisition timeout");
    var middle = Scope.Channel1.measure("Middle");
    var average = Scope.Channel1.measure("Average");
    print(idx + " Middle: " + middle + " V Average: " + average + " V");
    Wavegen.Channel1.Simple.Offset.value -= middle / 2;
    // WFScript currently stages parameter values until .config() is called.
    // Explicit config() is supported by both implementations.
    Wavegen.config();
}
Scope.stop();
Wavegen.stop();
