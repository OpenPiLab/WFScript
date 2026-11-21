# WFScript v0.2-dev.8 — Panel Data and Visual Value Bindings

## Purpose

Optional **project-owned variables**, defined once in the Property Editor's third tab (**Panel Data**), let compatible widgets share typed runtime values across multiple Pages. Existing `ui.set()` scripts and unbound widgets continue working. **This is a new in-memory runtime mechanism, not the similarly named shared disk directory `PanelData` for FileIO/SQLite.**

## Step-by-step workflow

1. In **Design**, open **Properties → Panel Data**, click **+ New**, enter a unique name, type, and initial value.
2. Alternatively, select a compatible widget, choose **Properties → Value Binding → + Create New Variable...**, and define the variable directly; the new variable is automatically assigned to that widget.
3. Bind another compatible widget through its Value Binding selector. **Only type-compatible values are listed.**
4. Use `Data.set('Motor.Speed', 1250)` in JavaScript. All widgets bound to `Motor.Speed` update automatically; there is no need for individual `ui.set()` calls.
5. During **Run**, Panel Data shows live values (read-only). Switching Pages never resets data; saving the Panel doesn't snapshot its current runtime values.

## JavaScript API

```javascript
Data.get('Motor.Speed');        // returns the current typed value
Data.set('Motor.Speed', 1250);  // set it (type checked, finite numeric values only)
Data.names();                  // list all defined names
```

Names may contain letters, digits, underscores and dot-separated namespaces, e.g. `Motor.Speed`; the first character of every segment must be a letter or underscore. Max name length: 100 characters; max 128 definitions per project. Types: `number`, `boolean`, `text` (maximum 4096 characters). Initial values are mandatory and type-checked.

Unknown names or wrong types cause clear `Data.get`/`Data.set` errors. Redundant updates of identical values do not re-emit all widget refreshes. Variable values live on the Panel's JavaScript worker thread; only safe Qt `QVariant` values cross to the GUI. No `QJSValue` crosses threads.

## Value-binding widget coverage (v0.2-dev.8)

| Variable type | Widgets | Direction |
|---|---|---|
| Number | Numeric Display (`numeric`), Professional Digital Display (`digital_display`), Gauge, Precision Dial, Segmented Bar, Multi-State Status Banner | Data → display |
| Number | Slider, Precision Rotary Input, Precision Numeric Stepper | Data ↔ operator input |
| Boolean | Status Beacon | Data → display |
| Boolean | Basic Toggle, Latching Switch | Data ↔ operator input |
| Text | Label | Data → display |
| Text | Option Selector | Data ↔ operator input |

Chart/plot series, experimental tables, Terminal buffers, images, equations and Test Executive widgets are not bound as scalar Values. Those require structured data or explicit commands; continuing to use `plots.*`, `ui.set`, or the relevant widget/event APIs avoids misleading half-working bindings. New widget types should add binding capability only when their native `applyRuntimeValue()` and event semantics are correct.

## One source of truth

For a bound widget Value, update through `Data.set()` rather than `ui.set()` (which rejects the conflicting update). Unbound controls continue supporting direct `ui.set()` and `ui.get()` as before. The input-to-data update happens before the widget's existing designer-managed handler, so callbacks can read the new value. Programmatic updates use native widget methods that suppress user-generated events, avoiding feedback loops and unintended hardware operations. **No data binding directly starts/stops hardware.** Commands remain explicit event-handler actions.

## Project and lifecycle rules

Definitions are saved in the project as top-level `panelData` JSON; individual controls store `bindings.value`. Existing projects without these fields remain valid and don't need migration. Every run begins with the configured initial values. Widgets across all Pages remain in one shared long-lived Panel runtime. Missing binding names, unsupported binding properties and incompatible types fail project validation rather than failing silently. The UI locks editing of definitions while running and refuses deletion when a binding remains. As a conservative first release, variable names and types cannot be changed in place; create a new variable and update scripts/bindings intentionally.

The Property Editor's Panel Data tab is project-scoped, while the Value Binding selector is control-scoped. A single `.wfpanel` still contains JavaScript source, Pages, widgets, visual events, definitions and bindings; no separate tag database is required.

## Example and tests

Open **Examples → Panel Data Lab**. One Live Workbench Page shows controls beside three synchronized actual-speed indicators, a typed status label, a read-only Boolean indicator, two-way numeric/Boolean/Text controls and a simulated 250 ms update timer. No AD3 hardware required.

The native `WFScriptPanelDataSmoke` test covers project serialization, invalid definitions, cross-Page bindings, type checking, native bridge one-to-many refresh, two-way input updates, and direct `ui.set` rejection. Native Qt build and 75 CTests remain unverified in the assistant environment; run tests using Windows Qt 6.9.2.

## Planned follow-ups, not part of this revision

Historical timeseries and chart buffers, persisted tag history, dynamic read/write driver mappings, tag change subscriptions, expression bindings and variable renaming/refactoring. These are deliberately excluded until ordinary scalar bindings are validated in the user's Windows Qt environment.

## v0.2-dev.8.1 — Live inspector and single-Page workbench correction

After a valid **+ New** or **Create New Variable...** edit, the project and the Property Editor's runtime-value cache are synchronized immediately. Subsequent worker `Data.set()` updates therefore appear under **Live Value** even for newly created, unbound variables. Updates modify the affected row rather than clearing the whole list on every sample, preserving selection and scroll position. The third column shows **Initial Value** in Design mode.

The Panel Data Lab now has one **Live Workbench** Page, with controls on the left and synchronized speed displays on the right. The demonstration coefficient is `0.12`, making the first-order response visible while editing setpoints. All original 19 widgets, six variables, 11 bindings and five saved visual events are preserved.
