# WFScript JavaScript Reference (practical guide)

This is a **practical reference** for the JavaScript environment exposed by WFScript. It is intentionally concise and focused on the parts that users will reach for first. Use WFScript's **autocomplete**, **F1 help**, and the built-in examples for deeper discovery.

## Runtime style

WFScript uses a long-lived JavaScript runtime built on Qt `QJSEngine`.

Typical scripts:

- respond to widget events,
- read and write widget state through `ui`,
- control instruments such as Wavegen and Scope,
- update plots and tables,
- run test sequences through the native Test Executive.

## Common globals

### `print(...)`
Writes text to the script/panel console.

```javascript
print("Hello from WFScript");
print("Frequency:", 1000);
```

### `ui.get(id)`
Reads a widget value by widget ID.

```javascript
const f = ui.get('freqHz');
```

For Experiment Tables, `ui.get()` returns **native JavaScript arrays**:

```javascript
const rows = ui.get('recipeTable');
if (Array.isArray(rows)) {
  for (const row of rows) {
    print(row);
  }
}
```

### `ui.set(id, value)`
Writes a widget value by widget ID.

```javascript
ui.set('statusText', 'Running');
ui.set('freqHz', 2500);
```

### `modules.load(path)`
Loads a JavaScript module from a project or example library path.

```javascript
const math = modules.load('MathTools.mjs');
```

## Instrument objects

The exact available members are best explored through autocomplete/F1 in the editor.
Common top-level instrument objects include:

- `Device`
- `Wavegen1`
- `Scope1`
- `Supplies`
- `StaticIO`
- `Patterns`

### Example: simple Wavegen + Scope capture

```javascript
Wavegen1.Channel1.Simple.Type.text = 'Sine';
Wavegen1.Channel1.Simple.Frequency.value = 1000;
Wavegen1.Channel1.Simple.Amplitude.value = 1.0;
if (!Wavegen1.run()) throw new Error(Wavegen1.lastError);

Scope1.Time.Samples.value = 4096;
Scope1.Time.Rate.value = 1000000;
Scope1.Channel1.Range.value = 5;
if (!Scope1.single()) throw new Error(Scope1.lastError);
if (!Scope1.wait(5)) throw new Error(Scope1.lastError);

const stats = Scope1.Channel1.statistics();
print('RMS:', stats.rms);
Wavegen1.stop();
```

## Widget events

Widgets can call named JavaScript handlers when the panel is running.

Example:

```javascript
function onStartClicked() {
  ui.set('statusText', 'Start clicked');
}
```

Common event patterns include:

- button clicked,
- selector changed,
- input value changed,
- table value changed.

See the examples for concrete event wiring.

## Tables and data processing

Experiment Tables are especially useful for:

- recipe rows,
- calibration data,
- frequency sweeps,
- user-entered measurements,
- test parameter lists.

Example:

```javascript
function readEnabledRows() {
  const rows = ui.get('recipeTable');
  const enabled = [];
  for (const row of rows) {
    if (!row || row.length < 2) continue;
    const useRow = Boolean(row[0]);
    const freqHz = Number(row[1]);
    if (useRow && Number.isFinite(freqHz)) {
      enabled.push(freqHz);
    }
  }
  return enabled;
}
```

## Plotting

WFScript includes plot-oriented examples for:

- analog waveform traces,
- digital timing diagrams,
- nominal/observed/limit curves,
- multi-trace retained captures.

The built-in examples are the best source of real plotting patterns:

- `AnalogCharts`
- `ChartTechniques`
- `TableLimitPlot`
- `TableDrivenWavegen`
- `DigitalTimingCharts`

## Test Executive

WFScript includes a native Test Executive with:

- Setup / Main / Cleanup step groups,
- numeric pass/fail limits,
- run information,
- result archiving,
- results browsing.

The best starting examples are:

- `AutomatedTestBench`
- `AutomatedTestStation`
- `AD3HardwareTestStation`

## Learn by example

If you are unsure how something should be done, the best path is:

1. Open a built-in example.
2. Click **Edit JavaScript**.
3. Use **autocomplete** and **F1 help** inside the editor.
4. Adapt that script into your own panel.

That workflow is the intended way to discover the API in WFScript.
