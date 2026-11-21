# Lab 02 — Widget Playground

**Level:** Beginner\
**Category:** Getting Started\
**Pages:** Showcase → Controls

## What you will build or observe

Understand input widgets, simulated calculations, and displays shared across Pages.

## Before you run

No equipment; all readings are synthetic.

## Follow along

1. Run the Panel and open the Controls Page.
2. Change the VOLTAGE SETPOINT, LOAD, SIMULATION MODE and ENABLE switch.
3. Return to Showcase and confirm its indicators follow the same state.
4. In Design mode inspect the `wbLoad` slider’s `valueChanged` event.

## Follow the JavaScript

`wbLoad` → `wbRefresh()` → read current settings with `ui.get()` → update gauges with `ui.set()`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Controls | `wbFrequency` (rotary_input) | `valueChanged` | `wbRefresh()` |
| Controls | `wbTarget` (numeric_stepper) | `valueChanged` | `wbRefresh()` |
| Controls | `wbMode` (option_selector) | `selectionChanged` | `wbRefresh()` |
| Controls | `wbEnable` (switch_button) | `toggled` | `wbRefresh()` |
| Controls | `wbLoad` (slider) | `valueChanged` | `wbRefresh()` |
| Controls | `wbPulse` (momentary_button) | `pressed` | `wbPressStarted()` |
| Controls | `wbPulse` (momentary_button) | `released` | `wbPressEnded()` |
| Controls | `wbReset` (button) | `clicked` | `wbResetControls()` |

## Make one small change

Change the simulated droop coefficient in `wbRefresh()` and observe how the voltage follows load.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

One calculation function can serve several input events, without a separate calculation for every widget.

## Safety and scope

Only a mathematical demonstration; gauge values are not AD3 measurements.

## If something does not work

If the showcase looks static, verify Run mode and the simulated ENABLE switch on the Controls Page.

## Related files

- **Editable project:** `examples/panels/WidgetPlayground.wfpanel`
- **Readable JavaScript:** `examples/source/WidgetPlayground.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
