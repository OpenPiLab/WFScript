# Lab 08 — Chart Techniques and Analytics

**Level:** Intermediate\
**Category:** Graphics & Charts\
**Pages:** Analytics → Axes

## What you will build or observe

Create and style plotted data, visible/hidden series and axis configurations.

## Before you run

No device. All plot points are synthetic.

## Follow along

1. Run and press Append trend point.
2. Press Reset trends; observe that the plot restarts.
3. On Axes, try Toggle hidden series and Cycle line widths.

## Follow the JavaScript

`trendAppend` → `onTrendAppendClicked()` → chart helper; style buttons call `plots` operations.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Analytics | `trendAppend` (button) | `clicked` | `onTrendAppendClicked()` |
| Analytics | `trendReset` (button) | `clicked` | `onTrendResetClicked()` |
| Axes | `axisToggle` (button) | `clicked` | `onAxisToggleClicked()` |
| Axes | `axisStyle` (button) | `clicked` | `onAxisStyleClicked()` |

## Make one small change

Change one series title or width in the plotting configuration, then regenerate.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Charts own their rendering; JavaScript provides numeric series, labels and styling.

## Safety and scope

Displayed data is not a captured instrument waveform.

## If something does not work

If a trace disappears, check whether the example intentionally hid that series.

## Related files

- **Editable project:** `examples/panels/ChartTechniques.wfpanel`
- **Readable JavaScript:** `examples/source/ChartTechniques.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
