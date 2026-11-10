# Testing WFScript v0.1

## 1) Offline repository checks

From the repository root:

```powershell
python tools/check_repo.py
python tools/build_examples.py --check
python tools/check_warning_cleanup.py
```

Optional JavaScript/offline example checks:

```powershell
node tests/v5_6_70_4_table_limit_smoke.js
node tests/v5_6_70_3_calibration_smoke.js
node tests/v5_6_70_3_multitrace_smoke.js
node tests/v5_6_68_operator_smoke.mjs
node tests/v5_6_68_ad3_hardware_station_smoke.mjs
```

## 2) Warning-cleanup release verification

The first Windows Debug build of WFScript v0.1 succeeded but emitted 373 GCC
warning messages, mainly repeated aggregate-initializer warnings. The warning
cleanup candidate addresses the exact reported categories without disabling
`-Wall`, `-Wextra`, or `-Wpedantic`.

**The revised files still require a Windows build.** In Qt Creator, run **Build
All** with native tests enabled and inspect any remaining `warning:` messages.
See [Warning cleanup audit](WARNING_CLEANUP_AUDIT.md) for the original counts and
fixes.

## 3) Build native Qt tests

Configure with:

- `WFSCRIPT_BUILD_TESTS=ON`

Then build all targets.

## 4) Run the full native CTest suite

```powershell
ctest --test-dir <your_build_directory> -j 2 --output-on-failure
```

**Previously validated source baseline (v5.7.0.12):**

- 66 tests passed
- 0 tests failed

The renamed/repackaged v0.1 source needs its own clean build and re-test before tagging.

## 5) Recommended manual checks before release

- Open **Getting Started** and run it in **Simulation**.
- Open each **Examples** group and load at least one Panel from each.
- Save and reopen a `.wfpanel` project.
- In a table example, confirm `Array.isArray(ui.get('recipeTable')) === true`.
- Run **Table-Driven Wavegen** in Simulation and verify multiple retained traces.
- Run a simulation Test Executive example and confirm cleanup behavior.
- If hardware is available, safely loop back the AD3 example as intended and confirm expected measurements.
- Check that archived JSON results appear in the Results Browser.

## 6) Example PowerShell command for the release gate

```powershell
$build = 'C:\Dev\WFBuild'
& 'C:\Qt\Tools\CMake_64in\ctest.exe' --test-dir $build -j 2 --output-on-failure
```
