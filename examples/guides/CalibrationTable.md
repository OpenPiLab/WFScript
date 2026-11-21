# Lab 12 — Calibration Lookup — Editable Table

**Level:** Intermediate\
**Category:** Tables & Data\
**Pages:** 00 Calibration Points → 01 Lookup & Chart

## What you will build or observe

Build an editable calibration curve and use it to convert sensor input to engineering units.

## Before you run

No hardware; the input voltage and calibration points are simulated.

## Follow along

1. Select Calibration Points and edit or restore the table.
2. Click VALIDATE & PLOT.
3. Open Lookup & Chart and move Sensor input (V).
4. Try SAVE SAMPLE TO LOG.

## Follow the JavaScript

`calibrationTable` → `onCalibrationTableValueChanged(rows)`; `probeInput` → lookup and plot update.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 00 Calibration Points | `calibrationTable` (experiment_table) | `valueChanged` | `onCalibrationTableValueChanged()` |
| 00 Calibration Points | `validateCalibration` (button) | `clicked` | `onValidateCalibrationClicked()` |
| 00 Calibration Points | `restoreCalibration` (button) | `clicked` | `onRestoreCalibrationClicked()` |
| 01 Lookup & Chart | `probeInput` (slider) | `valueChanged` | `onProbeInputValueChanged()` |
| 01 Lookup & Chart | `saveSample` (button) | `clicked` | `onSaveSampleClicked()` |
| 01 Lookup & Chart | `clearSamples` (button) | `clicked` | `onClearSamplesClicked()` |

## Make one small change

Change one calibration point slightly, validate, then compare the lookup output at the same input.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Calibration is a data transformation; table validation must precede interpolation.

## Safety and scope

Do not apply this illustrative calibration to a real sensor without verifying units and range.

## If something does not work

If validation fails, check duplicate input values and point ordering.

## Related files

- **Editable project:** `examples/panels/CalibrationTable.wfpanel`
- **Readable JavaScript:** `examples/source/CalibrationTable.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
