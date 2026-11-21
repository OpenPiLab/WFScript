# Lab 18 — Static Digital I/O

**Level:** Intermediate\
**Category:** Digital Instruments\
**Pages:** Digital I/O

## What you will build or observe

Set GPIO lines to Z/LOW/HIGH and read back the 16-bit digital input mask.

## Before you run

Use Simulation first. For a physical loopback insert a separate 1 kΩ resistor between DIO0 and DIO1.

## Follow along

1. Change one DIO selector to Z, LOW or HIGH.
2. Click READ ALL INPUTS and inspect the levels.
3. With verified wiring, try TEST DIO0 → DIO1.
4. Press ALL INPUTS (Z) before leaving.

## Follow the JavaScript

16 dynamically registered `dioSelectN` selectors → `dioApplyPin(i)`; named visual buttons control read and reset.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Digital I/O | `dioAllZ` (button) | `clicked` | `dioAllInputs()` |
| Digital I/O | `dioRead` (button) | `clicked` | `onDioReadClicked()` |
| Digital I/O | `dioLoopback` (button) | `clicked` | `dioLoopback()` |

## Make one small change

Change the displayed formatting of the input-mask hex value; leave the pin ownership logic intact.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Programmatic `ui.on()` is intentionally retained here to capture each of 16 pin numbers.

## Safety and scope

DIO levels are logic signals, not power outputs. Never connect conflicting driven outputs directly.

## If something does not work

If an input is not what you expect, inspect direction (output-enable mask), pin wiring and the status read.

## Related files

- **Editable project:** `examples/panels/StaticDigitalIO.wfpanel`
- **Readable JavaScript:** `examples/source/StaticDigitalIO.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
