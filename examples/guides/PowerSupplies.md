# Lab 17 — V+ / V− Power Supplies

**Level:** Intermediate\
**Category:** Analog Instruments\
**Pages:** Supplies

## What you will build or observe

Set separate V+/V− rails and watch actual readback follow changes over time.

## Before you run

Simulation first. Physical mode can energize the AD3 supply pins; verify wiring, load and safe voltage.

## Follow along

1. Set V+ and V− values and enable at least one rail.
2. Press POWER ON and watch the readouts update automatically.
3. Change a setpoint and compare the settled readback.
4. Press POWER OFF and allow the bounded readback refresh to finish.

## Follow the JavaScript

`psPowerOn` → `psStart()`; 250 ms read-only timer → `psShowStatus()` → psPlusReadout/psMinusReadout.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Supplies | `psVPlus` (numeric_stepper) | `valueChanged` | `psChanged()` |
| Supplies | `psVMinus` (numeric_stepper) | `valueChanged` | `psChanged()` |
| Supplies | `psEnablePlus` (switch_button) | `toggled` | `psChanged()` |
| Supplies | `psEnableMinus` (switch_button) | `toggled` | `psChanged()` |
| Supplies | `psPowerOn` (button) | `clicked` | `psStart()` |
| Supplies | `psPowerOff` (button) | `clicked` | `psStop()` |
| Supplies | `psRefresh` (button) | `clicked` | `onPsRefreshClicked()` |

## Make one small change

Change a setpoint while powered in Simulation and watch how the readout updates without pressing READ STATUS.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Actual readback must not be replaced by the requested setpoint. The polling timer never configures outputs.

## Safety and scope

Physical supplies are explicitly enabled; an unconfirmed stop must not be displayed as confirmed OFF.

## If something does not work

If a readout lags, distinguish hardware settlement from the 250 ms refresh; an unavailable readback is reported.

## Related files

- **Editable project:** `examples/panels/PowerSupplies.wfpanel`
- **Readable JavaScript:** `examples/source/PowerSupplies.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
