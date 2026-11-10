# WFScript v5.6.70.3 — Table Recipes Examples

## What changed

- **Examples → Analog → Table-Driven Wavegen — Multi-Trace Sweep** bundles the 3-Page
  multi-trace example physically verified on an AD3 on 2026-11-09. No waveform
  capture or test-code changes were made. The only script change is a comment
  at its beginning identifying it as a bundled example.
- **Examples → Getting Started → Calibration Lookup — Editable Table** demonstrates
  a second usage for the same `experiment_table` widget: a validated sensor
  voltage / temperature calibration table, piecewise-linear interpolation,
  an XY plot, an input slider, and read-only sample log (CSV-exportable).
- Both use native JavaScript arrays from `ui.get(tableId)` (v5.6.70.2 API).
- Standalone `.wfpanel` versions are supplied and completely self-contained.
- No changes to `.wfpanel` schema (v2), AD3 drivers, QJSEngine runtime,
  TestRunner, plot code, widgets, reporting, or existing examples.

## Test on Windows / Qt 6.9.2 MinGW

1. Compile v5.6.70.3 using the same known-working procedure as v5.6.70.2.
   Open **Examples → Getting Started → Calibration Lookup — Editable Table**.
2. Press Run Panel. On Page 00, edit a calibration point (e.g. change
   `2.00|50.0` to `2.00|70.0`) and press **VALIDATE & PLOT**.
3. Page 01: at 2.00 V the displayed result should be 70.0 °C.
   At 2.50 V, expected result is 80.0 °C when the 3.00 V row is 90.0 °C.
4. **SAVE SAMPLE TO LOG** should append a row. Right-click the result table
   to export CSV. Deliberately enter a duplicate voltage and verify that the
   parser rejects it without altering the editable table.
5. Select **Examples → Analog → Table-Driven Wavegen — Multi-Trace Sweep**.
   First use Simulation → PREVIEW ALL (NO I/O): distinct MODEL ONLY traces
   must be retained on the Panel plot.
6. Optionally in Physical Device mode, only on safe W1→CH1+ / GND→CH1− loopback,
   manually select **RUN ALL + CH1 CHECK**. Expect one retained real CH1
   trace per enabled row and corresponding measured RMS in the results table.
   W1 must be OFF after a sweep; confirm manually.
7. Reopen the old standalone multi-trace `.wfpanel` and the v5.6.70.2 native
   JavaScript Values example to verify they still open.

## Editing table values and persistence

`ui.get('calibrationTable')` returns a real nested JS array. `ui.set()` writes
rows back to a table. Operator edits while the panel is running are **runtime
state** and are not automatically written to the project file. To change the
saved default rows, stop the Panel, select the Experiment Data Table in the
Designer, edit **Initial Rows** in the Properties inspector, and save the `.wfpanel`.

No hardware is accessed from the Calibration example. The Wavegen example's
PREVIEW ALL operation is simulation-only even with a device connected; physical
output requires using the explicitly named hardware controls.

## Other reusable table recipe patterns

* Parameter matrix for automated tests: rows of setpoints, low/high limits and dwell
* DAC/supply ramp: commanded voltage and dwell, with careful output limits
* Motion profile: target position, speed and acceleration (requires a controller)
* Digital sequence or protocol tests: pattern data and sample-rate configuration
* Calibration correction table: measured raw readings and reference units

These are potential applications of the existing general table control, not
additional hardware implementations in v5.6.70.3.

## Verification boundary

Source-level, Node JavaScript mocks and resource consistency checks can execute
in this container; Windows Qt compilation and real AD3 runs cannot. The
physically observed prior example is retained, but v5.6.70.3 is not claimed
as hardware-validated until tested by the user.
