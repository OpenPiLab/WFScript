# Lab 21 — Digital Clock / PWM

**Level:** Intermediate\
**Category:** Digital Instruments\
**Pages:** Patterns

## What you will build or observe

Generate a hardware-timed clock/PWM and compare requested duty to captured duty.

## Before you run

Simulation first; physical verification uses DIO0 → 1 kΩ → DIO1.

## Follow along

1. Select OUTPUT FREQUENCY and PWM DUTY.
2. Press START CLOCK / PWM, then CAPTURE RUNNING CLOCK.
3. With loopback wiring try DIO0 → DIO1 TEST.
4. Press STOP / OUTPUT Z before disconnecting.

## Follow the JavaScript

`pgStart` → `pgSettings()` → `Patterns1.run()`; `pgCapture` → DigitalIn acquisition.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Patterns | `pgFreq` (option_selector) | `selectionChanged` | `pgInfo()` |
| Patterns | `pgDuty` (option_selector) | `selectionChanged` | `pgInfo()` |
| Patterns | `pgStart` (button) | `clicked` | `pgStart()` |
| Patterns | `pgStop` (button) | `clicked` | `pgStop()` |
| Patterns | `pgCapture` (button) | `clicked` | `onPgCaptureClicked()` |
| Patterns | `pgTest` (button) | `clicked` | `onPgTestClicked()` |

## Make one small change

Change duty from 50% to another allowed value; inspect how high and low durations should change.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

DigitalOut generates timing independently from JavaScript callbacks; DigitalIn samples it separately.

## Safety and scope

The DIO0 pin can be driven; use resistor isolation and confirm common ground.

## If something does not work

If verification fails, check rate, capture window, sampling alignment and loopback wiring.

## Related files

- **Editable project:** `examples/panels/ClockPWM.wfpanel`
- **Readable JavaScript:** `examples/source/ClockPWM.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
