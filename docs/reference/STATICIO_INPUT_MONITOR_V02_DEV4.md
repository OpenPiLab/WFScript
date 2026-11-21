# WFScript v0.2-dev.4 — Opt-in Static Digital Input Monitor

## Why this feature exists

A physical START switch, fixture interlock, proximity switch, or other slow digital signal can drive a Panel without JavaScript `setInterval()` loops. The C++ `StaticIOBridge` owns one `QTimer` on the **existing PanelRuntime / QJSEngine worker thread**. It reads **one 16-bit input snapshot** per poll, using only `FDwfDigitalIOStatus` followed by `FDwfDigitalIOInputStatus` in Hardware mode. It does not configure DIO directions, enable outputs, reset digital instruments, or call Scope/Wavegen configuration functions. The polling work nonetheless shares SDK/USB and script worker resources; users must measure performance with their actual workload.

## Activation is ALWAYS explicit

```javascript
// Registering does not start a timer or read the hardware:
StaticIO.onChange(0, function(level, pin, event) {
    if (level === 0) TestRunner.run(); // example; check TestRunner state/errors
}, {edge: 'falling', debounceMs: 30});

if (!StaticIO.startMonitor({pollIntervalMs: 20}))
    throw Error(StaticIO.lastError);

// Later, or in onShutdown():
StaticIO.stopMonitor();
```

When the Panel is loaded, **no timer is scheduled**. Monitoring is off by default, even when listeners have been registered. `startMonitor()` first samples all inputs to establish a baseline without emitting a false edge, then schedules the timer. `stopMonitor()` stops polling immediately but retains registered listeners for a subsequent explicit `startMonitor()`. `clearOnChange(pin)` and `clearAllOnChange()` remove callbacks; they do not start or stop polling. The Panel runtime also stops polling and clears callbacks during shutdown, before deleting the QJSEngine.

### Public API

| Member | Behavior |
|---|---|
| `StaticIO.onChange(pin, callback, options={})` | One callback per DIO0–DIO15, replaced on re-registration. `callback(value, pin, event)` |
| `StaticIO.clearOnChange(pin)` | Unregister one listener |
| `StaticIO.clearAllOnChange()` | Unregister all listeners |
| `StaticIO.startMonitor({pollIntervalMs:20})` | Explicit activation, integer 10–1000 ms; rejects double start |
| `StaticIO.stopMonitor()` | Stop polling without changing hardware outputs; safe to call repeatedly |
| `StaticIO.monitoring` | Read-only boolean reflecting timer state |
| `StaticIO.monitorStats()` | `monitoring`, `pollIntervalMs`, `watchCount`, `polls`, `changes`, `callbacks`, `readFailures`, `lastPollDurationUs`, `inputMask`, `lastError`, `timestampSource` |

The optional `onChange` parameters are `edge:'change'|'rising'|'falling'` (default `change`) and integer `debounceMs:0..1000` (default 30). A transition must remain unchanged for at least the requested debounce time before dispatch. A callback gets an event with `value`, `previous`, `pin`, `edge`, `stableMs`, `inputMask`, `timestampUtc`, `timestampSource`, and `simulation`. It executes in the Panel worker thread. Exceptions are reported through normal WFScript script diagnostics. A failure to read from the device stops the monitor and issues a diagnostic rather than inventing a 0 or a 1.

## Signal timing and safety limits

This is **host-polled digital state**, not a hardware interrupt or precision edge capture. Pulses shorter than the poll interval can be missed and `timestampUtc` is host time *after* the SDK read, not the actual hardware edge timestamp. A 20 ms interval and 30 ms debounce are appropriate to a slow pushbutton, not a high-speed encoder, clock, or critical safety input. Use the Logic Analyzer for fast signals and hardware-timed triggering. Do not use WFScript/AD3 polling as a safety-rated emergency stop or guard interlock; those require dedicated hardware.

The feature does not silently claim the digital resources used by UART/SPI/I²C/CAN. Starting while an SDK digital protocol owns the resource is rejected. If a digital protocol becomes active later, the next monitor poll detects that conflict and stops with a diagnostic. Scope and Wavegen have separate analog engines, but SDK calls and the USB connection still compete for time. Read-only polling should not *reconfigure* either analog instrument; it cannot be asserted to have zero timing overhead without the physical coexistence test.

## Simulated demonstration and physical wiring

The built-in **Digital Input Test Station** has three Pages: (1) explicit monitor lifecycle, manual status and simulated transitions; (2) callback registration/removal independent of polling; and (3) a project-owned Test Executive sequence started by a physical press. The `SIM` buttons explicitly check `StaticIO.simulation` before using `StaticIO.setPin`. They will never drive an AD3 output while a physical AD3 is selected. In simulation the input model reads back staged output levels and cannot reproduce genuine electrical switch bounce.

For physical use, configure DIO0 START and DIO1 fixture input with **external 10 kΩ pull-ups to a safe 3.3 V source**, and each momentary/fixture contact to GND. Use a common ground. Ensure both pins are left as inputs (high impedance) and do not apply industrial voltages. The sample Test Executive verifies DIO1 is LOW (fixture closed) with a numeric 1/1 PASS; it does not operate the Scope, Wavegen, relays, or any other instrument. It is an illustrative test station, **not a safety system**.

### Coexistence regression with a real Analog Discovery 3

1. Run the existing known-good Wavegen-to-Scope loopback **without** StaticIO monitoring, while logging actual frequency, RMS/amplitude, acquisition errors and latency. Do not assume numerical values without your own instrument settings.
2. Start `StaticIO.startMonitor({pollIntervalMs:50})`, then 20 ms and 10 ms in separate runs. Capture the same analog measurements, and log `monitorStats().polls` and `monitorStats().lastPollDurationUs`.
3. Compare both steady-state and streaming/record Scope behavior, including lost/corrupt sample counts if available. Check that the Wavegen never stops and the Scope configuration is unchanged.
4. Verify `stopMonitor()` makes `monitorStats().polls` stop incrementing while Scope/Wavegen continue, and test Panel Stop and restart. Confirm no false event on first baseline read.
5. Test conflict with an active SDK protocol and device-disconnect errors. Verify monitor stops rather than silently taking DigitalIn/Out ownership.

The native smoke test operates in simulation only. It does **not** substitute for this real-hardware test.
