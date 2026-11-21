# Preview a Test Before Running It

The **Execution Preview** shows the steps that WFScript will execute after expanding any Sweeps and Repeats. It helps you check an experiment without running the script or energizing an instrument.

## Open the preview

In the Test Sequence Editor, select the Sweep or Repeat row you want to inspect and click **Preview Execution…**, beside **Apply Step Changes**. The **Execution Preview** tab opens with *Selected row and its children* chosen. You can switch **Show** to *Entire sequence* or click **Refresh** after editing.

For a three-point sweep from 5.00 V to 4.50 V in −0.25 V increments, repeated three times, the preview shows the measurement step nine times: 5.00 V with iteration 0, 1, and 2; then 4.75 V with iterations 0, 1, and 2; then 4.50 V with iterations 0, 1, and 2. Selecting *Entire sequence* also shows Setup and Cleanup in their actual order.

The columns show the **global execution number**, Setup/Main/Cleanup group, expanded operation name, JavaScript procedure, resolved step parameters, acceptance checks, and the originating authored step. Long values have tooltips; you can resize the columns and scroll horizontally.

## What the preview does and does not do

The preview uses WFScript's existing native loop expander, including its limit expressions and maximum-100-steps safeguard. It shows values such as `voltage = 4.75` and `iteration = 2`, and converts relative limits to actual numbers for each iteration. For an array-wide named check such as `temperatures[*]`, the rule appears here but the exact per-element pass/fail count is determined only when runtime results are received.

The preview **never executes** your JavaScript, opens instruments, changes outputs, archives reports, or edits any generated step. It is a read-only inspection tool. For changes, go back to **Step Details**, edit the authored Sweep or child procedure, and preview again. **Save to Panel** commits those changes to the project; save the `.wfpanel` separately to persist it to disk.

## Preview errors

An invalid range, duplicate loop parameter, or oversized expansion displays an error above the preview table. Fix the authored configuration and click **Refresh**. A preview error is not a test failure and does not operate hardware.

## Related topics

- The existing **JSON Preview (read-only)** shows the authored definition for advanced inspection.
- A loop executes its child procedures sequentially for every setpoint. Repeating a sweep combination provides independent observations at each test condition.
- Run Information, reports, callbacks, and named structured-result evaluations are the same as RC1.5.12.
