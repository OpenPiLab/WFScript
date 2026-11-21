# Lab 30 — CAN Protocol — Classical CAN

**Level:** Advanced\
**Category:** Digital Instruments\
**Pages:** CAN Protocol

## What you will build or observe

Configure classical CAN, transmit a frame and poll the receive side.

## Before you run

Use Simulation first. Physical CAN requires an appropriate external CAN transceiver, wiring and bus termination.

## Follow along

1. Press CONFIGURE / START RX.
2. Press TRANSMIT FRAME only with proper transceiver and bus.
3. Press POLL RX to check for frames.
4. Press RELEASE PINS.

## Follow the JavaScript

`canCfg` → `onCanCfgClicked()` → `Protocol1.CAN.configure({tx:0,rx:1,...})`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| CAN Protocol | `canCfg` (button) | `clicked` | `onCanCfgClicked()` |
| CAN Protocol | `canSend` (button) | `clicked` | `onCanSendClicked()` |
| CAN Protocol | `canRead` (button) | `clicked` | `onCanReadClicked()` |
| CAN Protocol | `canStop` (button) | `clicked` | `onCanStopClicked()` |

## Make one small change

Change the standard CAN identifier in the Panel and observe the formatting of the transmitted ID.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

CAN traffic, acknowledgement and frame reception are bus-level phenomena, not guaranteed by script execution.

## Safety and scope

**Never connect CANH/CANL to AD3 DIO pins directly.** Use a compatible transceiver.

## If something does not work

Simulation cannot prove that a frame was acknowledged by a real CAN node.

## Related files

- **Editable project:** `examples/panels/ProtocolCAN.wfpanel`
- **Readable JavaScript:** `examples/source/ProtocolCAN.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
