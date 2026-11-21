# Lab 11 — Mathematical Equations and Experiment Tables

**Level:** Beginner–Intermediate\
**Category:** Tables & Data\
**Pages:** 00 Equations & Theory → 01 Experiment Tables

## What you will build or observe

Combine a theory Page, a frequency slider and editable experiment tables.

## Before you run

No hardware; equations and predicted measurements are simulated.

## Follow along

1. Examine the Equations & Theory Page.
2. On Experiment Tables move the Frequency slider and choose POPULATE PREDICTIONS.
3. Edit a table cell and inspect the corresponding valueChanged handler.

## Follow the JavaScript

`frequencyInput` → `populateLab()` → table updates; `studentTable` → `onStudentTableValueChanged(rows)`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| 01 Experiment Tables | `frequencyInput` (slider) | `valueChanged` | `populateLab()` |
| 01 Experiment Tables | `studentTable` (experiment_table) | `valueChanged` | `onStudentTableValueChanged()` |
| 01 Experiment Tables | `fillTable` (button) | `clicked` | `populateLab()` |
| 01 Experiment Tables | `clearTable` (button) | `clicked` | `onClearTableClicked()` |

## Make one small change

Edit the mathematical input value or a predicted-data cell and watch the table update.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

A table can accept operator-entered values and a separate results table can show calculated values.

## Safety and scope

Simulated values are not measurements of a physical circuit.

## If something does not work

If table edits appear to reset, distinguish the student-entered table from the generated prediction table.

## Related files

- **Editable project:** `examples/panels/EquationExperimentLab.wfpanel`
- **Readable JavaScript:** `examples/source/EquationExperimentLab.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
