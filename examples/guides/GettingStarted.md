# Lab 01 — Getting Started

**Level:** Beginner\
**Category:** Getting Started\
**Pages:** Guide → Events Tutorial

## What you will build or observe

Understand a Panel project, Design vs Run, and saved visual event bindings.

## Before you run

No equipment is required; stay in Simulation.

## Follow along

1. Read the Guide Page, then switch to Events Tutorial.
2. In Design mode select the CLICK ME button; open Properties → Events → clicked → Go to Handler.
3. Click Run Panel, click the tutorial button twice, and watch the count change.

## Follow the JavaScript

`tourButton` → `clicked` → `onTourButtonClicked()` → `ui.set("tourResult", …)`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Events Tutorial | `tourButton` (button) | `clicked` | `onTourButtonClicked()` |

## Make one small change

Change the message in `onTourButtonClicked()` to use your own words; rerun and confirm the counter still works.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

The Panel stores the event-to-function link; the main script provides the named function. All Pages share the same script.

## Safety and scope

No device outputs or wiring.

## If something does not work

If a click does nothing, check that the control has an enabled `clicked` event and that the function name matches exactly.

## Related files

- **Editable project:** `examples/panels/GettingStarted.wfpanel`
- **Readable JavaScript:** `examples/source/GettingStarted.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
