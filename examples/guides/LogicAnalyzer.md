# Lab 20 — Logic Analyzer

**Level:** Intermediate\
**Category:** Digital Instruments\
**Pages:** Logic Analyzer

## What you will build or observe

Capture DigitalIn samples and display transitions with configurable rate and depth.

## Before you run

Simulation first. For a physical loopback use DIO0 → 1 kΩ → DIO1.

## Follow along

1. Choose SAMPLE RATE and CAPTURE SAMPLES.
2. Click CAPTURE INPUTS.
3. Optionally use RUN DIO0 → DIO1 TEST after checking wiring.
4. Use CLEAR CHART to reset visualization.

## Follow the JavaScript

`liCapture` → `liCaptureOnly()` → DigitalIn acquisition and chart update; test uses explicit output activation.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Logic Analyzer | `liRate` (option_selector) | `selectionChanged` | `liUpdateInfo()` |
| Logic Analyzer | `liSamples` (option_selector) | `selectionChanged` | `liUpdateInfo()` |
| Logic Analyzer | `liCapture` (button) | `clicked` | `liCaptureOnly()` |
| Logic Analyzer | `liLoopback` (button) | `clicked` | `liLoopback()` |
| Logic Analyzer | `liClear` (button) | `clicked` | `onLiClearClicked()` |

## Make one small change

Change sample rate and compare the resulting time window with the same sample count.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Sample count divided by sampling rate is the acquisition duration; output loopback is optional.

## Safety and scope

Confirm voltage compatibility and series resistance before digital output loopback.

## If something does not work

An empty trace may simply mean no changing input signal was present during capture.

## Related files

- **Editable project:** `examples/panels/LogicAnalyzer.wfpanel`
- **Readable JavaScript:** `examples/source/LogicAnalyzer.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
