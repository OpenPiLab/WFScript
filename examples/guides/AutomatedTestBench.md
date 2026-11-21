# Lab 24 — Automated Test Bench — Measurement Results

**Level:** Beginner–Intermediate\
**Category:** Test Sequences & Executive\
**Pages:** 01 Test Summary → 02 Results Table

## What you will build or observe

Display measurement PASS/FAIL verdicts and maintain a compact table of results.

## Before you run

Simulation-only; no native instrument operations.

## Follow along

1. Click RUN DEMO TEST.
2. Inspect the Measurement Result widgets and table.
3. Click INJECT FAILURE to compare a failing limit result.
4. Use RESET RESULTS to start again.

## Follow the JavaScript

`runDemo` → `onRunDemoClicked()` → simulated measurements → result/table widgets.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Test Summary | `runDemo` (button) | `clicked` | `onRunDemoClicked()` |
| 01 Test Summary | `injectFailure` (button) | `clicked` | `onInjectFailureClicked()` |
| 01 Test Summary | `resetDemo` (button) | `clicked` | `onResetDemoClicked()` |

## Make one small change

Change one tolerance limit in the demonstration and rerun to see whether the result still passes.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

A measurement value and its acceptance verdict should be distinguishable and traceable.

## Safety and scope

This Panel demonstrates widgets; it is not the production native Test Executive.

## If something does not work

If all results pass after INJECT FAILURE, confirm the failure flag was activated before running the demo.

## Related files

- **Editable project:** `examples/panels/AutomatedTestBench.wfpanel`
- **Readable JavaScript:** `examples/source/AutomatedTestBench.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
