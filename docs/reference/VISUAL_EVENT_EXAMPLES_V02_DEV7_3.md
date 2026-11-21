# WFScript v0.2-dev.7.3 — Learning widget events in the designer

## Purpose

WFScript already supported visual widget-event management: in **Design mode**, select a widget, open **Widget Properties → Events**, choose a supported event, then **Go to Handler** or **Create Handler**. Previously the bundled examples mostly used literal `ui.on(...)` registrations, hiding this feature from learners.

This phase teaches **designer-managed UI events by default** while preserving script callbacks for timers, database operations, and native communications.

## Walkthrough: Getting Started

1. Open **Examples → Getting Started** in the Panel Designer.
2. Switch to the new **Events Tutorial** Page.
3. In **Design mode**, select **CLICK ME — DESIGNER EVENT**.
4. Choose **Widget Properties → Events → clicked**.
5. Verify that the event is enabled and assigned to `onTourButtonClicked`.
6. Use **Go to Handler** to navigate to its named global function in `GettingStarted.js`.
7. Run the Panel in Simulation, click twice, and verify that `tourResult` shows 1 time and 2 times. Stop the Panel to edit again.

No `ui.on()` registration is necessary: the `.wfpanel` stores `control.events.clicked.handler` and `enabled`, and the existing PanelRuntime calls that named function directly.

## Converted examples

| Example | UI event bindings | Notes |
|---|---:|---|
| Getting Started | 1 | New Page with an interactive visual-binding tutorial |
| Widget Playground | 8 | Shared refresh, press/release, and Reset |
| Panel Timers | 10 | Two Pages share named actions, timer callbacks unchanged |
| SQLite Data Lab | 8 | Database callbacks and persistence unchanged |
| Serial Communications | 6 | PC serial events and COM requirements unchanged |
| TCP Client/Server | 4 | Local text and binary connections unchanged |
| UDP Datagram Echo | 4 | Local datagrams unchanged |
| Communications Terminal | 14 | `sendRequested(payload)` and operator actions across three Pages |
| **Total** | **55** | 8 of 36 built-in examples; remaining 28 unchanged |

## Create Handler improvement

Previously the code generator handled only generic `valueChanged(value)` and `toggled(checked)` arguments. It now uses the `PanelEventCatalog` definitions registered by each widget factory. Examples:

- Option Selector `selectionChanged(text: string)` generates `function handler(text)` with JSDoc `@param {string} text`.
- Communications Terminal `sendRequested(payload: object)` generates `function handler(payload)` with JSDoc `@param {object} payload`.
- Experiment Data Table `valueChanged(rows: array)` generates `function handler(rows)` with JSDoc `@param {array} rows`.
- Button `clicked` produces a parameterless handler.

The handler generator still navigates to the function in the main script, preserves filename/line error reporting, and does not synthesize hidden code. The source-aware error and native execution paths are unchanged.

## What not to convert

* `Serial`, `TCP`, and `UDP` connection `.on(...)` events aren't UI widget events; they stay in JavaScript.
* Async `Database` operations and `Tool` dialogs still use callbacks.
* `setInterval` and `setTimeout` callbacks remain script functions.
* `StaticIO.onChange` refers to physical digital inputs, not widget events.
* Programmatically-generated `ui.on()` bindings may remain useful, especially repeated controls such as Static Digital I/O's sixteen selectors.

**Do not register the same event both visually and with `ui.on()`.** WFScript intentionally supports both, so the same event can otherwise invoke two separate callbacks. The offline and native smoke regressions explicitly reject duplicates in the examples converted here.

## Verification

Run from the project root:

```text
python tools/build_examples.py --check
python tests/visual_bindings_phase1_check.py
node tests/visual_bindings_phase1_mock.js
node tests/panel_timers_example_test.js
node tests/communications_examples_mock.js
node tests/communications_terminal_example_mock.js
```

For native Qt tests, build with `WFSCRIPT_BUILD_TESTS=ON` and run `ctest --output-on-failure`. The existing `WFScriptPanelEventInspectorSmoke` now loads all eight real embedded `.wfpanel` projects with the production parser, verifies every binding points to a declared function, and checks that the original `ui.on()` registration is absent. The runtime smoke also checks that an object payload reaches the named Communications Terminal handler.

Native Windows Qt compilation and GUI behavior must still be verified on the user's Qt 6.9.2 MinGW environment.
