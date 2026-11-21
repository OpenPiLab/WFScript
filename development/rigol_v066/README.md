# Rigol Four-Instrument Operator Bench v0.6.6 — readable test implementation

**Status:** panel-only source/offline-tested candidate; **not yet Windows Qt or physical Rigol tested**.
The previous v0.6.5 Panel physically passed 62 steps/50 measurements; that hardware result does not automatically validate v0.6.6. WFScript RC1.5.5 remains the last user-verified app baseline; RC1.5.6 native result-widget changes remain an independently unverified candidate.

## Start here

- `Rigol_Four_Instrument_Operator_Bench_v0_6_6_Readable_Tests.wfpanel` — open in WFScript RC1.5.5 or RC1.5.6. No new native API is required.
- `libraries/RigolTestSteps.mjs` — **the actual test implementation**, organized by procedure (preflight, DC sweep, DMM measurement, waveform capture, cleanup). This is the file a test developer should start reading. The test library is embedded in the `.wfpanel` file.
- `RigolBench_4Instruments_v0_6_6.js` — Panel operator controls, unchanged four-instrument operator functionality, existing SCPI session manager (`runP`), and only a narrow `RigolTestHost` service object for the test library.
- The `.wfpanel`'s `testDefinition` — **the unchanged Test Executive source of truth** for 62 named steps, order, pass/fail limits, and timeouts.
- `TEST_AUTHOR_GUIDE.md` — where to make common edits; what is not configurable yet.
- `WINDOWS_TEST_GUIDE.md` — staged real hardware checks and unverified behaviors.
- `tests/test_v066.mjs` — 62-step mock plus failure/cancellation/interlock tests; uses fake SCPI replies, not real hardware.

## Architecture

`Visual Test Definition → exported handler → procedure in RigolTestSteps.mjs → RigolTestHost.run() → existing serialized Rigol driver → result back to TestRunner`.

The exports at the bottom of `RigolTestSteps.mjs` bind fixed values, e.g. `power500` calls `measureSupply(5.00)`. The meaningful code is `measureSupply`, not a method buried in the Panel script. These per-value exports are *still required by the current native v1 test-definition API*. A parameterized Test Executive would remove them; it is **not implemented in this release**.

The module also includes a temporary ~35-line start/poll adapter because the current Test Executive does not natively await a Promise. That adapter is **not a second scheduler**: it simply bridges the existing polling contract and the Rigol driver's callback-based operations. A Promise-aware native step mode is a proposed future change, not an RC1.5.6 capability.

There are **no new SCPI connections**, no separate copy of the test driver logic, no C++ changes, and no change to the native Test Executive definition.

## Improvements over v0.6.5

- All test procedures, including capture/analysis and cancellation bookkeeping, have moved into `RigolTestSteps.mjs`, rather than a long Panel script block.
- The Panel exposes only connection state, the existing per-instrument serialized operation runner, UI updates and read-only endpoint metadata.
- A waveform step independently verifies PSU CH1 physically OFF before any generator setup, even if the user edits the native step order.
- Generator ownership begins **after verifying its initial OFF state**, but **before asynchronous configure/enable**. A refused operator-owned ON output is not disabled by cleanup.
- The v0.6.5.1 non-overlapping Automated Tests layout is retained.
- The native Test Executive, 62 steps, 50 numeric checks, limits, and five original SCPI modules remain unchanged.

## Build and tests

From the repository root:

```sh
python development/rigol_v066/build_panel.py
node development/rigol_v066/tests/test_v066.mjs
python tools/run_offline_tests.py
python tools/check_repo.py
```

`libraries/WFScpiCore.mjs` is an **identical copy of the original SCPI core only for standalone Node import testing**. The actual `.wfpanel` embeds the original core from v0.6.5.1 unchanged. Do not manually add duplicate SCPI cores to Panels.

## Safe operation

The outputs must begin OFF. Connect PSU CH1 to DMM **voltage/COM** terminals, and generator CH1 to oscilloscope CH1 separately. Never connect the PSU output to the generator output. Do not operate this Panel unattended. Software cleanup can verify OFF only while the hardware/network responds; STOP is not a physical interlock. An OFF label without a successful physical readback must not be relied upon.
