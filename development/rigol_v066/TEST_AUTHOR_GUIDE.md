# Creating and editing this Rigol test

This guide is for a test developer who opens the Panel without knowing its history.

## 1. What runs, and where do I edit it?

Open **05 Automated Tests**, then open the **Test Sequence Editor**. It contains the *actual saved native test definition* with all 62 steps, limits, groups, and handlers. Each step runs a **named export** from `RigolTestSteps.mjs`. The native Test Executive schedules and evaluates it, not the Panel script.

In the Panel's Libraries editor open **RigolTestSteps.mjs**. It has three parts:

1. The small start/poll adapter required by the current native TestRunner API.
2. **Actual test procedures**: `verifyInitialState`, `measureSupply`, `measureDmm`, `supplyOff`, `captureWaveform`, `analyzeWaveform`, `safeRelease`.
3. The simple named handler exports referenced by the saved Test Sequence Editor (`power500`, `dmm500`, `capture_sine_1Vpp_1000Hz`, etc.).

You should not need to find an operation in the main `RigolBench_4Instruments...js` script. That script connects the four instruments, handles operator controls, and exposes a small `RigolTestHost` interface for sharing its sessions.

## 2. A 5 V PSU step, explained

1. The saved step `psu_500` has a start handler `power500`, poll handler `pollValue`, result limits 4.90–5.10 V and 35 s timeout.
2. At the bottom of the test library, `power500()` calls `startStep(..., () => measureSupply(5.00))` (written without arrow syntax for Qt compatibility).
3. In **`measureSupply(target)`**, the library claims the output before the first asynchronous setup/enable, configures current limit/OVP/OCP, enables and verifies CH1 ON, settles for 250 ms, then reads the *measured* voltage.
4. `pollValue` returns that numeric result to the native Test Executive, which applies the saved limits and archives the measurement.
5. The separate `dmm500` step calls `measureDmm(5.00)` for an independent voltage reading. It must be read with its saved limits, not silently accepted merely because PSU readback passed.

## 3. A waveform test, explained

1. A saved `capture_...` action handler calls `captureWaveform(shape, amplitude, frequency, key)` in the test library.
2. The library checks that PSU CH1 is not owned by the run **and is physically OFF**. It verifies the generator output state before claiming ownership.
3. It configures generator CH1, checks ON, configures scope CH1/timebase/trigger, captures 1000 real scope points, and computes amplitude, frequency and AC RMS/Vpp.
4. Three following **once** numeric steps call `readMetric` for the just-captured key and apply **independent saved limits** for Vpp, frequency and shape.
5. Cleanup attempts generator and PSU OFF independently and verifies the hardware readback.

## 4. What can I change safely today?

- **Numeric limits and descriptions:** edit them directly in the saved Test Sequence Editor.
- **Step order or timeouts:** edit in the Test Sequence Editor, but do not reorder a waveform step before PSU OFF. A physical interlock now refuses that case as well.
- **Actual acquisition/procedure behavior:** edit the named procedure in `RigolTestSteps.mjs`; for example, the DMM range/NPLC or the waveform capture settling time.
- **Change one fixed voltage target:** update the corresponding named export in `RigolTestSteps.mjs` **and** the matching step name/limits in the editor. This is a current framework limitation and the reason we propose step parameters next.
- **Add a voltage or waveform condition:** add the matching handler exports and visual steps/limits. This remains more manual than it should be. Do not assume that changing one export automatically expands the test sequence.

For a reproducible external source workflow, edit `RigolBench_4Instruments_v0_6_6.js` and/or `libraries/RigolTestSteps.mjs`, then run `python development/rigol_v066/build_panel.py`. Edits to the embedded library in WFScript are stored inside the `.wfpanel` itself, so keep the editable project/source copy synchronized.

## 5. The next Test Executive API improvement

A future test step could have `{handler:'measureSupply', params:{voltage:5.00}, ...}` and native TestRunner would call the exported handler with a validated per-step context. The same handler would then serve every voltage. Another future step mode could await a returned Promise rather than using `startStep/pollValue` pairs. **Neither feature exists in RC1.5.5/RC1.5.6, and this Panel does not depend on them.** The present structure makes the migration local to the library and step definitions instead of the four-instrument Panel.
