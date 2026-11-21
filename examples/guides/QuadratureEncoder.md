# Lab 22 — Quadrature Encoder + Index

**Level:** Advanced\
**Category:** Digital Instruments\
**Pages:** Encoder

## What you will build or observe

Generate quadrature A/B with index and verify phase, direction and speed.

## Before you run

Simulation first; physical test uses separate 1 kΩ links DIO0→DIO3, DIO1→DIO4 and DIO2→DIO5.

## Follow along

1. Choose PULSES PER REVOLUTION, SPEED and DIRECTION.
2. Press PREVIEW (MATH) to inspect generated transitions.
3. With verified wiring START ENCODER then VERIFY 3× LOOPBACK.
4. STOP / RELEASE after the capture.

## Follow the JavaScript

`encStart` → `Patterns1` custom buffer output; `encVerify` → 16-bit DigitalIn capture and phase analysis.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Encoder | `encPpr` (option_selector) | `selectionChanged` | `encInfo()` |
| Encoder | `encRpm` (option_selector) | `selectionChanged` | `encInfo()` |
| Encoder | `encDirection` (option_selector) | `selectionChanged` | `encInfo()` |
| Encoder | `encStart` (button) | `clicked` | `encStart()` |
| Encoder | `encStop` (button) | `clicked` | `encStop()` |
| Encoder | `encCapture` (button) | `clicked` | `onEncCaptureClicked()` |
| Encoder | `encVerify` (button) | `clicked` | `onEncVerifyClicked()` |
| Encoder | `encPreview` (button) | `clicked` | `encPreview()` |

## Make one small change

Change the direction and identify which A/B phase relationship reverses.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

PPR, x4 edge counts and RPM are related but distinct quantities.

## Safety and scope

Only connect logic-compatible DIO pins through separate resistors; simultaneous output pins must not be tied.

## If something does not work

If direction fails verification, check A/B pin order and the configured reverse flag.

## Related files

- **Editable project:** `examples/panels/QuadratureEncoder.wfpanel`
- **Readable JavaScript:** `examples/source/QuadratureEncoder.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
