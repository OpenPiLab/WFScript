# Lab 27 — UART Protocol — Native SDK Terminal

**Level:** Intermediate\
**Category:** Digital Instruments\
**Pages:** UART Protocol

## What you will build or observe

Use native AD3 UART commands to transmit text and inspect received data.

## Before you run

Use Simulation for API exploration. For real UART use DIO0 TX→1 kΩ→DIO1 RX or a voltage-compatible device and common ground.

## Follow along

1. Click CONFIGURE / START RX.
2. Enter text and click SEND TEXT.
3. Click POLL RX to inspect received bytes.
4. Click RELEASE PINS when finished.

## Follow the JavaScript

`uartCfg` → `onUartCfgClicked()` → Protocol1.UART configure/startRx; send and poll use separate native calls.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| UART Protocol | `uartCfg` (button) | `clicked` | `onUartCfgClicked()` |
| UART Protocol | `uartSend` (button) | `clicked` | `onUartSendClicked()` |
| UART Protocol | `uartRead` (button) | `clicked` | `onUartReadClicked()` |
| UART Protocol | `uartStop` (button) | `clicked` | `onUartStopClicked()` |

## Make one small change

Change the transmitted text and compare the UTF-8/text result with received bytes when physically looped back.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Native Digilent UART is different from PC Serial COM ports.

## Safety and scope

Never connect RS-232 voltages directly to AD3 DIO. Simulation does not fabricate an ACK or received data.

## If something does not work

If RX is empty in simulation, that is expected; actual echo requires a correctly wired physical link.

## Related files

- **Editable project:** `examples/panels/ProtocolUART.wfpanel`
- **Readable JavaScript:** `examples/source/ProtocolUART.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
