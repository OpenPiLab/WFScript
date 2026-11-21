# WFScript RC1.5.9 — Parameterized Callback Test Steps

## A short example

The **Visual Test Sequence Editor** holds this step (JSON shown only for explanation):

```json
{
  "id": "psu_500",
  "name": "Measure PSU CH1 at 5.00 V",
  "group": "main",
  "type": "numeric",
  "mode": "callback",
  "handler": "measureSupply",
  "parameters": { "voltage": 5.0 },
  "limits": { "low": 4.90, "high": 5.10 },
  "unit": "V",
  "timeoutMs": 10000
}
```

The library contains **one reusable procedure**, rather than `power500`, `power475`, and so on:

```javascript
export function measureSupply(step) {
    const target = step.params.voltage;
    // Pseudocode: call your existing protected SCPI instrument API.
    PSU.setVoltage(target, function(error) {
        if (!TestRunner.isActive(step.token)) return;
        if (error) {
            TestRunner.fail(step.token, error.message);
            return;
        }
        PSU.readVoltage(function(readError, voltage) {
            if (!TestRunner.isActive(step.token)) return;
            if (readError) TestRunner.fail(step.token, readError.message);
            else TestRunner.complete(step.token, voltage);
        });
    });
}
```

`PSU.setVoltage` and `PSU.readVoltage` here are **illustrative method names**, not newly introduced WFScript global APIs. The physical Rigol Panel uses its existing SCPI drivers and instrument service guards. Do not use the snippet verbatim for hardware.

To test 4.75 V, change the step parameters to `{ "voltage": 4.75 }`, choose the same `measureSupply` handler, and configure limits in the editor. The C++ Test Executive produces a separate result and report row for each step.

## What C++ passes

- `step.id`: saved step ID, e.g. `psu_500`
- `step.name`: human-readable label
- `step.params`: saved JSON parameters object (or `{}`)
- `step.token`: an opaque, one-time token unique to this *invocation* of the step
- `step.runId`: the current run's execution ID

This object is supplied to **Once** and **On Start** handlers, including new **Callback** steps. Older no-argument exports remain valid. Poll mode still supplies the original `(stepElapsedMs, runElapsedMs)` first and adds the context *third*. On Finish still receives `(status, elapsedMs)` first and adds context *third*.

## Who owns completion?

For `mode:"once"`, the function returns its result immediately. For `mode:"poll"`, the executive periodically invokes a polling function. For `mode:"callback"`, the handler initiates work and returns normally, without a result; **only an explicit `TestRunner.complete(step.token, value)` or `TestRunner.fail(step.token, message)` finishes it**. C++ continues the timer, deadline, limits, progress and report updates. No JavaScript polling function is needed.

- Numeric step: `complete(token, finiteNumber)` → C++ compares numeric limits → PASS/FAIL.
- Action/test step: `complete(token, true)` → DONE/PASS; `complete(token, false)` → FAIL.
- Failure: `fail(token, 'explanation')` → ERROR; normal error policy proceeds to Cleanup.
- Duplicate, stale, canceled or timed-out token → false, with no mutation of the next step.
- Synchronously calling `complete` from within a handler is allowed: C++ defers the state transition until after the JavaScript call stack returns.
- A returned JavaScript Promise **is not automatically awaited**. Attach `.then(success, failure)` in the test library and call `complete` or `fail` from those callbacks. The Rigol v0.6.7 example shows how.

## STOP, physical safety and concurrency

The native token only controls which completion reply the executive accepts. It does **not** revoke an already-sent hardware command. An instrument may finish an operation after STOP, so every asynchronous continuation that might send another command must check `TestRunner.isActive(step.token)` or an equivalent driver operation guard. Keep output ownership, command serialization and timeout quarantine in the instrument layer. Use `TestRunner.terminate()` for normal operator STOP and a Cleanup group that sends OFF and verifies the actual output state; after communications loss, instruct the operator to verify the output physically.

Do not use `TestRunner.abort()` as an expected safe output-off mechanism; it skips the sequence Cleanup group. Likewise, the Panel's red STOP may interrupt the engine and cannot promise a remote SCPI instrument output will switch off.

## Editing

Open **Panel Designer → Test Sequence**. In Step Details choose **Callback**, select or type the handler, and press its adjacent **Edit** button. If it is not defined, choose to create an exported stub. Edit the code in the **Test Library JavaScript** tab, using the normal WFScript syntax editor. Enter parameters as JSON, configure the step's numeric limits and timeout, then **Save to Panel** and separately save the `.wfpanel` to disk. No JavaScript is executed while editing.

For an entirely safe first test use `development/callback_lab/TestExecutiveCallbackLab.wfpanel`. It contains simulated delayed replies, a measurement table, an event log and cleanup with no physical I/O.
