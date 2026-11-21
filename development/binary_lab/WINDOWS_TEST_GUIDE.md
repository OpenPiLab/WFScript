# WFScript v0.2 Native Binary Lab RC1.2 — Windows Test Guide

**Candidate status:** User compiled WFScript.exe and ran synthetic 100k WORD and protocol/file benchmarks in Windows Qt 6.9.2. RC1.2 repairs only the localhost TCP Panel lifecycle. The RC1.2 localhost test is **not yet hardware/Windows-validated**; the native CTests have not all passed; physical SCPI testing has not been performed with this new API. This is **not** the published v0.2 release.

## Install and build

Use the complete source ZIP in a NEW directory alongside your existing release-candidate checkout, or use the incremental patch on a fresh copy of the exact `WFScript_v0_2_GitHub_Release_Source(3).zip` source. Do not overwrite the owner-validated source until this prototype passes.

**For the incremental patch:** Extract the patch ZIP into a separate staging directory, copy a fresh extraction of the exact baseline ZIP into your target source directory, and run:

```bat
python apply_binary_patch.py "C:\Users\<you>\Desktop\Qt6\WFScript_v0_2_Binary_Test"
```

Run that command from the extracted patch staging directory, where `apply_binary_patch.py` and `PATCH_MANIFEST.json` reside. The patch script verifies original file hashes and refuses unexpected edits. The complete source ZIP is simpler and avoids patching altogether.

Run from a **Qt 6.9.2 MinGW developer shell**:

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DWFSCRIPT_BUILD_TESTS=ON
cmake --build build --config Release -j 6
ctest --test-dir build -C Release --output-on-failure
```

Native test name: `WFScriptBinaryBridgeSmoke`. There are 83 previously registered native tests, plus one new binary smoke test (84 total), subject to the same build settings.

## Panel test procedure

1. Launch the **new executable** (not the previous v0.2 release-candidate executable).
2. Open `WFScript_Native_Binary_Performance_Lab_v0_2_RC1_2.wfpanel` and Run in Simulation.
3. Page 01: run the 100k-point SCPI synthetic test three times, then 1M at least twice. Every row should say PASS. Confirm 100k → 200,000 payload bytes and 1M → 2,000,000 payload bytes. Note native vs JS timings and speedup factors.
4. Page 02: run File/Packet. Check PASS for 1 MiB sum32, 4096-byte Modbus CRC16, and endian/float32 fields. Scalar reads may show JS faster than native.
5. Page 02: run LOCAL TCP. Both legacy and native should report 200,009-byte **framed** transfers and PASS. The RC1.2 Panel ignores only an expected server-peer RemoteHostClosedError *after the complete request was echoed*, but fails on a premature close and reports the byte count. It binds only `127.0.0.1`; no external Rigol device is accessed.
6. On Page 01 press SAVE CSV. Confirm `binary_lab_metrics.csv` appears in the Panel's project data directory; save the Console output too.

## Interpreting the results

- `Binary.clockUs` is monotonic and reports microseconds. Values displayed are milliseconds.
- The timed SCPI test is simulated: it measures native/JS data handling **without Ethernet or a scope**.
- TCP loopback is actual local TCP between WFScript client/server and includes sends, receives and framing. It is not an isolated `QVariantList` conversion benchmark, but it does exercise the exact new optional native event path.
- Existing `TCP.on('data')` and `Serial.on('data')` still provide byte arrays; native `.on('binary')` provides a buffer. Avoid registering both for the same connection in a performance test.
- The new `Binary` methods do not yet work in old WFScript executables. The Rigol v0.4.9 Panel has not been converted and should work unchanged.

## If a test fails

Copy the complete compiler error or CTest output, then the Panel Console output and the CSV. Particularly useful: the name of the failing case, Windows Qt version, the legacy/native TCP callback counts, and the actual expected/received byte counts. Do not use an external instrument as a workaround for a failing localhost test.

## RC1.2 localhost fix

The Windows RC1.1 Panel reported `The remote host closed the connection` during localhost trials. Its server peer error callback treated the normal RemoteHostClosedError after an intentional client close as a fatal failure; duplicate aborts were also possible. RC1.2 tracks server echo byte counts and recognizes only the expected closed-after-complete case. It also closes accepted peers on cleanup and distinguishes early disconnects, with one terminal error report per run. No native Qt C++ code changed. This was validated with a Node lifecycle simulation, **not** with live Qt TCP; test again on Windows.
