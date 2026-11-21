# Testing WFScript v0.2

Separate **offline/source checks**, **native Qt tests**, **manual Windows GUI**, and **physical AD3 measurements**. A passing mock does not prove instrument behavior.

## 1. Offline source checks (no Qt or AD3)

From the repository root:

```powershell
python tools/check_repo.py
python tools/build_examples.py --check
python tools/check_warning_cleanup.py
python tools/check_release.py
python tools/run_offline_tests.py   # Requires Node.js
```

The runner executes every Python and Node offline test in `tests/`. Run focused tests in the same directory during development, especially the AC/RC, protocol, visual bindings, communications, and Operator View contracts when changing those areas. The GitHub Actions job runs the release/source checks before compiling.

## 2. Native Qt suite (requires Qt 6.9.2 kit)

Set `WFSCRIPT_BUILD_TESTS=ON` in the Qt Creator CMake settings, reconfigure, and Build All. Then:

```powershell
ctest --test-dir C:\Dev\WFBuild-v02 --output-on-failure -j 2
```

Do not report a native pass count unless the current clean build actually completes. Native tests include QJSEngine examples, widget factories, Panel runtime, test sequences, and Operator View GUI smoke checks. The CI source build is MSVC; owner's workstation is MinGW, so run both when practical.

## 3. GUI acceptance

- Main Windows application title displays **WFScript v0.2** (no development suffix).
- Open and run Getting Started, Panel Data Lab, AC Network Laboratory, RC Step Response Laboratory in Simulation.
- Operator bar displays **WFScript / Panel Name / Current Page**, switching Pages without restarting the script. F11/Escape restores the development layout without terminating the active Panel.
- Fit Page, Console/Watch right dock tabs, float/dock, word wrap, filters, per-run history, changed values, and Stop all work correctly.
- Equation symbols render correctly on RC least-squares Page and with the Equation Symbol QA Panel.
- Save/Load an existing v0.1 `.wfpanel` into a new path without corrupting it.

## 4. Physical AD3 acceptance (safe wiring)

- Check physical device connection/disconnection, Wavegen OFF on stop/error, and Scope acquisition with a known safe signal.
- Verify at least one AC network fit/residual capture and one RC step exponential capture; clearly label real versus synthetic output.
- Confirm CSV/JSON export and a safe Stop/abort during a running acquisition.
- Run relevant digital/protocol tests only with appropriate known wiring and safe levels.

## 5. Release gate

Record the exact commit/tag, Qt version and kit, all test output, operator screenshots, AD3 steps actually performed, and any unresolved issues in the release checklist. Do **not** imply everything was Windows-tested merely because a source archive or GitHub CI workflow exists.
