# Lab 36 — Communications Terminal — Serial / TCP / UDP

**Level:** Beginner–Intermediate\
**Category:** Communications\
**Pages:** 01 TCP — Local Echo → 02 UDP — Local Echo → 03 Serial — COM Port

## What you will build or observe

Reuse one native Terminal widget with TCP, UDP and optional PC Serial connections.

## Before you run

TCP/UDP use localhost; the Serial Page needs a COM device or virtual port pair.

## Follow along

1. On TCP, click START LOCAL TCP; send text and then a Hex bytes probe.
2. On UDP, click START LOCAL UDP and repeat; compare datagrams with TCP.
3. Use Display mode Text/Hex/Both and Clear.
4. On Serial, discover/select/open a COM port only if one is available.

## Follow the JavaScript

`termTcp` sendRequested → `sendTcp(payload)` → `TCP.write()`; native socket callbacks call `traffic()`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 TCP — Local Echo | `tcpStartTerm` (button) | `clicked` | `startTcp()` |
| 01 TCP — Local Echo | `tcpBinaryTerm` (button) | `clicked` | `sendTcpProbe()` |
| 01 TCP — Local Echo | `tcpStopTerm` (button) | `clicked` | `stopTcp()` |
| 01 TCP — Local Echo | `termTcp` (communications_terminal) | `sendRequested` | `sendTcp()` |
| 02 UDP — Local Echo | `udpStartTerm` (button) | `clicked` | `startUdp()` |
| 02 UDP — Local Echo | `udpBinaryTerm` (button) | `clicked` | `sendUdpProbe()` |
| 02 UDP — Local Echo | `udpStopTerm` (button) | `clicked` | `stopUdp()` |
| 02 UDP — Local Echo | `termUdp` (communications_terminal) | `sendRequested` | `sendUdp()` |
| 03 Serial — COM Port | `serialDiscoverTerm` (button) | `clicked` | `discoverSerial()` |
| 03 Serial — COM Port | `serialSelectTerm` (button) | `clicked` | `selectSerial()` |
| 03 Serial — COM Port | `serialOpenTerm` (button) | `clicked` | `openSerial()` |
| 03 Serial — COM Port | `serialBinaryTerm` (button) | `clicked` | `sendSerialProbe()` |
| 03 Serial — COM Port | `serialCloseTerm` (button) | `clicked` | `closeSerial()` |
| 03 Serial — COM Port | `termSerial` (communications_terminal) | `sendRequested` | `sendSerial()` |

## Make one small change

Add a new INFO message or change the binary `probe` bytes; rerun localhost echo.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Terminal widgets display and request sends; JavaScript chooses the transport. TX acceptance is not confirmed remote delivery.

## Safety and scope

No network traffic or COM connection starts until an operator explicitly requests it.

## If something does not work

If the terminal reports not connected, start its transport first; Send by itself cannot open hardware.

## Related files

- **Editable project:** `examples/panels/CommunicationsTerminalLab.wfpanel`
- **Readable JavaScript:** `examples/source/CommunicationsTerminalLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
