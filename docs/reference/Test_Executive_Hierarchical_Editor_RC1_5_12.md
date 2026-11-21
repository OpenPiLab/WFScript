# Editing automated tests in WFScript

The Visual Test Sequence Editor is where you define the **order of operations**. JavaScript test procedures still perform the instrument work. The native Test Executive runs the procedures, applies limits and records each result.

## The step tree

The **Step Details** tab has a full-width tree below the selected-step properties. It groups rows into **Setup**, **Main** and **Cleanup**, and shows Sweep / Repeat rows with their child steps indented beneath them. Click a triangle to expand or collapse a group.

A test can look like:

```text
SETUP
  Verify instruments and output states
MAIN
  Voltage sweep — 5.0 to 2.0 by −0.25
    Set PSU, read back output
    Read independent DMM voltage
  PSU output OFF and verify
  Waveform sweep — 2 amplitudes × 2 frequencies
    Capture scope and check waveform measurements
CLEANUP
  Generator output OFF and verify
  PSU output OFF and verify
```

The **Setup** and **Cleanup** sections remain outside loops. Cleanup still executes once following Main, including after a requested stop (subject to the existing runtime/cancellation guarantees).

## Create a voltage sweep

1. Select a Main step and click **+ Sweep**. WFScript places the new sweep just above the selection.
2. Select the sweep row. Set a clear name, such as *Voltage sweep*, and set the parameter name to `voltage`.
3. For a numeric range, choose **Range** and enter Start `5`, Stop `2`, Increment `-0.25`. For discrete choices, choose **Values** and enter a comma-separated list.
4. Select an existing Main procedure directly below the sweep and click **Indent**. Repeat for other procedures, or select the sweep and click **+ Child Step**.
5. Select each indented child to configure its procedure, mode, parameters and limits. The loop's current `voltage` is automatically available to the child as `step.params.voltage`.
6. Click **Apply Step Changes** to update the selected row. Use **Sequence Settings → Preview Executed Steps** to inspect the expansion, then **Save to Panel**. Finally save the `.wfpanel` project to disk.

A sweep runs **all child steps in order** for one parameter value before advancing to the next value. For a multi-axis sweep, each combination runs all children. A Repeat group runs all its children the chosen number of times.

## Named results and acceptance limits

A callback procedure might complete with:

```javascript
TestRunner.complete(step.token, {vpp: 2.05, hz: 1000.1, shape: 0.349});
```

Use **Named Results & Limits** to match result paths such as `vpp`, `hz`, or `channels[0]`. Fixed limits are ordinary numbers. For sweep-dependent limits, enter expressions such as `amplitude * 0.85`, `amplitude * 1.15`, or `voltage + -0.1`. The expression is resolved by the existing loop expander *before* the native limit evaluator runs. No arbitrary JavaScript expression evaluation is introduced.

The regular **Measurement / acceptance** column offers a checkbox for scalar relative limits, with a parameter and low/high offsets. This prevents changing a dynamic limit into a constant accidentally.

## Editing a procedure

Select a child step, locate its procedure in **Procedure and completion**, and click the adjacent **Edit** button. WFScript switches to the embedded **Test Library JavaScript** editor at that function. If it does not exist, you can create an exported stub. Both the definition and edited library can be staged together using Save to Panel.

## Moving and removing steps

**Move Up/Down** reorders within the current parent only. **Indent** moves a step into the immediately preceding Sweep or Repeat. **Outdent** moves a child after its parent, provided the parent retains at least one child. **Duplicate** copies the selected step or group with a new ID. **Remove** deletes the selected step and, for a parent, all of its descendants.

The tree intentionally doesn't use unconstrained drag/drop: moving an output-enable or cleanup step into a loop by accident could change hardware behavior. All edited definitions still undergo native expansion and validation before saving or running.

## Compatibility and release status

This RC1.5.12 editor revision is based on RC1.5.11. The execution engine and the physically exercised Rigol v0.6.8 Panel have not been modified. The updated editor and its native GUI smoke tests require Windows Qt compilation and visual inspection before considering RC1.5.12 stable.
