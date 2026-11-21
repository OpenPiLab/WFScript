# Lab 07 — Embedded Images and Instruction Cards

**Level:** Beginner\
**Category:** Graphics & Charts\
**Pages:** 00 Circuit Guide → 01 Your Lab Notes

## What you will build or observe

Put an image, equation and instructional cards into a self-contained Panel.

## Before you run

No device or JavaScript interaction is required.

## Follow along

1. Stop the Panel to enter Design mode.
2. Select the schematic image and inspect its Properties.
3. Select an Instruction Card and change its caption.
4. Save As a new `.wfpanel` and reopen it.

## Follow the JavaScript

No widget callbacks; the entire instructional graphic is stored within the `.wfpanel` itself.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). This static instructional Panel intentionally has no event handlers.

There are no event bindings to create or navigate. Design changes are saved as widget properties.

## Make one small change

Replace one instructional caption with the procedure for your own simple circuit.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Not every useful Panel requires JavaScript; static educational Pages are valid projects.

## Safety and scope

The drawing is instructional; do not treat it as a wiring authorization.

## If something does not work

If an image is not visible after reopening, confirm that it was embedded in the project rather than linked externally.

## Related files

- **Editable project:** `examples/panels/EducationalVisuals.wfpanel`
- **Readable JavaScript:** `examples/source/EducationalVisuals.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
