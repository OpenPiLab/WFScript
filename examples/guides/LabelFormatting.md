# Lab 03 — Multiline and Rich Text Labels

**Level:** Beginner\
**Category:** Graphics & Charts\
**Pages:** Text Labels

## What you will build or observe

Use plain multiline text and formatted HTML in Label widgets.

## Before you run

No hardware or files required.

## Follow along

1. Run the Panel and click UPDATE RICH TEXT LABEL.
2. Inspect the formatted output.
3. In Design mode inspect the label’s text-format properties and the button’s clicked handler.

## Follow the JavaScript

`demoUpdate` → `onDemoUpdateClicked()` → `ui.set()` updates the rendered label.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Text Labels | `demoUpdate` (button) | `clicked` | `onDemoUpdateClicked()` |

## Make one small change

Change an HTML `<b>` fragment or a line break in the script and observe the difference.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Labels can be static instructional material or dynamic status displays; HTML needs escaping for untrusted data.

## Safety and scope

No hardware; escape user-provided text before inserting it into rich HTML.

## If something does not work

If HTML shows literally, check whether the selected Label is configured for rich text.

## Related files

- **Editable project:** `examples/panels/LabelFormatting.wfpanel`
- **Readable JavaScript:** `examples/source/LabelFormatting.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
