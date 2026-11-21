# Lab 29 — I²C Protocol — Master and Spy

**Level:** Intermediate\
**Category:** Digital Instruments\
**Pages:** I2C Protocol

## What you will build or observe

Configure a native I²C master, issue a register read and inspect Spy traffic.

## Before you run

Use Simulation for API exploration. Physical I²C needs voltage-compatible pull-ups, SDA/SCL and common ground.

## Follow along

1. Press CONFIGURE.
2. Set the intended address and try READ REG 0x00 with a real target.
3. Try START SPY and POLL SPY to inspect bus activity.
4. Press RELEASE PINS.

## Follow the JavaScript

`i2cCfg` → `onI2cCfgClicked()` → `Protocol1.I2C.configure({scl:0,sda:1,...})`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| I2C Protocol | `i2cCfg` (button) | `clicked` | `onI2cCfgClicked()` |
| I2C Protocol | `i2cRead` (button) | `clicked` | `onI2cReadClicked()` |
| I2C Protocol | `i2cSpy` (button) | `clicked` | `onI2cSpyClicked()` |
| I2C Protocol | `i2cPoll` (button) | `clicked` | `onI2cPollClicked()` |
| I2C Protocol | `i2cStop` (button) | `clicked` | `onI2cStopClicked()` |

## Make one small change

Change the address only to one used by your own I²C target; consult its actual register map.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

I²C acknowledgements depend on an external slave; a generic read cannot predict every sensor’s data format.

## Safety and scope

External pull-ups are required; never drive a bus at an incompatible voltage.

## If something does not work

NACK is a meaningful device/bus response, not a JavaScript parser failure.

## Related files

- **Editable project:** `examples/panels/ProtocolI2C.wfpanel`
- **Readable JavaScript:** `examples/source/ProtocolI2C.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
