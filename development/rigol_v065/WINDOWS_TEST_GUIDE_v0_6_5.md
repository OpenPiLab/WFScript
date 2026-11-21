# Rigol Four-Instrument Operator Bench v0.6.5 — Native Test Station

**Status:** panel-only development candidate for **WFScript RC1.5.5 (Qt 6.9.2/MinGW)**. Source inspection and Node.js offline mocks passed. **No Windows GUI, C++ CTests, or physical four-instrument test has been performed for this revision.** No WFScript C++ files or original Rigol SCPI libraries were changed.

## What this Panel adds

A fifth page, **05 Automated Tests**, is appended to the four original Rigol operator pages. The project contains a **native project-owned `testDefinition`**, an embedded `RigolTestSteps.mjs` module of handler exports, and a small shared `RigolTestStation` adapter in its Panel operator JavaScript. The runtime's C++ Test Executive owns order, delays, polling, numeric inclusive comparisons, PASS/FAIL, Setup/Main/Cleanup, and JSON/CSV report generation. **Do not recreate/configure the test with `TestRunner.add`, `configure`, or `loadDefinition` from JavaScript.** Edit it using **Panel Designer → Test Sequence**.

The new Page has Run, Stop + Cleanup, Reset, native Run Information fields, Test Sequence, Measurement Table, Event Log, and JSON/CSV export controls. These are the existing native widgets/APIs.

## Physical wiring and safety

**Before connecting cables or running anything, switch both controlled CH1 outputs OFF on their instrument front panels.** This project issues real LAN SCPI output commands even if WFScript's *AD3 Simulation* banner is visible. It is NOT a hardware interlock or a general-purpose device-safety controller.

1. Connect **DP932A CH1 +** to **DM858 V input** and **DP932A CH1 −** to **DM858 COM**. Select the DMM voltage terminals and an appropriate DC voltage measurement configuration. No external load is required for this high-impedance measurement test. **Never put the multimeter in current mode with this wiring.**
2. Connect **DG902 Pro CH1 output** to **MHO954 CH1 input** through an appropriate coax cable. The Panel configures the scope CH1 for **1 MΩ**, **DC coupling**, and **1× probe**, and the generator for **High-Z output-load calibration**. Do **not** connect the PSU output to the generator output or short two driven outputs together.
3. Check the instruments' earth/return arrangements, voltage and connector ratings, and make sure no device under test or other electronics are attached to either loopback. Verify all cables before energizing the instruments.
4. Power supply test current limit: **0.20 A**; OVP: **5.50 V** enabled; OCP: **0.25 A** enabled. CH1 will be energized up to **5 V**. Generator signals are **zero offset**, with **1 or 2 Vpp** at **1 or 5 kHz**. Do not connect sensitive circuitry to these outputs during the test.
5. **STOP + CLEANUP requests software cleanup but is not an emergency stop.** Closing the Panel or losing TCP cannot guarantee physical outputs OFF. If a report or status says OFF is unconfirmed, switch both CH1 outputs OFF manually at the instruments and verify. Do not leave the bench unattended while a test is running.

## First Windows visual/runtime check (before live hardware)

1. Open the standalone `Rigol_Four_Instrument_Operator_Bench_v0_6_5_Automated_Tests.wfpanel` with **WFScript RC1.5.5**. Check that all five pages open and the first four resemble v0.6.4 without widget changes.
2. Select **05 Automated Tests**. Make sure all 17 new widgets are visible, especially the scrollable native Run Information form, Test Sequence, Measurement Table, Event Log, and buttons. Enter a Fixture/UUT ID and operator ID as desired.
3. Do **not** connect instruments yet. Press RUN. It must **refuse** to start without the wiring acknowledgement / all four instrument connections. This test must send **no output commands**.
4. Connect all four Rigol instruments using the ordinary **CONNECT** button on their original Pages. Stop the optional DM858 monitor timer. Set DG902 CH1 and DP932A CH1 to physically OFF using their own front panels. Leave CH2/CH3 outputs unchanged.
5. Review the physical cable arrangement and click **WIRING CONFIRMED**; this acknowledgement is a manual checklist, **not** electrical wiring detection. Press **RUN AUTOMATED TEST** once.

## Expected sequence

The Test Executive runs **62 steps** including **50 numeric comparisons**:

- Setup: verifies that all four devices are connected and idle, both controlled CH1 outputs read back **OFF**, scope CH1 responds, and DM858 is configured to **DCV / 10 V / 5 PLC**. It refuses to commandeer an initially ON output.
- PSU: sets DP932A CH1 to **5.00 V** with the limits above, then verifies the PSU's **measured output voltage**. It reads the same voltage with the independent DM858. Repeats PSU readback and DMM measurement for **4.75, 4.50, …, 2.00 V**, inclusive (13 target setpoints, 26 numeric comparisons). PSU CH1 is then commanded OFF and physically read back as OFF before moving to waveforms.
- Generator and scope: for **sine and square**, each at **1 and 2 Vpp**, each at **1 and 5 kHz** (8 conditions), it turns generator CH1 OFF, configures the signal with **High-Z calibration**, then enables CH1 and checks actual output state. It configures scope CH1 and its timebase, runs a fresh scope acquisition, waits briefly, stops the scope, and downloads the physically measured **1000-point NORMAL CH1 waveform**. It computes **Vpp, frequency (hysteretic rising edges), AC RMS/Vpp**, and checks **three native numeric limits per condition** (24 comparisons).
- Cleanup (even after a step error or STOP + CLEANUP): attempts **generator CH1 OFF + readback**, then **PSU CH1 OFF + readback** in separate steps. If a device is disconnected or busy for too long, it reports failure; it never claims a physical OFF that was not verified.

The default test uses **`stopOnFailure=false`** so out-of-tolerance numeric measurements are reported while later measurements can still proceed. Execution/transport errors, timeouts, and an operator STOP transition to cleanup. The overall deadline is 480 seconds, with step timeouts recorded in the project-owned definition.

## Acceptance limits — deliberately practical preliminary defaults

| Measurement | Inclusive acceptance |
| --- | --- |
| PSU CH1 voltage readback | Setpoint ±0.10 V |
| DM858 DC voltage | Setpoint ±0.075 V |
| MHO954 CH1 Vpp | Programmed amplitude ±15% |
| MHO954 CH1 frequency | Programmed frequency ±3% |
| Sine AC RMS/Vpp | 0.32 through 0.40 (ideal sine ≈0.354) |
| Square AC RMS/Vpp | 0.43 through 0.55 (ideal square ≈0.500) |

These values are **initial engineering test limits, not the manufacturers' calibration accuracy specifications.** The shape test checks basic waveform class, not formal sine THD, square duty cycle, or rise time. Set the acceptance windows appropriately for real hardware and probe/load conditions in the Visual Test Sequence Editor.

The Panel does not reset the scope timebase/trigger/channel setup after normal testing, and the DMM remains configured for DCV. The PSU and generator retain their last programmed setpoints but outputs are commanded OFF with readback. Other channels are not intentionally modified.

## Reports and results

The **Measurement Table** gives a PASS/FAIL result for each of the **50 physical numerical checks**, including acceptance limits and the actual readings. The **Test Sequence** shows step statuses, skipped steps after hard errors, and cleanup outcome; the **Event Log** shows execution history. Reports include execution ID, times, definition hash, run information, step statuses, numeric results, and five `variables` fields identifying live Rigol SCPI transport and endpoints. **Note:** the generic `report.instrument.mode` field currently refers to the AD3 runtime, so it may say *simulation* even when these independently connected Rigol TCP instruments are physical; use `variables.rigolMode` and the recorded endpoint fields to identify this test hardware.

After cleanup the Test Executive **automatically archives a JSON record** to its configured/default test-history directory. The **EXPORT JSON** and **EXPORT CSV** buttons write separate copies to `Documents/WFScript Reports`, using the native `TestRunner.exportReport()` API. A FAILED or ERROR result is saved as a real result; nothing is artificially substituted with a simulated passing measurement.

### Suggested real-hardware validation order

1. Confirm all four connections and physical wiring; verify preflight refusal if one CH1 output is initially ON. Verify no output-state change on refusal.
2. Execute a complete test and compare the DP932A CH1, DM858, DG902 CH1, and MHO954 CH1 front-panel readings against the report.
3. Stop during the PSU sweep, then during a waveform test. Verify outputs physically OFF after cleanup and check report statuses.
4. Test an induced failure (for example, change one numeric acceptance limit in the Test Sequence Editor) and confirm that FAIL is recorded while cleanup executes; restore limits afterward.
5. Verify archived JSON/CSV, the initial four Rigol operator Pages, and that a new run does not inherit stale measurements or results.

**Scope limitation:** CH1 waveform measurements are based on 1000-point stopped NORMAL captures, not 1-million-sample RAW. This specifically tests the native Test Executive integration; it does not replace the pending physical multi-channel 1M RAW regression plan.
