# Native Binary Lab RC1.3 — developer and operator notes

This opt-in native binary subsystem extends the RC1.2 source that the owner successfully ran under Windows Qt6. No physical MHO954 acquisition has yet been attempted through this new buffer interface.

**Four-page test Panel:** `NativeBinaryPerformanceLab.wfpanel`. The embedded JavaScript is synchronized with `NativeBinaryPerformanceLab.js`. Use the **recompiled RC1.3 executable**. Pages 01–02 preserve the previous synthetic SCPI 100k/1M and actual localhost TCP comparison. Page 03 tests typed read/write and actual FileIO native-buffer persistence; Page 04 tests Intel/Motorola DBC field packing and native scaled sample/bins processing.

The new FileIO test writes `binary_rc13_roundtrip.bin` inside the normal shared PanelData folder. It never accesses arbitrary external paths. The original TCP benchmark uses only `127.0.0.1`, port 0; there are no instrument connections or deliberate hardware outputs anywhere in the Lab Panel.

The native `BinaryBuffer` has read/write LE/BE 8/16/32/64 integer and 32/64-bit float operations, signed fields, strict bounds, text/base64/hex, CRC16/CRC32, sums, search, bit-level CAN DBC packing and scaled values. Native `BinaryStream` can assemble text, IEEE488.2, fixed, length-prefixed and delimited frames. `BinarySamples` retains decoded numeric data and exports stats or min/max bins for a future plot bridge. The existing legacy communications and FileIO APIs are unchanged.

**API contract and limitations:** [docs/BINARY_API_RC1_3.md](../../docs/reference/BINARY_API_RC1_3.md)

Offline checks from the source root:

```text
python tools/run_offline_tests.py
python tools/check_api_knowledge.py
python tools/check_repo.py
python tools/check_release.py
node tests/binary_lab_rc13_panel_mock.js
```

The native C++ `WFScriptBinaryBridgeSmoke` and `WFScriptUtilitySmoke` tests have additional RC1.3 cases. They have **not** been compiled or executed on Windows for this candidate. Qt6 is unavailable in the source-authoring Linux environment. The CMake UtilitySmoke dependency has been updated for `BinaryBridge`.

Do not infer a MHO954 waveform download speedup from synthetic benchmarks. Keep the working Rigol v0.4.9 transfer path unchanged until an apples-to-apples instrument test.
