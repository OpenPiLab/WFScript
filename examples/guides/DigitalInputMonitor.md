# Lab 19 — Digital Input Test Station — Native Monitor

**Level:** Advanced\
**Category:** Digital Instruments\
**Pages:** 01 Input Monitor → 02 JavaScript Callbacks → 03 Test Station

## What you will build or observe

Monitor debounced DIO input changes and optionally trigger the Test Executive.

## Before you run

No automatic polling or output at startup. Hardware inputs are active-low with 10 kΩ pull-ups to 3.3 V, switches to ground.

## Follow along

1. Press START MONITOR and inspect stats.
2. Try simulation HIGH/LOW buttons in Simulation.
3. On JavaScript Callbacks compare registering callbacks with polling.
4. On Test Station read the fixture status; arm auto-start only deliberately.

## Follow the JavaScript

`monitorStart` → `onMonitorStartClicked()` → `StaticIO.onChange()` + `StaticIO.startMonitor()`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Input Monitor | `monitorStart` (button) | `clicked` | `onMonitorStartClicked()` |
| 01 Input Monitor | `monitorStop` (button) | `clicked` | `onMonitorStopClicked()` |
| 01 Input Monitor | `monitorStatsButton` (button) | `clicked` | `onMonitorStatsButtonClicked()` |
| 01 Input Monitor | `monitorRead` (button) | `clicked` | `onMonitorReadClicked()` |
| 01 Input Monitor | `monitorClear` (button) | `clicked` | `onMonitorClearClicked()` |
| 01 Input Monitor | `simStartHigh` (button) | `clicked` | `onSimStartHighClicked()` |
| 01 Input Monitor | `simStartLow` (button) | `clicked` | `onSimStartLowClicked()` |
| 01 Input Monitor | `simFixtureHigh` (button) | `clicked` | `onSimFixtureHighClicked()` |
| 01 Input Monitor | `simFixtureLow` (button) | `clicked` | `onSimFixtureLowClicked()` |
| 02 JavaScript Callbacks | `callbacksRegister` (button) | `clicked` | `onCallbacksRegisterClicked()` |
| 02 JavaScript Callbacks | `callbacksClearOne` (button) | `clicked` | `onCallbacksClearOneClicked()` |
| 02 JavaScript Callbacks | `callbacksClearAll` (button) | `clicked` | `onCallbacksClearAllClicked()` |
| 02 JavaScript Callbacks | `callbacksStart` (button) | `clicked` | `onCallbacksStartClicked()` |
| 02 JavaScript Callbacks | `callbacksStop` (button) | `clicked` | `onCallbacksStopClicked()` |
| 03 Test Station | `stationArm` (button) | `clicked` | `onStationArmClicked()` |
| 03 Test Station | `stationDisarm` (button) | `clicked` | `onStationDisarmClicked()` |
| 03 Test Station | `stationRun` (button) | `clicked` | `onStationRunClicked()` |
| 03 Test Station | `stationTerminate` (button) | `clicked` | `onStationTerminateClicked()` |
| 03 Test Station | `stationReset` (button) | `clicked` | `onStationResetClicked()` |

## Make one small change

Change the debounceMs value in `registerInputs()` and compare callback sensitivity in Simulation.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Registering callbacks and starting input polling are separate operations; edges are host-observed.

## Safety and scope

**Not a safety E-stop.** Do not connect industrial voltages; use independent hardwired safety interlocks.

## If something does not work

If input changes do not appear, check whether polling was started and that your edge selection matches the transition.

## Related files

- **Editable project:** `examples/panels/DigitalInputMonitor.wfpanel`
- **Readable JavaScript:** `examples/source/DigitalInputMonitor.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
