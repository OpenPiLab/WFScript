# Lab 25 — Automated Test Station — Project-Owned C++ Executive

**Level:** Advanced\
**Category:** Test Sequences & Executive\
**Pages:** 00 Test Overview → 01 Run Information → 02 Test Station → 03 Measurements → 04 Event Log → 05 Run Report → 06 Shared UUT View

## What you will build or observe

Operate a project-owned Setup/Main/Cleanup Test Executive and inspect archived reports.

## Before you run

No AD3 hardware needed; embedded `TestSteps.mjs` returns simulated measurements.

## Follow along

1. In Design mode open Test Sequence to inspect steps, limits and Run Information fields.
2. On Run Information enter required UUT fields; try a run without them to observe validation.
3. Use RUN TEST and open Measurements / Run Report.
4. Try STOP (CLEANUP), RESET and report export.

## Follow the JavaScript

`uutRun` → `onUutRunClicked()` → `TestRunner.run()`. Native C++ owns step scheduling; `TestSteps.mjs` exports measurement functions.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Run Information | `uutRun` (button) | `clicked` | `onUutRunClicked()` |
| 01 Run Information | `uutReset` (button) | `clicked` | `onUutResetClicked()` |
| 02 Test Station | `stationNext` (button) | `clicked` | `onStationNextClicked()` |
| 02 Test Station | `stationPass` (button) | `clicked` | `onStationPassClicked()` |
| 02 Test Station | `stationFail` (button) | `clicked` | `onStationFailClicked()` |
| 02 Test Station | `stationReset` (button) | `clicked` | `onStationResetClicked()` |
| 05 Run Report | `reportRefresh` (button) | `clicked` | `refreshReport()` |
| 05 Run Report | `reportExportJson` (button) | `clicked` | `onReportExportJsonClicked()` |
| 05 Run Report | `reportExportCsv` (button) | `clicked` | `onReportExportCsvClicked()` |
| 06 Shared UUT View | `mirrorRead` (button) | `clicked` | `onMirrorReadClicked()` |

## Make one small change

Change a numeric test limit in the Test Sequence Editor—not in this operator script—and observe the new verdict.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

There is **one test definition**, edited visually. The operator script only invokes actions and displays saved results.

## Safety and scope

Terminate requests cooperative cleanup; emergency abort is distinct and may skip sequence-level cleanup.

## If something does not work

If Run refuses to start, inspect required Run Information and `TestRunner.lastError` before editing scripts.

## Related files

- **Editable project:** `examples/panels/AutomatedTestStation.wfpanel`
- **Readable JavaScript:** `examples/source/AutomatedTestStation.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
