# Lab 10 — Digital Timing and Protocol Charts

**Level:** Beginner–Intermediate\
**Category:** Graphics & Charts\
**Pages:** Digital → Protocol

## What you will build or observe

Understand sampled digital signals and schematic protocol timing.

## Before you run

No hardware; sequences are synthetic.

## Follow along

1. Run the Digital Page and click Regenerate logic.
2. Switch to Protocol and click Regenerate protocols.
3. Compare transitions and labelled traces.

## Follow the JavaScript

`logicRefresh` / `protoRefresh` → named event handlers → plotting routines.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| Digital | `logicRefresh` (button) | `clicked` | `onLogicRefreshClicked()` |
| Protocol | `protoRefresh` (button) | `clicked` | `onProtoRefreshClicked()` |

## Make one small change

Alter one generated bit pattern and compare its plotted transitions.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Digital timing charts communicate discrete states over time; they do not transmit UART/SPI/I²C by themselves.

## Safety and scope

This is a visualization, not a protocol compliance test.

## If something does not work

A chart does not show the AD3 pins unless an actual acquisition API was used.

## Related files

- **Editable project:** `examples/panels/DigitalTimingCharts.wfpanel`
- **Readable JavaScript:** `examples/source/DigitalTimingCharts.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
