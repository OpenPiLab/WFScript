# Pattern Generator — WFScript v5.6.48

## Overview

`Patterns1` and `Patterns` reference a single Digilent SDK-backed DigitalOut controller. This first release provides a hardware clock or PWM on **one physical DIO pin at a time**. The default is DIO0. It does not use JavaScript timers to toggle the output.

### Example

```javascript
const clock = Patterns1.Channels.DIO0;
clock.DIO.value = 0;       // Physical DIO0; .Pin.value is a WFScript alias
clock.Type.text = 'Clock';
clock.Output.text = 'PP';  // 3.3 V push-pull
clock.Idle.text = 'Z';     // High impedance when not running
clock.Frequency.value = 1000; // 1 kHz
clock.Duty.value = 50;       // 50%

if (!Patterns1.run()) throw Error(Patterns1.lastError);
try {
    const s = Patterns1.status();
    print('Configured', s.actualFrequencyHz, 'Hz,', s.actualDutyPercent, '%');
    // Use Logic1 for actual digital acquisition while clock is running.
} finally {
    if (!Patterns1.stop()) print('STOP NOT CONFIRMED', Patterns1.lastError);
}
```

| API | Meaning |
|---|---|
| `Patterns1.Channels.DIO0.DIO.value` | Physical DIO pin index 0–15, device-dependent; WaveForms-compatible selector name |
| `.Pin.value` | Alias for `.DIO.value` |
| `.Type.text` | `Clock` only |
| `.Output.text` | `PP` (push-pull) only |
| `.Idle.text` | `Z` (high impedance) only |
| `.Frequency.value` | Requested 1 to 1,000,000 Hz |
| `.Frequency.real` | Calculated realizable Hz after configuration |
| `.Duty.value` | Requested duty 1–99% |
| `.Duty.real` | Calculated realizable duty after configuration |
| `Patterns1.run()` | Program and start hardware DigitalOut; Boolean success |
| `Patterns1.config()` | Program without starting the clock |
| `Patterns1.stop()` | Stop and disable owned DigitalOut channel |
| `Patterns1.status()` | Configuration + running flag + SDK-base-clock data; includes `simulation` |
| `Patterns1.lastError` | Latest error message |

**Difference from WaveForms:** This is a deliberately small WaveForms-style subset, not a complete reimplementation. In WFScript, setting properties only stages values; explicitly call `config()` or `run()`.

## Signal generation and readback

The C++ native bridge queries the device's DigitalOut internal clock and counter/divider limits, calculates a legal divider and HIGH/LOW counter values, and reports the actual mathematical output frequency/duty. Physical electrical performance still needs measurement. The built-in `14 Patterns` Page arms Logic1 during a running clock, reads 16-bit digital sample words, extracts DIO0 and DIO1, and measures frequency, duty cycle and state agreement.

### Safety and coexistence

- Pins remain un-driven on opening a Panel; only Start/Test calls `run()`.
- Stop and script/Panel cleanup attempt to stop the DigitalOut and disable its output. If an SDK call fails, a warning is shown. Confirm with a meter when in doubt.
- Release Static I/O outputs using `StaticIO.stop()` before starting the Pattern Generator.
- Static I/O changes are blocked while this Pattern bridge reports running. There is not yet a comprehensive inter-instrument pin reservation manager.
- DIO signals are 3.3 V digital. Do not attach a DIO output to another driven output or directly to a high-voltage signal.
- The dedicated test loops DIO0 to DIO1 through a 1 kΩ resistor only. No external supply required.
- When simulating, all plots are synthetic preview data and intentionally labelled as such.

## Help and completion

Typing `Patterns1.` and `Patterns1.Channels.DIO0.` presents available members. F1 lookup covers the clock properties and lifecycle methods. The Script Editor includes a `patternclock` example snippet.

## Next steps after hardware validation

A subsequent revision can add multiple independent channels, counter and finite pulse trains, arbitrary digital data, a full shared-pin resource manager, and hardware DigitalOut→DigitalIn trigger synchronization.
