# Lab 37 — Panel Data: Live Workbench

**Level:** Beginner–Intermediate\
**Category:** Getting Started\
**Pages:** One — **Live Workbench**\
**Hardware:** None. All values are simulated.

## What you will learn

The controls and live readouts are side by side so you can see a data binding respond immediately. The seven project-owned shared variables are named `Motor.TargetRPM`, `Motor.ActualRPM`, `Motor.LoadPct`, `Motor.Ready`, `Motor.Mode`, `Motor.Status` and derived `Motor.TargetRPS`. These are **in-memory runtime values**, unrelated to the disk folder named `PanelData` used by FileIO/SQLite.

## Follow along

1. Open **Examples → Panel Data Lab**. In Design mode, open **Properties → Panel Data** and inspect the six entries. The column is labelled **Initial Value** because the Panel isn't running.
2. Look at `targetInput`, `loadSlider`, `readySwitch` and `modeSelector` on the **left**. Each has an optional **Properties → Value Binding**. The right-hand `targetReadout`, `loadMeter`, and `readyBeacon` use the same variables.
3. Select `speedDisplay`, `speedDial`, and `speedGauge` on the **right**. All three bind to `Motor.ActualRPM`. The `targetReadout` display instead binds to the **computed** `Motor.TargetRPS` variable. No separate JavaScript widget assignments are needed.
4. Before running, find `Data.computed('Motor.TargetRPS', ['Motor.TargetRPM'], ...)` near the beginning of the script. This formula divides Target RPM by 60, converts it to revolutions per second and updates the `targetReadout` display automatically. Click **Run Panel**. The **LIVE RESULTS** subtitle immediately displays the initial target from `Data.onChange(...,{immediate:true})`. Enable **READY**, adjust **Target RPM**, and press **START SIMULATION**. Watch the three speed displays change together **without switching Pages**. The slower first-order response gives you time to observe the effect.
5. While running, reopen the Properties pane (automatic Focus hides it) and select **Panel Data**. The third column reads **Live Value**; values follow changes, including variables you create yourself before running.
6. While the simulated motor runs, turn **READY OFF**. A `Data.onChange('Motor.Ready',...)` subscriber safely stops the *simulation* without requiring a separate Button event. Re-enable READY and START if desired. Click **STOP** to stop the model timer, then **RESET ALL** to restore the simulated values. Click **Stop / Design** on the toolbar to exit the Panel runtime. The Panel Data column changes back to **Initial Value**, because runtime values are not saved as defaults.

## Follow the JavaScript

The source is at `examples/source/PanelDataLab.js` and is embedded in the `.wfpanel` project. The model runs every 250 ms and publishes one measured speed value:

```javascript
const previous = Data.get('Motor.ActualRPM');
const target = Data.get('Motor.TargetRPM');
const load = Data.get('Motor.LoadPct');
const achievable = target * (1 - 0.0012 * load);
Data.set('Motor.ActualRPM', Number((previous + 0.12 * (achievable - previous)).toFixed(1)));
```

All three right-hand speed widgets follow that one `Data.set()` call. `Data.get()` reads a variable; `Data.names()` lists all definitions. `Data.isComputed(name)` reports whether a definition was registered as a derived read-only value in the current run. Input controls publish changes through their two-way bindings **before** the saved JavaScript event handler executes.

### Observe changes without manually wiring widget events

The example now registers two Data subscriptions in JavaScript:

```javascript
const targetWatcher = Data.onChange('Motor.TargetRPM', function(value, previous, name) {
    ui.set('hintControls', 'TARGET ' + value + ' RPM');
}, {immediate:true});

const readyWatcher = Data.onChange('Motor.Ready', function(ready) {
    if (!ready && modelTimer !== null) stopMotorSimulation();
});
```

`Data.onChange()` receives `(value, previousValue, name)` and returns a numeric subscription ID. The target subscriber executes when you change the **targetInput** control *or* when RESET runs `Data.set('Motor.TargetRPM',1200)`. The READY subscriber stops the simulation when READY is switched off. A write of the **same value** produces no event, and initial defaults do not notify unless `{immediate:true}` is requested (then `previousValue` is `undefined`). The example calls `Data.offChange(id)` in `onShutdown()`; subscriptions are also cleared when a Panel run ends.

These subscriptions **do not replace designer-managed control events**: for example, `onTargetChanged(value)` still demonstrates the Value Binding event handler. The Data subscription is for state changes regardless of their source. Programmatic `Data.set()` updates still never simulate an operator click or touch hardware.

**Avoid feedback loops:** A callback can write another variable, but do not make two subscriptions continually write new values back to one another. Events are queued within each synchronous dispatch cycle and capped at 256 to detect such loops; callback exceptions appear in the Panel diagnostics.

### Saved visual event handlers

| Control | Event | Handler |
|---|---|---|
| `startButton` | `clicked` | `startMotorSimulation()` |
| `stopButton` | `clicked` | `stopMotorSimulation()` |
| `resetButton` | `clicked` | `resetMotorSimulation()` |
| `targetInput` | `valueChanged` | `onTargetChanged(value)` |
| `modeSelector` | `selectionChanged` | `onModeChanged(text)` |

Use **Design → select widget → Properties → Events → Go to Handler** to inspect these functions. No manual `ui.on()` duplicate registrations are necessary.

## Create your own computed value

Declare `Experiment.Voltage` (Number, initial 3.3) and `Experiment.VoltageMilli` (Number, initial 3300) in **Properties → Panel Data**. Bind a read-only Numeric or Digital Display widget to `Experiment.VoltageMilli`. Register at script startup:

```javascript
Data.computed('Experiment.VoltageMilli', ['Experiment.Voltage'], function(volts) {
    return volts * 1000;
});
```

Then `Data.set('Experiment.Voltage', 2.5)` automatically makes the bound millivolt display read `2500`. Calling `Data.set('Experiment.VoltageMilli', 2500)` throws, because the target is computed and read-only. This is a **runtime declaration in the saved JavaScript**; both variable definitions must be added in Design mode for the editor to offer the bindings. See [docs/PANEL_DATA_COMPUTED_V02_DEV8_5.md](../../docs/reference/PANEL_DATA_COMPUTED_V02_DEV8_5.md).

## Create a variable and watch it change

1. In **Design**, open **Properties → Panel Data → + New** and create `Experiment.Voltage` as **Number** with initial value `0`. Alternatively, select a compatible widget and use **Value Binding → + Create New Variable...**.
2. In `startMotorSimulation()`, immediately before the `setInterval` call, add `Data.set('Experiment.Voltage', 3.3);`.
3. Run the Panel, reopen **Properties → Panel Data**, and click **START** after setting READY. The new row should now show a **Live Value** of `3.3`. The variable does not need to be bound to a visible widget to update in this list.
4. Stop / Design: the value returns to the saved **Initial Value** (`0`). For persistent readings, use explicit FileIO or SQLite rather than changing project defaults automatically.

## Make one small change

Change the `0.12` coefficient in `simulationTick()` to `0.06`. Watch the actual motor speed rise more slowly while the target remains fixed. Save the modified project with **Save / Export**. This simulation never commands the AD3 or physical motor hardware.

## Rules that prevent confusion

- Value Binding is optional. Unbound widgets still use `ui.get()` / `ui.set()`.
- Once a widget's Value is bound, `Data.set(name, value)` owns that value; direct `ui.set(widgetId, ...)` is rejected.
- A programmatic Data change refreshes widgets without simulating operator actions or hardware commands.
- Variable types must match widget types: Number, Boolean and Text. Read-only computed targets cannot be bound to operator-editable controls.
- A variable referenced by a widget cannot be deleted while that binding remains. JavaScript references cannot be automatically renamed.
- **Initial Value** is the design-time default. **Live Value** is the runtime state; it isn't silently persisted.

## Safety and scope

This example is simulation-only. The READY control changes a Boolean; only the explicit START button runs the synthetic model. There are no AD3 output operations, COM ports or network connections.

## If something does not work

Confirm the Panel is running, the correct widget Value Binding is selected, and the variable name exactly matches the `Data.set()` argument. Expand **Properties** when Run Focus hides the pane. A newly created variable must appear immediately in Design mode and reflect its `Data.set()` changes in the live column during Run.

## Related files

- `examples/panels/PanelDataLab.wfpanel` — one-Page workbench with all 19 widgets and seven typed variables.
- `examples/source/PanelDataLab.js` — source for the embedded script.
- `docs/PANEL_DATA_V02_DEV8.md` — Panel Data architecture, API and binding coverage.
