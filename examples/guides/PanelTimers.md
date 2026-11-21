# Lab 04 — Panel JavaScript Timers — Interval & Timeout

**Level:** Beginner\
**Category:** Getting Started\
**Pages:** 01 Timer Dashboard → 02 API & Shared State

## What you will build or observe

Start, stop and cancel nonblocking timers without freezing the user interface.

## Before you run

Simulation-only; no hardware needed.

## Follow along

1. Press START 250ms and watch the count increase.
2. Press STOP 250ms and confirm the count stops.
3. Press SCHEDULE 2s and then CANCEL 2s; inspect the log.
4. Switch Pages and notice that timer state is shared.

## Follow the JavaScript

`timerStartFast` → `startFast()` → `setInterval()`; `timerStopFast` → `stopFast()` → `clearInterval()`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Timer Dashboard | `timerStartFast` (button) | `clicked` | `startFast()` |
| 01 Timer Dashboard | `timerStopFast` (button) | `clicked` | `stopFast()` |
| 01 Timer Dashboard | `timerStartOnce` (button) | `clicked` | `scheduleDemo()` |
| 01 Timer Dashboard | `timerCancelOnce` (button) | `clicked` | `cancelDemo()` |
| 01 Timer Dashboard | `timerResetCounts` (button) | `clicked` | `resetTimerCounters()` |
| 02 API & Shared State | `guideStart` (button) | `clicked` | `startFast()` |
| 02 API & Shared State | `guideStop` (button) | `clicked` | `stopFast()` |
| 02 API & Shared State | `guideOnce` (button) | `clicked` | `scheduleDemo()` |
| 02 API & Shared State | `guideCancel` (button) | `clicked` | `cancelDemo()` |
| 02 API & Shared State | `guideClearLog` (button) | `clicked` | `clearTimerLog()` |

## Make one small change

Change the 250 ms interval to 500 ms, then compare how quickly the counter updates.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Widget events start/stop the timer; the timer callback is a separate asynchronous operation.

## Safety and scope

No hardware; always clear intervals when they are no longer needed.

## If something does not work

Unexpected repeat callbacks usually mean another interval was started without clearing the previous one.

## Related files

- **Editable project:** `examples/panels/PanelTimers.wfpanel`
- **Readable JavaScript:** `examples/source/PanelTimers.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
