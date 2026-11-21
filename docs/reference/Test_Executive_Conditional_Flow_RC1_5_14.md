# WFScript RC1.5.14 — Conditional Flow

## What changed

IF and ELSE are now ordinary nested rows in the hierarchical Test Sequence Editor. A native condition compares a recorded value and chooses exactly one branch. No JavaScript expression is evaluated by the condition. The selected branch executes normal test steps using the existing callback, numeric, timing, reporting and Cleanup machinery.

**This is a development candidate.** It has source-level/offline verification only; Windows Qt 6.9.2 compilation, new CTests and physical instrument tests are pending.

### Author an IF

1. Open the Test Sequence Editor; in **Step Details**, click **+ IF**. It is inserted as a Main sequence row. If you selected a step inside a sweep, the IF is inserted inside the same sweep.
2. Select the IF row. In **Data source**, choose Test variable, Sweep/repeat parameter, Recorded measurement, Step status, Run Information, or Test counter.
3. Choose the name/step from the lists, comparison operator, and typed comparison value. For a structured measurement, enter the named result path (such as `vpp`). Only numbers support ordering (`<`, `<=`, `>`, `>=`); text and booleans support equality/inequality.
4. Add your THEN steps as children beneath IF (or select existing steps and use **Indent**). Choose **+ ELSE** while selecting IF to create a false-path group. Its child steps are edited normally.
5. **Preview Execution** shows both branches and their conditions. Their final outcome is unknown until runtime; preview performs no instrument commands and no script execution.
6. Save to Panel, save the `.wfpanel`, and then RUN. Nonselected branches are reported as **SKIPPED**, not FAIL. Cleanup is always outside conditional branches.

### Test Variables catalog

The new **Test Variables** tab lets you declare a variable's name, type (`number`, `string`, `boolean`), an optional initial value, and a description. Initial values are restored at the start of each run. Script authors continue to use the existing native calls:

```javascript
export function setup(step) {
    if (!TestRunner.setVariable('temperature', 48.5))
        throw Error(TestRunner.lastError);
    TestRunner.complete(step.token, true);
}
```

`setVariable()` is supported from an active synchronous test handler; it does not itself accept a completion token. The previously existing `TestRunner.variable('temperature')` and `TestRunner.variables()` methods remain available. A variable declared without an initial value must be set **before** any condition reading it executes. Undeclared variables may still be set by existing scripts, but the variable picker lists declared names only.

### Where can IF read from?

| Source | Example | What is evaluated |
|---|---|---|
| Test variable | `temperature > 45` | Current run-scoped value |
| Sweep/repeat parameter | `voltage <= 3` or `iteration == 2` | Current expanded step's inherited parameters |
| Recorded measurement | Step `readVoltage`, path `voltage`, `> 4.75` | Most recent relevant completed measurement matching current sweep/repeat context |
| Step status | Step `initialize`, `== "pass"` | Status of a prior step |
| Run Information | `uutId == "Model-A"` | Immutable operator/UUT snapshot captured at RUN |
| Test counter | `failedMeasurements > 0` | Current native summary counters |

The comparison always uses **stored values**. It does not run JavaScript or issue a new instrument measurement. Missing data or incompatible types produce an ERROR and enter Cleanup, rather than silently choosing ELSE. A condition returning false is not a measurement FAIL.

### Limits and safety

- Conditions and their branches are allowed only inside Main; Setup and Cleanup retain their existing fixed ordering.
- Branches may be nested inside bounded sweeps and repeats; their child steps retain inherited values and `templateId`s. Expanded execution stays within the existing 100-step limit (both potential branches are counted).
- Only IF/ELSE is added, not unrestricted GOTO, parallel execution, or unbounded conditional retries.
- A numeric measurement that fails with **Stop Main on failure** enabled will still initiate Cleanup before an IF intended to inspect that failure; disable that policy deliberately when a diagnostic branch should handle an ordinary out-of-limit reading.
- A conditional cannot bypass physical output safety checks. Its evaluations read only stored data; instrument operation guards and the original Cleanup logic are unchanged.
- A branch not chosen is archived with `status: "skipped"` and reason `Conditional branch not selected`. IF entries include a boolean decision and the evaluated source in their detail text.

### Hardware-free example

Open `WFScript_Conditional_Flow_Lab_RC1_5_14.wfpanel`. Setup stores a 48.5 °C simulated temperature in a declared run variable. The top-level IF chooses the high-temperature branch. Inside a three-point voltage sweep, another IF evaluates the actual named `voltage` measurement of the **current iteration**, choosing THEN at 5 V and ELSE at 4.75 and 4.50 V. The normal waveform and diagnostic demonstrations are retained.

The example has 24 expanded rows counting both conditional branches, of which four should be marked SKIPPED, and 17 numeric measurements. It requires no instruments. Those expectations have not yet been verified on the native Windows build.
