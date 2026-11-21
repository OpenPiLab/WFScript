# Build your first useful WFScript Panel — a simulated voltage divider

This tutorial starts with a blank Panel. It needs **no hardware** and teaches the same workflow used throughout the built-in examples. You will create a small calculator with three inputs, a result display, a button and a status message.

## 1. Create a new Panel

In WFScript, choose **New Panel**, remain in **Design** mode and add the following controls from the Toolbox. In each control's Properties give it the exact ID shown below. Set reasonable initial values for the numeric controls.

| Widget type | Widget ID | Visible caption | Example initial value |
|---|---|---|---|
| Numeric Stepper | `supplyVoltage` | Supply (V) | 5 |
| Numeric Stepper | `topResistor` | R1 (kΩ) | 10 |
| Numeric Stepper | `bottomResistor` | R2 (kΩ) | 10 |
| Digital Display | `dividerOutput` | Calculated output (V) | 0 |
| Label | `dividerStatus` | Calculation status | Press CALCULATE |
| Button | `calculateButton` | CALCULATE | — |

The IDs—not the visible captions—are how JavaScript addresses widgets. Capitalization must match exactly.

## 2. Use the visual Event editor

Select `calculateButton`. Open **Properties → Events → clicked** and choose **Create Handler** or assign the name `onCalculateClicked`. Use **Go to Handler** to open the script editor. You should have **one** saved `clicked` binding and **no** manual `ui.on('calculateButton',...)` call.

Paste or edit the handler to implement this calculation:

```javascript
// This runs when the operator clicks CALCULATE.
function onCalculateClicked() {
    const vin = Number(ui.get('supplyVoltage'));
    const r1 = Number(ui.get('topResistor'));
    const r2 = Number(ui.get('bottomResistor'));

    // A real divider cannot have negative resistance or zero total resistance.
    if (![vin, r1, r2].every(Number.isFinite) ||
        r1 <= 0 || r2 <= 0) {
        ui.set('dividerStatus', 'Enter positive R1 and R2 values.');
        return;
    }

    // For a simple unloaded divider, Vout = Vin × R2 / (R1 + R2).
    // Both resistances use kΩ; the ratio is dimensionless.
    const vout = vin * r2 / (r1 + r2);

    ui.set('dividerOutput', vout);
    ui.set('dividerStatus', 'SIMULATED — ideal, unloaded divider.');
}
```

This formula assumes an **unloaded, ideal** resistor divider. The output is calculated, **not measured** by the AD3.

## 3. Test and learn

Click **Run Panel** (it automatically focuses the Panel). With Vin = 5 V and R1 = R2 = 10 kΩ, press CALCULATE. The displayed output should be 2.5 V. Change R2 to 20 kΩ and calculate again: the expected output is approximately 3.333 V.

Try changing the button caption, or make R1 twice R2. Then Stop/Design and **Save As** your own `.wfpanel` project.

## 4. Add live updates (optional)

Once the button version works, bind each Numeric Stepper's **valueChanged** event to `onCalculateClicked`. All three controls may point to the **same named function**. Now changing an input updates the output automatically, without removing the CALCULATE button or adding `ui.on()` calls.

Remember that control values are inputs, while the Digital Display shows a **calculated** result. This pattern generalizes to sensor calibration, test-limit calculators and device configuration Panels.

## 5. Debugging checklist

- The Panel must be in Run mode before operator click events execute.
- The widget IDs must exactly match the strings passed to `ui.get()` and `ui.set()`.
- The button must have an enabled saved `clicked` binding to the named function.
- Don't add `ui.on()` for an event already bound visually; otherwise the same action can run twice.
- If the output is blank, inspect the Script Editor error location and verify all three numeric values.

## Next examples

Continue with [Widget Playground](guides/WidgetPlayground.md) and [Panel Timers](guides/PanelTimers.md). The full [beginner path](LEARN_WFSCRIPT.md) also introduces local TCP/UDP communications and SQLite persistence.
