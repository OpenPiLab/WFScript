# v0.6.5 change / verification report

## Source provenance

Source baseline: user-confirmed Windows Qt6.9.2/MinGW **WFScript RC1.5.5** complete source, including `development/rigol_v064/Rigol_Four_Instrument_Operator_Bench_v0_6_4_Native_Charts.wfpanel`. All original 4 operator Page objects and the 5 embedded SCPI library strings are byte-for-byte identical in the new .wfpanel.

## Change scope

**Panel-only**. Adds fifth Page `rigoltest`, project-native test definition and run information/report storage, one embedded `RigolTestSteps.mjs`, and a Panel JavaScript test adapter. The original Panel JavaScript is kept except for version annotation and 3 focused guards blocking ordinary connect/disconnect/runP commands while the test owns the instruments. All existing native chart APIs and C++ files are untouched.

## Test design

- 62 Test Executive steps, 50 numeric checks. The preflight also records explicit `rigolMode` and all four SCPI endpoints as native report variables (because generic report.instrument.mode is AD3-centric).
- 13 target voltages 5.00→2.00V at 0.25V intervals, measured independently by PSU and DMM (26 checks).
- 8 waveform conditions (sine / square × 1 / 2 Vpp × 1 / 5 kHz), each checked by CH1 scope Vpp, frequency, and AC RMS/Vpp (24 checks).
- Native C++ executor owns sequence, polling, limits, PASS/FAIL, cleanup, report and archiving.
- Explicit output OFF/readback preflight; separate generator and PSU OFF/readback cleanup; output ownership survives in-flight command interruption; no silent interpretation of TCP closing as output OFF.

## Tests actually run in container

- Node --check: appended operator code, module, standalone bridge parse successfully.
- New mock `tests/test_v065.mjs` exercises all 62 steps with 50/50 comparisons passing on **synthetic simulated instrument replies**, including independent PSU/DMM measurements and scope-sampled sine/square signals.
- Test asserts original 4 Page JSON records and original 5 embedded SCPI module sources are unchanged, and checks library handler names against testDefinition.
- Fault mocks cover initially ON PSU preflight refusal (no output change), DM858 overload (failure + PSU OFF), STOP-before-PSU-enable, and STOP-during-generator-configure (output OFF after cleanup).
- Existing v0.6.4 mock `test_native_chart_064.mjs` passed without source edits.

## Not validated

- **No** Qt6 Windows application build/startup, native Test Executive / CTests, physical SCPI hardware, output cleanup timing under real TCP failure, or waveform accuracy calibration tested for v0.6.5. The mock does not constitute real hardware verification.
- Do not promote v0.6.5 to the validated baseline before testing; **RC1.5.5 remains the validated WFScript application** and **v0.6.4 remains the previous embedded Rigol Panel revision**.
- No emergency stop guarantee: STOP uses software cleanup, and panel/process termination or network loss cannot force physical instrument outputs OFF.
