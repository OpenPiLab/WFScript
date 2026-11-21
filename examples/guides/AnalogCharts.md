# Lab 09 — Analog Charts and Frequency

**Level:** Beginner–Intermediate\
**Category:** Graphics & Charts\
**Pages:** Analog → XY → Frequency

## What you will build or observe

Compare time-series, XY and frequency-domain plots.

## Before you run

No device; waveforms and spectral content are generated mathematically.

## Follow along

1. Regenerate the time-series graph on Analog.
2. On XY, press Regenerate XY.
3. On Frequency, press Recalculate FFT and inspect the changed plot.

## Follow the JavaScript

Three Page buttons → separate `on…Clicked()` handlers → plotting helpers (`plots` API).

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Analog | `analogRefresh` (button) | `clicked` | `onAnalogRefreshClicked()` |
| XY | `xyRefresh` (button) | `clicked` | `onXyRefreshClicked()` |
| Frequency | `freqRefresh` (button) | `clicked` | `onFreqRefreshClicked()` |

## Make one small change

Adjust the synthetic frequency in the relevant graph generator and regenerate the curve.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

A Page organizes the display, but all chart logic lives in one JavaScript project.

## Safety and scope

Plots are synthetic demonstrations, not calibrated AD3 acquisitions.

## If something does not work

If a chart is empty, use its regeneration button before concluding that the drawing API failed.

## Related files

- **Editable project:** `examples/panels/AnalogCharts.wfpanel`
- **Readable JavaScript:** `examples/source/AnalogCharts.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
