# Lab 33 — PC Serial — Text & Binary

**Level:** Beginner–Intermediate\
**Category:** Communications\
**Pages:** 01 PC Serial

## What you will build or observe

Discover PC COM ports, open a port and send both text and binary data.

## Before you run

A real USB serial device, suitable TX/RX loopback, or virtual COM pair is required to receive echoes.

## Follow along

1. Click DISCOVER PORTS.
2. Choose an available COM port and click OPEN.
3. Try SEND TEXT and SEND BINARY; inspect the Event Log.
4. Click CLOSE before disconnecting.

## Follow the JavaScript

`serOpen` → `openPort()` → `Serial.create()`/open; `Serial.on("data")` handles asynchronous RX.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 PC Serial | `serDiscover` (button) | `clicked` | `discoverPorts()` |
| 01 PC Serial | `serChoose` (button) | `clicked` | `choosePort()` |
| 01 PC Serial | `serOpen` (button) | `clicked` | `openPort()` |
| 01 PC Serial | `serText` (button) | `clicked` | `sendText()` |
| 01 PC Serial | `serBinary` (button) | `clicked` | `sendBinary()` |
| 01 PC Serial | `serClose` (button) | `clicked` | `closePort()` |

## Make one small change

Change the transmitted text and inspect the raw byte representation.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

PC Serial is separate from the AD3 `Protocol1.UART` digital pins.

## Safety and scope

For UART adapters match voltage levels and ground; do not confuse TTL UART with RS-232.

## If something does not work

No RX with no device/loopback is expected and does not by itself indicate an application failure.

## Related files

- **Editable project:** `examples/panels/SerialCommsLab.wfpanel`
- **Readable JavaScript:** `examples/source/SerialCommsLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
