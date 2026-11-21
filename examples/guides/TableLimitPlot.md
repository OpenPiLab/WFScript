# Lab 13 — Table-to-Plot — Limits and Observations

**Level:** Intermediate\
**Category:** Tables & Data\
**Pages:** 00 Edit Data & Limits → 01 XY Plot & Results

## What you will build or observe

Turn operator-edited data into observed, nominal and tolerance-limit plots.

## Before you run

No hardware. Values are entered in the table.

## Follow along

1. Edit an observation or limit in the profile table.
2. Press VALIDATE & PLOT TABLE.
3. On XY Plot & Results inspect which rows pass the limits.
4. Restore example data to compare the baseline.

## Follow the JavaScript

`limitInputTable` → `onLimitInputTableValueChanged(rows)`; `plotEnteredData` → `onPlotEnteredDataClicked()`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 00 Edit Data & Limits | `limitInputTable` (experiment_table) | `valueChanged` | `onLimitInputTableValueChanged()` |
| 00 Edit Data & Limits | `plotEnteredData` (button) | `clicked` | `onPlotEnteredDataClicked()` |
| 00 Edit Data & Limits | `restoreLimitData` (button) | `clicked` | `onRestoreLimitDataClicked()` |
| 01 XY Plot & Results | `clearLimitResults` (button) | `clicked` | `onClearLimitResultsClicked()` |

## Make one small change

Increase one observation beyond its upper bound and rerun validation to see a failure.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Keep raw operator data separate from derived PASS/FAIL calculations and charts.

## Safety and scope

Limits are demonstrations; real acceptance criteria require engineering justification.

## If something does not work

If nothing plots, validate the numeric columns and restore the sample dataset.

## Related files

- **Editable project:** `examples/panels/TableLimitPlot.wfpanel`
- **Readable JavaScript:** `examples/source/TableLimitPlot.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
