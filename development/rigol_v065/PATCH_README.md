# Rigol v0.6.5 panel-only patch

Applies **only** to an extracted WFScript **RC1.5.5** complete-source checkout with `development/rigol_v064/` present. Extract the patch into the repository root, preserving folders. It adds `development/rigol_v065/` and does **not** replace or remove any existing C++ files, embedded Rigol driver modules, or earlier Panels. Open `development/rigol_v065/Rigol_Four_Instrument_Operator_Bench_v0_6_5_Automated_Tests.wfpanel` using the **Windows RC1.5.5 executable**; no native rebuild is needed for the new Panel itself. The tests and generator build helper are provided for developers.

For the Panel alone, download the standalone `.wfpanel` instead of applying the source patch.

The new Test Executive commands physical outputs. Read `WINDOWS_TEST_GUIDE_v0_6_5.md` before running live equipment.
