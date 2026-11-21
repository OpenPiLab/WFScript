# WFScript Rigol v0.6.6 — Windows/real hardware validation

**Not yet Windows or hardware validated.** No native source changes; the new Panel is intended for RC1.5.5 or the pending RC1.5.6 candidate. RC1.5.6 widget responsiveness requires an actual RC1.5.6 executable. Use the original user-tested v0.6.5 Panel as the reference.

## Part A: offline / non-energized smoke

1. Open `Rigol_Four_Instrument_Operator_Bench_v0_6_6_Readable_Tests.wfpanel` in WFScript. Verify all five Pages open, and the fifth Page has no overlapping Event Log/footer.
2. In the Test Sequence Editor, verify **62 steps, 50 numerical measurements**, and the preserved limits and step order.
3. In the library browser, open `RigolTestSteps.mjs`; verify procedures `measureSupply`, `measureDmm`, `captureWaveform`, `safeRelease` are actually implemented there. No `RigolTestStation` forwarder should exist.
4. Press RUN without connected instruments/wiring confirmation: it must refuse without issuing output commands.
5. Check Run/Stop/Reset/export event bindings and that instrument pages show their previous controls.

## Part B: physical test — only with wiring verified

- DP932A CH1 +/- → DM858 voltage/COM input. **Never use DMM current jack** for this connection.
- DG902 PRO CH1 BNC → MHO954 CH1 BNC, separate from PSU wiring.
- Verify both output channels are physically OFF before starting. Verify grounding/probe limits, 1 MΩ scope input and high-Z generator setting.
- Connect all four instruments; stop DMM monitor. Only then check WIRING CONFIRMED.
- Execute one full sequence and compare with prior v0.6.5 result (62 steps, 50 measurements, about 23 s on the previously tested bench). No guarantee of exact duration.
- Verify physical generator/PSU OFF on their front panels and archive output contents.

## Part C: negative tests (only if safe)

- Before RUN, with a device already ON under operator control, preflight must refuse and **must not switch it off**.
- A safely edited test definition that attempts waveform capture before the PSU-OFF step should fail with an interlock; do not create unsafe electrical connections to test this.
- Normal STOP should proceed through both cleanup steps and require actual OFF readbacks. A process crash, unplugged Ethernet cable, or inaccessible instrument **cannot guarantee OFF**. Do not simulate such faults with hazardous live circuitry.

## Results / limitations

- Look for newly introduced module-import errors (e.g. missing `invoke`), early rejected tests, or wrong status in operator labels.
- The generic JSON report `instrument.mode` remains AD3-centric and may say `simulation` even for physical Rigol instruments. The `variables.rigolMode` and endpoint fields identify the actual SCPI hardware test. This limitation was **not fixed** here.
- The test still uses native v1 start/poll handlers, not Promise-aware native execution or parameterized steps.
- RC1.5.6 hidden-page visual refresh optimization remains a **separate unverified native change**, not part of the v0.6.6 Panel refactor.
