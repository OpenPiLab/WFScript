# WFScript 5.6.44 - Analog Discovery 3 programmable power supplies

WaveForms scripting object spelling is preserved, with a deliberate **explicit commit** requirement for safety:

```javascript
Supplies.Output.PositiveSupply.Voltage.value = 3.3;
Supplies.Output.NegativeSupply.Voltage.value = -3.3;
Supplies.Output.PositiveSupply.Enable.value = 1;
Supplies.Output.NegativeSupply.Enable.value = 1;
if (!Supplies.run()) throw Error(Supplies.lastError); // master ON + commit
var state = Supplies.status();
print(state.positiveVoltage, state.negativeVoltage, state.measuredAvailable);
// Change a live setpoint:
Supplies.Output.PositiveSupply.Voltage.value = 2.5;
if (!Supplies.config()) throw Error(Supplies.lastError);
// Always stop when finished:
if (!Supplies.stop()) throw Error(Supplies.lastError);
```

WaveForms-compatible paths: `Supplies.MasterEnable.checked`, `Supplies.Output.PositiveSupply.Enable.value`, `.Voltage.value` and corresponding negative rail. Alias `Supplies1` is also supported. The `config`, `run`, `stop`, `status`, `lastError` and `running` members are WFScript additions/common instrument conveniences, and **WFScript does not automatically apply property assignments** until `config()`/`run()` is called. This is intentionally different from the WaveForms GUI-backed property behavior.

V+ accepted range: +0.5 to +5 V. V- accepted range: -0.5 to -5 V (signed). Default is both supplies disabled, master disabled, setpoints +1/-1 V. `run()` explicitly enables the master, but does not turn on rail enables that remain 0. `stop()` disables master and also runs during standalone script completion / Panel exit (if supplies were touched). Invalid settings cannot be applied, and cause an emergency stop if outputs were running. A failed SDK configuration triggers a best-effort master-off, and errors are returned to the script via lastError.

SDK AnalogIO mapping on AD3: channel 0 V+, channel 1 V-; node 0 = enable; node 1 = requested voltage. `FDwfAnalogIOConfigure` and `FDwfAnalogIOEnableSet` are used for staged configuration and master control. `FDwfAnalogIOStatus` and `FDwfAnalogIOChannelNodeStatus` provide hardware voltage readback when available. `status().measuredAvailable` distinguishes physical measurement from mere setpoint; simulated mode is explicitly labeled. This revision intentionally does not expose total power limit or current control.

**Hardware verification**: Compile on Qt 6.9.2 / MinGW 64-bit, select Physical Device, verify V+ and V- using a multimeter and a benign load, exercise individual rail enables, master stop, voltage changes, and Panel Stop. Do not assume physical validation from source-level tests. The combined user rails share a device power budget; see Digilent AD3 power specifications. Disconnect sensitive loads for first tests.

**Important**: A script only mutating supply properties never enables physical hardware. In simulation no device library calls are issued. `status()` on a physical device with no available SDK voltage readback returns null for voltage and `measuredAvailable=false`.
