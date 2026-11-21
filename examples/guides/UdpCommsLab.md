# Lab 35 — UDP Local Datagram Echo — Text & Binary

**Level:** Beginner–Intermediate\
**Category:** Communications\
**Pages:** 01 Local UDP Datagrams

## What you will build or observe

Use two localhost UDP sockets to exchange individual datagrams.

## Before you run

No hardware or outside network.

## Follow along

1. Click BIND TWO SOCKETS.
2. Press SEND TEXT and SEND BINARY.
3. Observe remote peer and echoed datagrams.
4. Press STOP UDP.

## Follow the JavaScript

`udpStart` → `startLocal()` → `UDP.bind()`; `on("message")` handles received datagrams.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Local UDP Datagrams | `udpStart` (button) | `clicked` | `startLocal()` |
| 01 Local UDP Datagrams | `udpText` (button) | `clicked` | `textTest()` |
| 01 Local UDP Datagrams | `udpBinary` (button) | `clicked` | `binaryTest()` |
| 01 Local UDP Datagrams | `udpStop` (button) | `clicked` | `stopLocal()` |

## Make one small change

Change the binary payload and verify that the returned byte array is identical.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Unlike TCP, UDP preserves datagram boundaries but provides no delivery guarantee.

## Safety and scope

No external network required; do not assume datagrams are reliably delivered on real networks.

## If something does not work

If nothing is received, confirm both sockets have been bound before sending.

## Related files

- **Editable project:** `examples/panels/UdpCommsLab.wfpanel`
- **Readable JavaScript:** `examples/source/UdpCommsLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
