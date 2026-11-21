# Lab 34 — TCP Local Client/Server — Text & Binary

**Level:** Beginner–Intermediate\
**Category:** Communications\
**Pages:** 01 Local TCP Client & Server

## What you will build or observe

Use a TCP client/server on localhost and compare text lines with binary stream data.

## Before you run

No hardware or external network; localhost (`127.0.0.1`) is enough.

## Follow along

1. Click START SERVER.
2. Try SEND TEXT and SEND BINARY.
3. Watch the local server echo responses.
4. Press STOP TCP.

## Follow the JavaScript

`tcpStart` → `startLocal()` → TCP listener/client; socket `on("data")` callbacks receive bytes.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Local TCP Client & Server | `tcpStart` (button) | `clicked` | `startLocal()` |
| 01 Local TCP Client & Server | `tcpText` (button) | `clicked` | `textTest()` |
| 01 Local TCP Client & Server | `tcpBinary` (button) | `clicked` | `binaryTest()` |
| 01 Local TCP Client & Server | `tcpStop` (button) | `clicked` | `stopLocal()` |

## Make one small change

Change the example text and observe the echoed reply.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

TCP preserves byte order but not message boundaries. Delimiters/framing belong in the application.

## Safety and scope

The server binds to localhost only; do not blindly expose it to an untrusted network.

## If something does not work

Binary data may include zero and newline bytes: use raw data callbacks instead of assuming line parsing.

## Related files

- **Editable project:** `examples/panels/TcpCommsLab.wfpanel`
- **Readable JavaScript:** `examples/source/TcpCommsLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
