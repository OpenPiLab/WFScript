# Lab 15 — Signal Lab — Wavegen and Scope

**Level:** Beginner–Intermediate\
**Category:** Analog Instruments\
**Pages:** Signal Lab

## What you will build or observe

Control waveform shape and amplitude; preview W1 and acquire Scope CH1.

## Before you run

Use Simulation first. For physical loopback, verify W1 to CH1+ and common ground; do not drive unknown loads.

## Follow along

1. Select W1 SHAPE and adjust frequency/amplitude.
2. Click RENDER PREVIEW; no output is needed for a preview.
3. Click START W1 only when ready; then CAPTURE CH1 and STOP W1.

## Follow the JavaScript

`sigShape` → `sigInputsChanged()` → preview or live output update; `sigStart` → explicit `onSigStartClicked()`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Signal Lab | `sigShape` (option_selector) | `selectionChanged` | `sigInputsChanged()` |
| Signal Lab | `sigFrequency` (rotary_input) | `valueChanged` | `sigInputsChanged()` |
| Signal Lab | `sigAmplitude` (numeric_stepper) | `valueChanged` | `sigInputsChanged()` |
| Signal Lab | `sigOffset` (numeric_stepper) | `valueChanged` | `sigInputsChanged()` |
| Signal Lab | `sigStart` (button) | `clicked` | `onSigStartClicked()` |
| Signal Lab | `sigStop` (button) | `clicked` | `onSigStopClicked()` |
| Signal Lab | `sigCapture` (button) | `clicked` | `onSigCaptureClicked()` |
| Signal Lab | `sigPreview` (button) | `clicked` | `onSigPreviewClicked()` |

## Make one small change

Change Sine to Square or Triangle in Simulation and compare the preview; only then try physical mode.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

The requested settings and acquired measurements have different meanings; live reconfiguration errors stop W1.

## Safety and scope

Hardware can be driven only by explicit START; respect the conservative amplitude-plus-offset limit.

## If something does not work

If a shape changes on the preview but not hardware, inspect instrument status and the W1 configuration path.

## Related files

- **Editable project:** `examples/panels/SignalLab.wfpanel`
- **Readable JavaScript:** `examples/source/SignalLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
