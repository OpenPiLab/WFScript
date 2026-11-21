# Lab 31 — SWD Protocol — Target Registers

**Level:** Advanced\
**Category:** Digital Instruments\
**Pages:** SWD Protocol

## What you will build or observe

Read a debug-port identification register from a compatible ARM SWD target.

## Before you run

Use Simulation for API structure. Physical use requires voltage-matched SWCLK, SWDIO and common ground.

## Follow along

1. Press CONFIGURE.
2. Issue LINE RESET.
3. Click READ DP IDCODE on a compatible target.
4. Press RELEASE PINS.

## Follow the JavaScript

`swdCfg` → `onSwdCfgClicked()` → configure clock/data; `swdRead` → read DP register.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| SWD Protocol | `swdCfg` (button) | `clicked` | `onSwdCfgClicked()` |
| SWD Protocol | `swdReset` (button) | `clicked` | `onSwdResetClicked()` |
| SWD Protocol | `swdRead` (button) | `clicked` | `onSwdReadClicked()` |
| SWD Protocol | `swdStop` (button) | `clicked` | `onSwdStopClicked()` |

## Make one small change

Change the clock rate only within the capabilities of your actual SWD target.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

SWD requires protocol acknowledgements and turnaround timing; a simulated read does not identify hardware.

## Safety and scope

Do not attach signals to an unknown target voltage; this example does no writes on startup.

## If something does not work

If no IDCODE appears, check target power, common ground, pin swap and SWD clock rate.

## Related files

- **Editable project:** `examples/panels/ProtocolSWD.wfpanel`
- **Readable JavaScript:** `examples/source/ProtocolSWD.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
