# Lab 26 — AD3 Hardware Test Station — W1 to CH1

**Level:** Advanced\
**Category:** Test Sequences & Executive\
**Pages:** 00 Wiring & Guide → 01 Run Information → 02 Live Test → 03 Measurements → 04 Event Log → 05 Run Report

## What you will build or observe

Run a real W1→Scope CH1 loopback test with native limit checking and reports.

## Before you run

Physical wiring: W1→CH1+ and AD3 GND→CH1−. Verify voltage range and no other connected circuitry.

## Follow along

1. Review Wiring & Guide in Design mode.
2. Inspect Test Sequence, run fields and the `AD3LoopbackSteps.mjs` library.
3. With verified wiring choose Physical Device and use RUN AD3 LOOPBACK TEST.
4. Inspect Measurements, Event Log and Run Report; stop/reset after testing.

## Follow the JavaScript

`uutRun` → `onUutRunClicked()` → `TestRunner.run()`; module `AD3LoopbackSteps.mjs` configures W1, captures CH1 and cleans up.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Run Information | `uutRun` (button) | `clicked` | `onUutRunClicked()` |
| 01 Run Information | `uutReset` (button) | `clicked` | `onUutResetClicked()` |
| 02 Live Test | `stationRun` (button) | `clicked` | `onStationRunClicked()` |
| 02 Live Test | `stationStop` (button) | `clicked` | `onStationStopClicked()` |
| 02 Live Test | `stationReset` (button) | `clicked` | `onStationResetClicked()` |
| 02 Live Test | `stationReportButton` (button) | `clicked` | `refreshReport()` |
| 05 Run Report | `reportRefresh` (button) | `clicked` | `refreshReport()` |
| 05 Run Report | `reportExportJson` (button) | `clicked` | `onReportExportJsonClicked()` |
| 05 Run Report | `reportExportCsv` (button) | `clicked` | `onReportExportCsvClicked()` |

## Make one small change

Change only a saved acceptance limit in Test Sequence, then compare the reported verdict after a safe capture.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Test execution order, timeouts and limits belong to the project test definition, not to duplicate JavaScript configuration.

## Safety and scope

**Real hardware is driven.** Confirm loopback, range and output shutdown; never treat simulator values as physical PASS.

## If something does not work

If acquisition fails, inspect the native log and hardware wiring before raising limits or skipping a step.

## Related files

- **Editable project:** `examples/panels/AD3HardwareTestStation.wfpanel`
- **Readable JavaScript:** `examples/source/AD3HardwareTestStation.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
