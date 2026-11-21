# Lab 16 — AD3 Scope and Range Discovery

**Level:** Beginner–Intermediate\
**Category:** Analog Instruments\
**Pages:** AD3

## What you will build or observe

Acquire a Scope CH1 snapshot, compute statistics and query SDK hardware range choices.

## Before you run

Simulation first. For physical supply measurement, verify V+ → CH1+ and GND → CH1− with a safe configured range.

## Follow along

1. Click QUERY SDK RANGES to inspect supported input range steps.
2. Click Capture Scope1 CH1.
3. Compare DC mean, RMS and the trend plot.

## Follow the JavaScript

`ad3Capture` → `onAd3CaptureClicked()` → `Scope1.single()` / `wait()` / `statistics()`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| AD3 | `ad3Capture` (button) | `clicked` | `onAd3CaptureClicked()` |
| AD3 | `ad3Ranges` (button) | `clicked` | `onAd3RangesClicked()` |

## Make one small change

Reduce the sample count only after understanding acquisition duration and its effect on statistics.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

A requested software range is not necessarily a separate hardware gain step; distinguish configured and actual range.

## Safety and scope

Scope input limits apply; never assume its acquisition is isolated from connected circuitry.

## If something does not work

If acquisition fails, check the selected device, connections, and reported Scope1 error.

## Related files

- **Editable project:** `examples/panels/AD3ScopeExplorer.wfpanel`
- **Readable JavaScript:** `examples/source/AD3ScopeExplorer.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
