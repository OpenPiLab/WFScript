// PORTABLE SUBSET: intended for both Digilent WaveForms 3.25.1 Script and WFScript v5.2.
// HARDWARE: connect W1 to Scope CH1 with the correct common reference.
// Select hardware mode explicitly in WFScript. Do not connect to unknown live circuits.
Wavegen1.Channel1.enable = true;
Wavegen1.Channel1.Mode.text = "Simple";
Wavegen1.Channel1.Simple.Type.text = "Sine";
Wavegen1.Channel1.Simple.Frequency.value = 1000;
Wavegen1.Channel1.Simple.Amplitude.value = 1.0;
Wavegen1.Channel1.Simple.Offset.value = 0;
Scope1.Time.Rate.value = 1000000;
Scope1.Time.Samples.value = 8192;
Scope1.Trigger.Trigger.text = "Auto";
Scope1.Trigger.Type.text = "Edge";
Scope1.Trigger.Source.text = "Channel 1";
Scope1.Trigger.Condition.text = "Rising";
Scope1.Trigger.Level.value = 0;
Scope1.Trigger.Hysteresis.value = 0.05;
Wavegen1.run(); // Do not test its return value in portable scripts.
// Example-only settling interval; this does not assert that the first-capture issue is fixed.
if (!wait(0.15)) throw new Error("Interrupted during output settling");
Scope1.single();
if (!Scope1.wait(5)) throw new Error("Capture failed or timed out");
print("Samples", Scope1.Channel1.getData().length);
print("RMS", Scope1.Channel1.measureCapture("RMS"));
print("Peak2Peak", Scope1.Channel1.measureCapture("Peak2Peak"));
print("Frequency", Scope1.Channel1.measureCapture("Frequency"));
Wavegen1.stop();
