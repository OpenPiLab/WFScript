# Scope range discovery — WFScript v5.6.45

The AD3 has two physical analog input gain stages. The WaveForms SDK can report
multiple *selectable peak-to-peak range values*, which are not distinct gain
stages. Do not hardcode a supposed AD3 step list: query your connected device.

```javascript
var caps = Scope1.capabilities();
print('Available:', caps.rangeStepsAvailable);
print('SDK selectable ranges (Vpp):', Scope1.Channel1.Range.steps);
print('SDK info min/max:', caps.rangeMinVpp, caps.rangeMaxVpp);

Scope1.Channel1.Range.value = 10;  // Requested 10 Vpp
if (!Scope1.config()) throw Error(Scope1.lastError);
print('Requested Vpp:', Scope1.Channel1.Range.value);
print('Effective Vpp:', Scope1.Channel1.Range.real);
```

- `.value`: requested Vpp. Unchanged from previous WFScript revisions.
- `.real`: effective value read from `FDwfAnalogInChannelRangeGet` following a
  successful `Scope1.config()` or acquisition setup. In simulation it mirrors
  the request, not an actual hardware measurement.
- `.steps`: SDK-advertised step array from `FDwfAnalogInChannelRangeSteps`.
  Queried **once** when a physical Scope bridge is constructed. `[]` when
  simulated or the installed SDK lacks the optional export.
- `Scope1.capabilities()` now includes `rangeStepsAvailable`, `rangeSteps`,
  and, if provided, `rangeMinVpp`, `rangeMaxVpp`, `rangeStepCount`.
  For failure/missing SDK functions check `rangeStepsError`.
- Steps and min/max are discovery data. Do not assume only returned step
  values can be requested, and do not equate them with hardware gain ranges.

The bundled `10 AD3` Page has a **QUERY SDK RANGES** button (read-only) and
now captures at 10 Vpp with both DC mean and RMS displayed. The `02 Signal Lab`
Page reports requested and effective ranges after a capture. Its fixed
2/5/10 Vpp choices remain as examples; dynamic runtime rewriting of the
Option Selector is **not** part of this focused revision.

## Suggested real-hardware acceptance checks

1. With AD3 connected in Physical mode, click `QUERY SDK RANGES` on Page 10.
   Record the exact printed list and min/max; do not assume a particular list.
2. Request 5 Vpp, call `Scope1.config()`, read `.real`; repeat with 10 Vpp.
3. Apply +3.5 V DC through the BNC board, DC coupling, 1x probe, and verify
   an accurate reading with the 10 Vpp scope setting, versus clipping at 5 Vpp.
4. Verify both channels expose the same step list. Test simulation (empty
   hardware steps), no-device startup, and older DWF DLL behavior.
5. Confirm that querying ranges does not energize W1, V+, or V-.

Native Windows build and physical AD3 checks must be performed by the user;
source/static tests are not a substitute for hardware validation.
