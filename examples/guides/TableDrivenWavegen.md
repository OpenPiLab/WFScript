# Lab 14 — Table-Driven Wavegen — Multi-Trace Sweep

**Level:** Advanced\
**Category:** Tables & Data\
**Pages:** 00 Edit Frequency Recipe → 01 Apply Wavegen Settings → 02 Results & JavaScript

## What you will build or observe

Use an editable frequency recipe to generate preview traces or deliberately drive W1 and capture CH1.

## Before you run

Begin in Simulation. For a physical experiment verify W1 → CH1+ and AD3 GND → CH1− with a safe circuit and appropriate range.

## Follow along

1. On Edit Frequency Recipe inspect the rows and click VALIDATE & READ TABLE.
2. Use PREVIEW ALL (NO I/O) before applying any real output.
3. Inspect Apply Wavegen Settings and Results & JavaScript.
4. Only with verified wiring, deliberately choose physical mode and initiate the capture action.

## Follow the JavaScript

`recipeTable` → validation; `runAll` → `onRunAllClicked()` → per-row W1 configuration, CH1 capture and trace retention.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 00 Edit Frequency Recipe | `recipeTable` (experiment_table) | `valueChanged` | `onRecipeTableValueChanged()` |
| 00 Edit Frequency Recipe | `validateRecipe` (button) | `clicked` | `onValidateRecipeClicked()` |
| 00 Edit Frequency Recipe | `addRecipeRow` (button) | `clicked` | `onAddRecipeRowClicked()` |
| 00 Edit Frequency Recipe | `resetRecipe` (button) | `clicked` | `onResetRecipeClicked()` |
| 00 Edit Frequency Recipe | `previewRecipe` (button) | `clicked` | `onPreviewRecipeClicked()` |
| 01 Apply Wavegen Settings | `resetCursor` (button) | `clicked` | `onResetCursorClicked()` |
| 01 Apply Wavegen Settings | `applyNext` (button) | `clicked` | `onApplyNextClicked()` |
| 01 Apply Wavegen Settings | `runAll` (button) | `clicked` | `onRunAllClicked()` |
| 01 Apply Wavegen Settings | `stopW1` (button) | `clicked` | `onStopW1Clicked()` |

## Make one small change

Change one recipe frequency within the configured bounds and observe its MODEL ONLY preview first.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Preview data and actual measured CH1 traces are intentionally labelled differently; failures must stop W1.

## Safety and scope

This example can energize an AD3 output. Preview first, check wiring, observe voltage limits and stop W1 when done.

## If something does not work

If a row is rejected, look at its seven required columns and Run? flag. Do not bypass validation.

## Related files

- **Editable project:** `examples/panels/TableDrivenWavegen.wfpanel`
- **Readable JavaScript:** `examples/source/TableDrivenWavegen.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
