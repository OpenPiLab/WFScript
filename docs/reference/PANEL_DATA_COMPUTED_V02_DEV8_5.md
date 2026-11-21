# WFScript v0.2-dev.8.5 — Computed Panel Data

## Purpose

`Data.computed()` declares a **derived, read-only value** in the same typed Panel Data system used by existing Value Bindings and `Data.onChange` subscriptions. This is a deliberately small, deterministic subset of Vue's computed-value concept, not an automatic Vue-style dependency tracker.

## First example

1. In **Design → Properties → Panel Data**, declare two Number variables: `Motor.TargetRPM` with initial `1200` and `Motor.TargetRPS` with initial `20`.
2. Bind an editable Numeric Stepper to `Motor.TargetRPM`, and a read-only Digital Display to `Motor.TargetRPS` using their **Value Binding** properties.
3. At Panel script startup (before timers or other actions), register the formula:

```javascript
Data.computed('Motor.TargetRPS', ['Motor.TargetRPM'], function(rpm) {
    return Number((rpm / 60).toFixed(2));
});
```

4. Run in **Simulation** and change Target RPM. The native binding refreshes the display and the Property Editor's live-value column without an explicit widget-update call.

**Important:** The target must be a named variable already saved in the `.wfpanel`. This allows the designer to validate its type and bind widgets *before* running the script. A computed definition is registered at runtime; it is not serialized separately in the Panel Data JSON. Its formula is ordinary saved JavaScript. Targets become read-only once registered in each run and revert to design-time initial values after Stop.

## API

```javascript
Data.computed(targetName, dependencies, formula); // returns targetName
Data.isComputed(name);                            // boolean
Data.get(targetName);                             // current computed value
Data.onChange(targetName, (value, previous, name) => { /* ... */ });
```

- The `dependencies` array has **1–16 distinct, existing Panel Data names**, in the same positional order supplied to the formula. It is **not automatic dependency inference**: include every variable that influences the calculation. Keep the formula pure; **do not call `Data.set()` or `Data.computed()` from inside it**.
- At most **64** computed variables can be registered during a Panel run. The target's saved Number, Boolean or Text type is enforced, including finite numbers and text length limits.
- Registration evaluates once with current inputs and immediately updates the derived target if its current value differs. An invalid initial result, duplicate registration, unknown variable, self-reference, indirect cycle, or binding the computed target to an operator-editable control is rejected without locking the target.
- `Data.set()` on a registered computed variable throws; the native typed bridge also rejects direct write requests and bound operator inputs. Use **read-only display widgets** for computed targets.
- Source changes via `Data.set()` or a bound operator input recompute **affected formulas once each in dependency order**, including chains and diamonds. Unchanged intermediate values don't re-trigger dependent calculations. The affected values are validated before committing, and listeners receive source and derived notifications. All changed derived values are committed **before source `Data.onChange` callbacks execute**, so reading a derived value in a source listener sees the updated state.
- If a calculation throws or returns an invalid type, the error is sent to Panel diagnostics, previous derived values are preserved, and the source-change notification still runs. The formula retries on a subsequent relevant input change; there is **no automatic rollback of the original writable source**.
- Pure computed propagation is **synchronous and unbatched**. `Data.onChange()` retains the 256-event feedback guard for *imperative* subscriptions that keep writing to each other. Cycle rejection prevents cycles between computed formulas, not arbitrary side effects in user-defined `Data.onChange()` callbacks.
- Formulas and computed-read-only flags are discarded when the Panel stops. They don't modify `.wfpanel` definitions or any physical outputs.

## Multiple dependencies and chains

First declare the three Number targets in Panel Data, then:

```javascript
Data.computed('Motor.TargetRPS', ['Motor.TargetRPM'], function(rpm) {
    return rpm / 60;
});
Data.computed('Motor.AllowedRPM', ['Motor.TargetRPM', 'Motor.LoadPct'], function(target, load) {
    return target * (1 - 0.0012 * load);
});
Data.computed('Motor.AllowedRPS', ['Motor.AllowedRPM'], function(rpm) {
    return rpm / 60;
});
```

The dependency graph enforces source-to-target order for downstream changes. The example formula is educational only; it is not an approved motor safety limit.

## Tests

Offline:

```powershell
node tests/panel_data_computed_mock.mjs
node tests/panel_data_events_mock.mjs
node tests/panel_data_example_mock.mjs
python tests/panel_data_computed_source_check.py
```

Qt native on Windows (after configure and Build All with `WFSCRIPT_BUILD_TESTS=ON`):

```powershell
ctest --test-dir <build-directory> -R "WFScriptPanelData(Computed|Events|Smoke)" --output-on-failure
```

`WFScriptPanelDataComputedSmoke` executes the **exact production JavaScript bootstrap** against a real `PanelUiBridge`, with a bound native input and two read-only output displays. The full native suite has **78 registered tests**; it has **not** been executed in the offline authoring environment because Qt6 development libraries aren't installed there.

## Unchanged implementation

The Panel Designer still persists typed variable definitions in `.wfpanel`; PanelRuntime and `PanelUiBridge` remain on the worker QJSEngine thread. The GUI uses QVariant signals only. Scope/Wavegen/AD3 SDK, TestRunner, communications, SQLite and FileIO are unchanged. No schema migration or hardware command is required.
