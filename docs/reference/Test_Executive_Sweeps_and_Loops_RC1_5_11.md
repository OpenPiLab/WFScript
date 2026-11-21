# Test Executive — Sweeps, Repeats, and Reusable Procedures (RC1.5.11)

This revision adds **bounded, declarative iteration** to WFScript's project-owned Test Sequence. One authored loop group expands into independently reportable native test steps **before RUN**. No dynamic JavaScript `while` loop is added to the Test Executive. The existing callback token, deadline, acceptance, stop-on-failure and Cleanup behavior continue to apply to each expanded step.

## Quick start in the visual editor

1. Open **Test Sequence Editor** and select **Step Details** (the default tab). The table shows an authored `⟳` sweep as one group row.
2. Select that row and open **Loops & Sweeps**. You can edit the group, its axes, its child procedures and relative limits in a single JSON document. Use **Preview Expanded Steps** before saving.
3. Use **Edit Child Procedure** to jump to an existing exported handler in the embedded JavaScript editor. If a child handler does not exist yet, create/export it in the **Test Library JavaScript** tab; the child navigation button does not yet generate stubs.
4. Use **Save to Panel**, then save the `.wfpanel` project to disk. The authored compact definition is retained; the generated steps are *not* written back as dozens of separate definitions.
5. On RUN, the native Test Executive validates the *entire expanded plan* before beginning and gives every generated step its own parameters, token, timeout, PASS/FAIL and report entry.

The editor includes **+ Sweep** and **+ Repeat** starter templates. Its loop-group editor currently uses documented JSON rather than a fully visual per-axis/child-grid editor. That is a remaining authoring improvement, not a hidden script generation mechanism.

## Numeric range with two child procedures

```json
{
  "id": "voltage_sweep",
  "name": "PSU + DMM sweep",
  "group": "main",
  "loop": {"parameter": "voltage", "start": 5, "stop": 2, "increment": -0.25},
  "steps": [
    {
      "id": "psu_readback", "name": "PSU at ${voltage} V",
      "type": "numeric", "mode": "callback", "handler": "setAndReadPsu",
      "limits": {
        "low": {"param": "voltage", "offset": -0.1},
        "high": {"param": "voltage", "offset": 0.1}
      }
    },
    {
      "id": "dmm_reading", "name": "DMM at ${voltage} V",
      "type": "numeric", "mode": "callback", "handler": "readDmmAt",
      "limits": {
        "low": {"param": "voltage", "offset": -0.075},
        "high": {"param": "voltage", "offset": 0.075}
      }
    }
  ]
}
```

The two functions execute *in that order* at **each** voltage. Thirteen setpoints generate twenty-six executable steps, preserving the same PSU/DMM interleaving as the physical Rigol v0.6.8 test. The native engine injects `step.params.voltage` into both functions. It also resolves the relative acceptance bounds using that exact setpoint; it does **not** rely on JavaScript performing pass/fail comparisons.

## Cartesian sweep over two or three axes

```json
{
  "id": "waveform_sweep",
  "name": "Waveform sweep",
  "group": "main",
  "loop": {
    "axes": [
      {"parameter": "amplitude", "values": [1, 2]},
      {"parameter": "frequency", "values": [1000, 5000]}
    ]
  },
  "steps": [{
    "id": "capture", "name": "${amplitude} Vpp at ${frequency} Hz",
    "type": "numeric", "mode": "callback", "handler": "generateAndVerifyWaveform",
    "parameters": {"shape": "sine"},
    "measurements": {
      "vpp": {"name": "Amplitude ${amplitude}",
              "low": {"param": "amplitude", "factor": 0.85},
              "high": {"param": "amplitude", "factor": 1.15}, "unit": "Vpp"},
      "hz": {"name": "Frequency ${frequency}",
             "low": {"param": "frequency", "factor": 0.97},
             "high": {"param": "frequency", "factor": 1.03}, "unit": "Hz"}
    }
  }]
}
```

This generates four ordinary waveform steps, with measured Vpp/frequency evaluated individually inside each step. The order is **first axis outermost**, final axis fastest-changing: (1 V, 1 kHz), (1 V, 5 kHz), (2 V, 1 kHz), (2 V, 5 kHz).

A `loop` can also be written directly on an individual step, without `steps: [...]`, to expand a single procedure. A `loop.axes` declaration allows up to three axes. Value lists accept finite numbers, strings and booleans; ranges accept finite numbers and require a reachable inclusive stop value.

## Bounded repeats

```json
{
  "id": "repeat_diagnostic",
  "name": "Three diagnostic passes",
  "group": "main",
  "repeat": {"count": 3, "index": "trial"},
  "steps": [{
    "id": "diagnostic", "name": "Diagnostic ${trial}",
    "type": "numeric", "mode": "callback", "handler": "readDiagnostic",
    "limits": {"low": 0, "high": 2}
  }]
}
```

The callback receives `step.params.trial` values **0, 1, 2**. Repeat and sweep can be combined, with each sweep combination receiving the requested repeat count. Nested loop groups are allowed to depth 3. These are *fixed, pre-expanded repeats*, not unbounded or `while`/condition-driven loops.

## Relative limit expressions

Scalar `limits` and named-result `measurements` accept either a numeric `low`/`high`, or an object:

`{"param": "expected", "factor": 0.97, "offset": -0.001}`

The native expansion computes `expected * factor + offset`; `factor` defaults to 1 and `offset` defaults to 0. The referenced `step.params.expected` must be a finite number. This is data-only arithmetic; **no JavaScript eval** occurs. Measurement names and step names may contain `${parameter}` placeholders for readable reporting.

## Limits and safety

- Maximum **100 expanded executable steps**, the existing Test Sequence capacity.
- Maximum **100 numeric evaluations per run**, preserving the Measurement Table capacity.
- Sweep: maximum 3 axes, up to 100 values per axis, and reachable inclusive ranges.
- Repeat: integer count **1–100**; loop nesting depth at most 3.
- Sweeps and repeats are permitted in the **Main** group only. Setup and Cleanup execute once and never inherit loop iteration context.
- The entire proposed expansion and all resulting handlers and limits are validated **before** replacing the previous runnable definition; a rejected loop cannot partially replace it.
- Generated step IDs are unique and carry `templateId` plus their actual step parameters in the report. Saved `.wfpanel` definitions retain the compact authored groups.
- The Test Executive uses existing callback tokens to reject stale replies. A token check does **not** cancel an outstanding physical SCPI command; output ownership, real-hardware interlocks, and Cleanup remain required.
- Native loops are currently sequential, not parallel; they have no conditional `break` or data-dependent branch feature. This is deliberate for reproducible hardware tests.

## Instruments and resource reuse

The physical Rigol v0.6.9 sample continues to share the **existing four operator-owned SCPI sessions** through `RigolTestHost` and the proven `runP()` guards. There are no duplicate sockets and no new, unaudited way to bypass exclusive output control. Its test library still calls the same `setAndReadPsu`, `readDmmAt`, `generateAndVerifyWaveform`, and Cleanup functions. This already demonstrates reusable instrument services across manual controls and automatic tests.

An application-wide C++ instrument resource registry (with binding, reservations, acquisition timeouts and cleanup ownership) may eventually be useful, especially for concurrent stations. It is intentionally **not** part of RC1.5.11: providing raw device objects without preserving the tested operation guards would be a regression, not a simplification.

## Example Panels

- `WFScript_Test_Executive_Sweeps_Loops_Lab_RC1_5_11.wfpanel`: hardware-free, five authored groups, twelve executable steps, seventeen numeric measurements.
- `Rigol_Four_Instrument_Operator_Bench_v0_6_9_Sweeps_and_Loops.wfpanel`: seven authored items, **38 executable steps / 50 numeric measurements** with the same driver invocation order, limits and output cleanup as the physically tested Rigol v0.6.8 test. Only this new v0.6.9 candidate includes a small waveform-key fallback and fixes the `undefined PASS / undefined FAIL` footer display. The older physical baseline remains unchanged.

**Verification:** Python/Node mocks and static checks pass. Windows Qt compilation, the two new native CTests, editor visual checks, cancellation during a live repeated sequence, and physical Rigol v0.6.9 operation are *not* yet verified.
