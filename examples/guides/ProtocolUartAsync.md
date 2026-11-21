# Lab 32 — UART Asynchronous Lab — Native Terminal, Callbacks and Test

**Level:** Advanced\
**Category:** Digital Instruments\
**Pages:** 01 Native Terminal → 02 JavaScript Callback → 03 Automated Test

## What you will build or observe

Compare native UART Terminal display, JavaScript receive callbacks and automated echo testing.

## Before you run

No DIO is driven on startup. Physical echo test requires DIO0→1 kΩ→DIO1; simulation must not PASS the hardware test.

## Follow along

1. On Native Terminal configure auto reception, then send and read text.
2. On JavaScript Callback register and inspect incoming data.
3. On Automated Test inspect the project-owned sequence before running a physical loopback test.

## Follow the JavaScript

`uartView` sendRequested → `onUartViewSendRequested(text)`; `Protocol1.UART.onData()` remains a device callback.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Native Terminal | `asyncConfigure` (button) | `clicked` | `onAsyncConfigureClicked()` |
| 01 Native Terminal | `manualConfigure` (button) | `clicked` | `onManualConfigureClicked()` |
| 01 Native Terminal | `asyncSend` (button) | `clicked` | `onAsyncSendClicked()` |
| 01 Native Terminal | `asyncRead` (button) | `clicked` | `onAsyncReadClicked()` |
| 01 Native Terminal | `asyncStats` (button) | `clicked` | `onAsyncStatsClicked()` |
| 01 Native Terminal | `asyncStopTimer` (button) | `clicked` | `onAsyncStopTimerClicked()` |
| 01 Native Terminal | `asyncClear` (button) | `clicked` | `onAsyncClearClicked()` |
| 01 Native Terminal | `asyncRelease` (button) | `clicked` | `onAsyncReleaseClicked()` |
| 01 Native Terminal | `uartView` (uart_terminal) | `sendRequested` | `onUartViewSendRequested()` |
| 02 JavaScript Callback | `callbackStart` (button) | `clicked` | `onCallbackStartClicked()` |
| 02 JavaScript Callback | `callbackSend` (button) | `clicked` | `onCallbackSendClicked()` |
| 02 JavaScript Callback | `callbackDrain` (button) | `clicked` | `onCallbackDrainClicked()` |
| 02 JavaScript Callback | `callbackDisable` (button) | `clicked` | `onCallbackDisableClicked()` |
| 02 JavaScript Callback | `callbackStats` (button) | `clicked` | `onCallbackStatsClicked()` |
| 02 JavaScript Callback | `callbackRelease` (button) | `clicked` | `onCallbackReleaseClicked()` |
| 03 Automated Test | `testRun` (button) | `clicked` | `onTestRunClicked()` |
| 03 Automated Test | `testStop` (button) | `clicked` | `onTestStopClicked()` |
| 03 Automated Test | `testReset` (button) | `clicked` | `onTestResetClicked()` |
| 03 Automated Test | `testReport` (button) | `clicked` | `onTestReportClicked()` |

## Make one small change

Change the displayed receive message without changing the native buffer management.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

The widget can display native receive data without manually polling from a JavaScript timer; TestRunner owns its scheduler.

## Safety and scope

Hardware loopback needs resistance and safe logic voltage; test cleanup must release DIO.

## If something does not work

If the automated test fails in Simulation, that is intentional: simulated echo does not establish physical correctness.

## Related files

- **Editable project:** `examples/panels/ProtocolUartAsync.wfpanel`
- **Readable JavaScript:** `examples/source/ProtocolUartAsync.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
