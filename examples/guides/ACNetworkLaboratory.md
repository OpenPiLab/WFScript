# AC Network Laboratory — interactive simulation to physical AD3 measurements

## Learning objective

Build, understand, and experimentally characterize **first-order passive RC low-pass and high-pass filters**. Compare theoretical complex transfer functions to real two-channel captures, document discrepancies, and produce reproducible results.

This example is a **ten-Page self-contained `.wfpanel`**. It is more than a drawing: it includes dynamic mathematical simulations, two illustrated breadboard maps, an explicit hardware preflight and W1 enable, single-shot and bounded multi-frequency AC gain/phase captures, comparisons against theory, CSV/JSON reports, lossless compression, and open-source JavaScript modules.

## Parts, hardware and limits

- Analog Discovery 3 with flywire harness and USB connection, computer with WFScript.
- Solderless breadboard and three short hookup leads (plus AD3 flywires).
- **1 kΩ resistor** (typical 1/4 W) and **100 nF non-polarized ceramic capacitor**.
- The illustrated physical sample has **R = 1000 Ω, C = 100 nF, fC ≈ 1591.55 Hz, τ = 100 μs**.
- No AD3 V+/V− power supplies or external powered circuitry are required.
- Conservative generated sine signal: **0.25–1.00 V peak**, **0 V offset**, **100 Hz–20 kHz**. Do not increase until you understand the circuit and AD3 limits.
- For physical operation the Panel rejects R below **900 Ω** (accepting measured values of nominal 1 kΩ parts, such as 980 Ω). Software R/C values must match installed component values. **The software cannot sense resistor/capacitor values or wire placement.**

Official Digilent AD3 getting-started guide: <https://files.digilent.com/manuals/WaveForms/3.24.4/start10.html>. The AD3 has two differential Scope inputs and W1/W2 analog waveform outputs. The Panel uses W1 and CH1/CH2 only.

## Build the low-pass circuit (Page 02)

Read the labeled diagram **on Page 02**. W1 OFF before constructing anything.

1. Use the breadboard's left A–E group, where holes on **one numbered row** connect together. Different rows are isolated. Verify the board's actual hole pattern.
2. Place the **1 kΩ resistor from C7 to C13** (in series, connecting VIN row 7 to VOUT row 13).
3. Place the **100 nF ceramic capacitor from D13 to D20** (VOUT to GND). It is non-polarized.
4. **W1 → A7; CH1+ → B7; CH2+ → B13.**
5. **W1 GND → A20; CH1− → B20; CH2− → C20** (same breadboard ground row).
6. Inspect carefully for accidental shorts/bridges. Scope channels are differential, but here their negative inputs share the intended GND reference.

For **high-pass** on Page 03, turn W1 OFF, disconnect the old series/shunt components, and **swap their positions**: 100 nF capacitor **C7 → C13**; 1 kΩ resistor **D13 → D20**. The AD3 leads remain the same. Select **High-pass** in the model before enabling physical output again.

**Never** connect W1 to AD3 supply pins, an externally powered breadboard, mains, or a digital input. The flywire hookup and common reference are specific to the default AD3 harness: do not assume BNC adapter or third-party harness wiring is identical. Use this in a supervised low-voltage lab.

## Follow along

1. **Page 00 — START/MISSION:** Read the equipment list, stages, and safety/provenance notes. Use Simulation mode initially.
2. **Page 01 — THEORY + SIMULATE:** Try Low-pass, 1000 Ω, 100 nF, and 1600 Hz. The model plots magnitude, phase, and time-domain input/output waveforms. Switch to High-pass and notice the sign of phase. The data is not collected by an AD3.
3. Press **ADD MODEL SWEEP TO NOTEBOOK** to record theoretical frequency-response rows as clearly labeled MODEL values.
4. **Page 02 or 03 — BUILD:** Follow the component and flywire diagram for the selected topology. Make electrical connections with W1 OFF.
5. In WFScript choose **Device → Physical Device** *before* running the Panel. Select the actual AD3 hardware and check the circuit again. On **Page 04**, toggle **WIRING VERIFIED** (an operator declaration, not an electrical interlock).
6. Click **ARM W1** to explicitly enable the generated low-amplitude sine; if arming is rejected, follow the reported error. Choose a frequency and press **CAPTURE CH1 + CH2**. The Panel must use actual Scope CH1/CH2 sample arrays to create AD3 rows.
7. Press **RUN AD3 FREQUENCY SWEEP** only after confirming that single capture works. The Panel changes W1 frequency one at a time, allows a settling delay, samples both Scope channels, computes a gain/phase pair, and **disables W1 automatically** at the end. The operator can press **STOP W1 / ABORT** at any time.
8. **Page 05 — NOTEBOOK:** Review the overlay plots: purple line is theoretical and green points are physical AD3 measurements. Table rows retain source, topology, component values, frequency, predictions, observed response, and error.
9. **Pages 07–09 — LEARN THE FIT:** Derive the linear least-squares equations, follow the JavaScript source step by step, and experiment with synthetic fractional-cycle, noise and harmonic data. Use the Real AD3 button only after a physical capture to inspect measured fit quality. More detail: `examples/guides/ACNetworkLeastSquares.md`.
10. Export **CSV + JSON**. Optional actions export the last physical capture's raw paired samples or compress its data with **Pako + base64-js**, then verify lossless recovery. Output is written to **FileIO.dataDirectory()**, i.e. the WFScript application data folder, not automatically next to the `.wfpanel`.
11. **Page 06 — CHALLENGES:** Complete the conceptual questions and include waveform captures, wiring photo, raw table, Bode plots and discussion of uncertainty in a lab report.

## Mathematical theory

For C in farads and f in hertz:

\[
\tau=RC, \qquad f_c=\frac{1}{2\pi RC}
\]

Low-pass, VOUT across capacitor:

\[
H_{LP}(j\omega)=\frac{1}{1+j\omega RC}
\]

High-pass, VOUT across resistor:

\[
H_{HP}(j\omega)=\frac{j\omega RC}{1+j\omega RC}
\]

Voltage gain is \(|H|=V_{OUT,peak}/V_{IN,peak}\), logarithmic gain is \(20\log_{10}|H|\) dB, and phase is the output phasor angle minus the input phasor angle. At the corner frequency, both networks have magnitude \(1/\sqrt{2}\) (−3.0103 dB); phase is −45° for the low-pass and +45° for the high-pass.

The theoretical plots assume ideal components and a zero-impedance source. Actual observations may differ because of component tolerance, stray capacitance, oscilloscope loading, amplitude/phase noise, and limited sample duration.

## How physical capture differs from theory

The native bridge obtains the last completed Scope CH1 and Scope CH2 sample arrays **from one acquisition**. `CaptureAnalysis.mjs` independently fits `offset + a·sin(2πft) + b·cos(2πft)` on each channel using the **actual acquisition sampling rate** and the commanded sine frequency. It derives gain from the ratio of fitted peaks and phase from the difference in fitted angles. This is more stable than seeking individual zero crossings in noisy, non-integer-cycle captures.

It is still educational metrology. We do **not** independently verify the actual generator frequency or calibrate the AD3 channels. The method assumes a near-sinusoidal steady-state input with adequate input amplitude and output SNR. The script rejects insufficient amplitude, implausibly high passive gain, nonfinite captures, and many acquisition failures. If the user incorrectly connects the probes, the software cannot guarantee the connected circuit is safe or that the result is meaningful.

## Measurement Notebook capacity (v0.2-dev.8.9.1 and later)

The Notebook on Page 05 now displays **11 columns** (Source, Topology, R, C, frequency, predicted gain, measured/model gain, gain in dB, predicted phase, observed phase, and gain error). It retains up to **500 experiment records** within one run; the general Experiment Data Table supports up to 32 columns and 2,000 rows. The corrected Panel requires a **v0.2-dev.8.9.1 or later executable** to load; v8.8 and v8.9 have an 8-column hard limit. Exports still preserve the complete measurement data and device/source provenance.

## Importable JavaScript libraries

Embedded source modules (portable inside `.wfpanel`):

- `CircuitLab.mjs` — project-authored ideal RC network theory and Bode/time synthesis; uses **Fraction.js** for exact rational sample grids.
- `CaptureAnalysis.mjs` — project-authored measured two-channel sine fitting and error summaries.
- `SignalPackage.mjs` — project-authored archival adapter invoking upstream **Pako** and **base64-js**, and upstream **Fraction.js**.
- `vendor/fraction.mjs`, `vendor/pako.mjs`, `vendor/base64.mjs` — unchanged third-party algorithms already validated by WFScript DSP Studio. Their full required license notices are embedded in their module headers.

`Pako`/`base64-js` are used only on request, after an actual physical capture; ordinary model plots do not call hardware or write files. No network fetch or npm installation is required to open the Panel.

## Make one small change

In Simulation mode, change R from 1000 Ω to 2200 Ω. The theoretical cutoff should drop from about 1591.5 Hz to about 723.4 Hz. Move the test frequency around the new corner and observe the Bode plots and phase. The **physical breadboard** must also use a 2.2 kΩ resistor if you later perform a comparable hardware measurement.

## Safety and scope

The Panel explicitly guards W1 arming behind a physical-device check and a user-operated wiring confirmation; arming uses a bounded sine with 0 V DC offset. It stops on AD3 errors, operator STOP, sweep completion, and `onShutdown()`; the native PanelRuntime cleanup is an additional backstop. This is not a hard safety interlock. For circuits other than the supplied passive RC filters, verify voltage/current/current-source limits independently.

## If something doesn't work

- **No response in Simulation:** Run the Panel, select the THEORY + SIMULATE Page, and inspect `measurementStatus`/Panel Console for errors. The model can run with no device.
- **W1 won't ARM:** Select a Physical Device before Run Panel, verify wiring, and flip WIRING VERIFIED. The entered R must be at least 900 Ω (a measured nominal 1 kΩ part is acceptable); the software's component values must match physical parts.
- **No signal:** Check W1 ↔ row 7, common GND ↔ row 20, CH1+ ↔ row 7, CH2+ ↔ row 13; use scope flywire pins, not DIO.
- **Physical gain or phase differs:** Confirm measured channel peaks are plausible and output is connected to intended node. High-pass should be positive phase below cutoff, low-pass negative phase above cutoff. Check R/C tolerance and breadboard parasitics.
- **W1 stays on after a failure:** Press STOP W1, then Stop/Design and disconnect the breadboard if safe. Do not assume a software stop alone is a safety-rated power disconnect.
- **Where did exports go?** View the Directory shown on Page 05. The application uses its writable PanelData area; files do not become part of the `.wfpanel` project automatically.

## Related files

- `examples/panels/ACNetworkLaboratory.wfpanel` — complete self-contained project.
- `examples/source/ACNetworkLaboratory.js` — readable source embedded into the project.
- `examples/libraries/CircuitLab.mjs` and `CaptureAnalysis.mjs` — numerical modules.
- `tests/offline/ac_network_lab_mock.mjs` and `ac_network_modules.mjs` — deterministic simulated and numerical tests.
- `tests/ac_network_lab_smoke.cpp` — Qt `QJSEngine`/Panel schema native smoke target (requires Qt6 build).

## v0.2-dev.8.9.2 — fitting lessons and residual diagnostics

The seven original Pages are preserved and three new Pages are appended. Page 07 derives the centered least-squares method for `d+a sin+b cos`; Page 08 maps every stage to the actual embedded `CaptureAnalysis.mjs` implementation; Page 09 plots raw and fitted waveforms plus per-channel residuals from reproducible synthetic samples or actual AD3 CH1/CH2 captures. The new diagnostics include residual RMS in volts, residual percent of the fitted peak and R² (a descriptive goodness-of-fit number, *not* calibrated accuracy). The on-screen Notebook still has 11 columns; CSV appends four diagnostic columns for physical rows and JSON includes their named values. The synthetic generator never controls W1 or adds a physical row. Full theory and exercises are in `ACNetworkLeastSquares.md`.
