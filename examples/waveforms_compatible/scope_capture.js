// PORTABLE SUBSET: intended for both Digilent WaveForms 3.25.1 Script and WFScript v5.2.
// Captures CH1 without enabling any output. The input signal is application-dependent.
Scope1.Time.Rate.value = 1000000;
Scope1.Time.Samples.value = 4096;
Scope1.Trigger.Trigger.text = "None";
Scope1.single(); // WaveForms returns undefined; WFScript returns bool.
if (!Scope1.wait(5)) throw new Error("Scope acquisition timed out or was stopped");
var samples = Scope1.Channel1.getData();
print("Captured", samples.length, "samples");
print("CH1 RMS:", Scope1.Channel1.measureCapture("RMS"));
print("CH1 Peak2Peak:", Scope1.Channel1.measureCapture("Peak2Peak"));
