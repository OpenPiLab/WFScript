# WFScript — RC Step Response and Time Constant Laboratory

**Version:** v0.2-dev.8.9.3\
**Project:** `RCStepResponseLaboratory.wfpanel` — eleven Pages, 116 native visual controls, one embedded JavaScript module, one embedded circuit diagram.\
**Modes:** SYNTHETIC MODEL (does not touch hardware) and deliberately armed PHYSICAL AD3 (real two-channel Scope samples).\
**Prerequisite:** A working WFScript Qt 6 application with the WaveForms SDK and native Panel JavaScript APIs; a physical Digilent Analog Discovery 3 is optional until Page 04.\
**Important:** The generated model and the physical sample records are never substituted for each other. The panel is an educational exercise, not an independently calibrated instrument.

## Learning outcomes

By completing this lab, students should be able to:

1. Derive a first-order RC differential equation from KVL and the constitutive equations for a resistor and capacitor.
2. Solve the equation for an arbitrary initial capacitor voltage following a piecewise-constant input change.
3. Relate exponential charging and discharging to the definition of the time constant, including the 63.2% criterion and 5τ settling rule.
4. Predict the steady periodic response to a symmetric square wave without treating each half-cycle as starting from zero volts.
5. Use a physical AD3 to capture both the input step and capacitor response, locate each input edge, and isolate complete transient intervals.
6. Fit an exponential by least squares without a hidden numerical library, learn why the optimization is partly linear and partly nonlinear, and interpret its residuals.
7. Compare rising and falling edge time constants and understand what differences can reveal about noise, component behavior and measurement technique.
8. Determine a cutoff frequency **from a time-domain experiment**, then connect it to the frequency-domain AC Network Laboratory.
9. Preserve an evidence trail that distinguishes entered component values, synthetic calculations, and actual acquired voltages.

## Equipment, baseline components and circuit

- Digilent Analog Discovery 3 (optional for simulation), WaveForms SDK, WFScript v0.2-dev.8.9.2 or a compatible version that supports the existing instrument/visual/widget APIs.
- One non-polarized capacitor and one resistor, breadboard, AD3 flywires; an optional DMM/LCR meter.
- The actual components previously measured: **R = 980 Ω**, **C = 571 nF at 1 kHz**. These are entered parameters, not measurements made by this project.
- **Electrical connections:** W1 to VIN; R from VIN to VCAP; C from VCAP to common ground; CH1+ to VIN; CH2+ to VCAP; W1 GND, CH1−, CH2− to the same ground. **No external power supply** and no externally energized circuit.
- Physically use the low-pass topology (capacitor to ground, output across capacitor). Rewire only with W1 OFF. A generic high-pass RC circuit is *not* this lab's DUT.
- Real W1 uses **Square**, 0 V DC offset, user-commanded 0.20–0.50 V peak; the sample starting point is ± amplitude. Its 0.5 V peak default means 1 Vpp square wave, not 0.5 Vpp. CH1 and CH2 are set to the same 5 Vpp Scope range as in the earlier AC example.

A wiring-confirmation switch is only a checklist. It cannot detect incorrect or hazardous external wiring. Observe AD3 voltage/current limits and the physical circuit before energizing.

## Follow along — Page-by-Page

### 00 — Mission + roadmap

This Page shows what students will *derive*, *simulate*, *build*, *measure*, *fit*, and *report*. It contains the central identity

\[\tau=RC,\qquad f_c=\frac{1}{2\pi\tau}.\]

The two quantities are related because they arise from the **same differential equation**; one is a time-domain response measure and the other is a frequency-domain response measure.

### 01 — Derive the physical equation

Apply KVL to the series resistor and capacitor:

\[v_{in}(t)=v_R(t)+v_C(t).\]

Use Ohm's law \(v_R=Ri\) and the capacitor law \(i=C\frac{dv_C}{dt}\):

\[RC\frac{dv_C}{dt}+v_C=v_{in}(t).\]

After the input changes to a constant voltage \(V_\infty\), define \(u(t)=v_C(t)-V_\infty\). The ODE reduces to \(du/dt=-u/(RC)\). Separating the variables:

\[\frac{du}{u}=-\frac{dt}{RC}\Rightarrow \ln|u|=-\frac{t}{RC}+K.\]

Exponentiate, and use the initial condition \(u(0)=V_0-V_\infty\):

\[\boxed{v_C(t)=V_\infty+(V_0-V_\infty)e^{-t/\tau},\quad\tau=RC}.\]

The formula works for **rising and falling** steps without changing its algebra. At \(t=0\), the capacitor cannot jump instantaneously. As \(t\rightarrow\infty\), the resistor current \(i=(v_{in}-v_C)/R\) tends toward zero.

### 02 — Build the physical circuit

Use the fully embedded labeled schematic; do not assume that a sine-wave schematic tells you the requested W1 setting. In this experiment W1 is explicitly configured to **Square**. Check shared ground, the resistor series connection, and the capacitor to ground. There are no digital I/O connections.

The capacitor may be non-polarized ceramic or film. A polarized capacitor should not be used with the laboratory's symmetric negative/positive square-wave stimulus.

### 03 — Synthetic experiment, no physical device

The Page starts from the user's measured component values: R=980 Ω, C=571 nF, 100 Hz square wave, 0.5 V peak, and 0.4 mV RMS deterministic synthetic noise.

The model uses the **steady periodic solution** to avoid generating a made-up transient from 0 V at every edge. If the half-period is \(T_h\), let \(q=e^{-T_h/\tau}\). For a ±V square wave, the capacitor voltage immediately after a rising edge in periodic steady state is:

\[V_{up,start}=-V\frac{1-q}{1+q}.\]

The corresponding falling-edge starting value is \(V_{down,start}=+V(1-q)/(1+q)\). After each transition the voltage follows the exact first-order expression from Page 01. A deterministic pseudo-random generator optionally adds known, synthetic zero-mean noise for an educational exercise. It is not a noise model of the AD3 itself.

Click **RUN SYNTHETIC EXPERIMENT** and inspect how the input square wave differs from the capacitor output. Try raising the synthetic noise from 0 to 1, 5 and 10 mV RMS. Then vary R or C, observe the slow/fast response, and compare with the theoretical τ and fc readouts.

**Do not label any values on this Page as instrument measurements.** The Page's plots always name the source, either SYNTHETIC MODEL ONLY or PHYSICAL AD3, and its result rows retain that source.

### 04 — Actual two-channel AD3 measurement

Start with the simulation. To measure hardware, connect the circuit as shown, select an actual **Physical Device**, and enable `stepWiringVerified`. For the focused hardware exercise the code enforces:

| Parameter | Allowed physical acquisition |
|---|---|
| Resistor | 900 Ω or higher |
| RC time constant | 70 µs to 1200 µs |
| Square-wave frequency | 60 Hz to 150 Hz |
| Half-cycle settling | At least 6τ for every half period |
| Commanded W1 amplitude | 0.20 to 0.50 V peak, zero offset |
| Sample acquisition | 8192 paired samples, 250 kS/s requested; actual rate read back |

The default 980 Ω / 571 nF circuit yields \(\tau=559.58~\mu s\); the 100 Hz square wave has a 5 ms half-period, or ~8.94τ. A 5τ transient lasts ~2.798 ms. At the requested sampling rate this gives ~140 samples per time constant.

Click **CAPTURE PHYSICAL CH1 + CH2**. A single explicit click configures W1 with the safe values, checks native W1 function readback when available, waits 300 ms for periodic settling, captures two actual channels, and automatically requests W1 OFF whether acquisition succeeds or fails. The Scope is configured with **Trigger=None** and the code finds actual CH1 edges in the recorded waveform; no hardware-trigger synchronization is assumed.

Unlike the AC Lab's frequency sweep, this first revision performs **one physical capture per click**. It is deliberately simple and avoids new native instrument bridge changes.

Click **STOP / ABORT W1 NOW** at any time. Changes to R, C, or square-wave frequency revoke the previous wiring confirmation. If a stop call returns false or throws, the status explicitly warns rather than claiming the output is definitely off. The app cannot guarantee interruption of a blocking vendor SDK call; do not treat this as a safety stop.

### 05 — Fitted waveform and residuals

The Page shows a trace of one complete CH2 transition, the estimated exponential and the **residual** (measured sample minus reconstructed fitted sample). Other readouts include:

- Mean τ across all complete transitions, and separate RISE and FALL means.
- The 63.2% threshold method as a cross-check.
- Cutoff frequency derived from \(f_c=1/(2\pi\tau_{fit})\).
- Pooled RMS residual, normalized to the measured CH1 input swing; this is **not** ADC resolution or calibrated accuracy.
- The percentage disagreement between fitted τ and the theoretical τ calculated from **operator-entered** R,C.
- A fitted voltage plateau \(V_\infty\), coefficient A, and an \(R^2\) estimate with eight digits.

A residual of a few hundred microvolts may be consistent with excellent model agreement, but a small residual alone does not verify the absolute accuracy of the instrument or any input component values.

### 06 — The 63.2% graphical method

From the exponential equation, at \(t=\tau\) the remaining distance to the final voltage is \(e^{-1}=36.7879\%\), so the capacitor has completed \(1-e^{-1}=63.2121\%\) of its change. Students can locate this level on CH2 and calculate elapsed time since the CH1 input edge.

**Implementation detail that matters:** the initial amplitude and asymptote used for this threshold are obtained from the fitted curve. To correct the fact that the numerical fitting interval begins three samples after the CH1 transition, the fitted amplitude is extrapolated back to the CH1 edge time. The first measured threshold crossing is linearly interpolated between two CH2 samples. Thus this is a useful graphical cross-check, **not a statistically independent estimate** of the fit parameters.

At \(5\tau\), \(1-e^{-5}=99.3262\%\) of the voltage movement is complete. Emphasize that neither "63.2% of the final voltage" nor "63.2% of a maximum amplitude" is universally correct: the criterion is 63.2% **of the voltage difference between the initial and final levels**.

### 07 — Least-squares exponential fitting derivation

For sampled voltage after an input edge, with \(t_i=(i-i_0)/f_s\), use the model

\[y_i=V_\infty + A e^{-t_i/\tau}+\varepsilon_i.\]

The time constant τ is inside an exponential so solving all three unknowns simultaneously is a nonlinear regression problem. But **for any fixed trial τ**, define \(E_i=e^{-t_i/\tau}\), and the model becomes linear in \(V_\infty\) and A:

\[y_i=V_\infty+A E_i+\varepsilon_i.\]

Minimize the squared error \(SSE=\sum_i(y_i-V_\infty-AE_i)^2\). Taking partial derivatives with respect to \(V_\infty\) and A and setting them to zero yields

\[N V_\infty+A\sum E_i=\sum y_i,\]

\[V_\infty\sum E_i+A\sum E_i^2=\sum E_i y_i.\]

Eliminating \(V_\infty\):

\[A=\frac{\sum E_i y_i-\frac{\sum E_i\sum y_i}{N}}{\sum E_i^2-\frac{(\sum E_i)^2}{N}},\qquad V_\infty=\frac{\sum y_i-A\sum E_i}{N}.\]

The implementation first tests 57 logarithmically spaced candidate τ values and then uses 42 **golden-section minimization** steps over the best neighboring interval in \(\ln\tau\). Each trial solves this two-variable linear least-squares regression exactly. This pedagogical algorithm is called *variable projection*: eliminate the linear unknowns and search only over nonlinear parameters.

The residual, RMS and \(R^2\) are then computed in the original voltage units, not from logarithms of measured voltages.

#### Why not take a logarithm?

For a known, exact final value, \(\ln|y_i-V_\infty|\) is theoretically linear in time. But the final voltage is not perfectly known from a real finite-duration capture, and logarithms greatly amplify measurement errors as voltage approaches the asymptote. Least squares on the **original sampled voltage** is more straightforward to interpret.

#### How the first and last samples are selected

- Sort CH1 voltage samples and estimate low/high plateaus from the 10th/90th percentiles.
- The CH1 midpoint gives a threshold crossing time; linear interpolation improves sub-sample timing.
- Ignore crossings too close to the previous transition and reject a weak input swing below 0.15 V.
- For each adjacent pair of opposite-direction CH1 edges, exclude three samples at either boundary, rejecting implausible short/long half-cycles.
- Fit CH2 samples only over these **complete half-cycles** so a missing boundary is not mistaken for a full exponential.
- Require both a complete RISE and a complete FALL, a sufficient exponential amplitude, and sensible polarity relative to CH1.

### 08 — The actual code, step by step

The file `examples/libraries/RCStepAnalysis.mjs` is embedded as `RCStepAnalysis.mjs` in the project's **Libraries** editor, so saving the `.wfpanel` preserves the mathematical implementation. The Panel source `RCStepResponseLaboratory.js` handles UI and physical instruments; the module is plain JavaScript without SDK calls.

| Panel teaching step | Function / expressions | Mathematical meaning |
|---|---|---|
| Find the steps | `edgesFromInput()` | Observed CH1 edge times |
| Isolate windows | `analyzeCapture()` | Select CH2 samples from complete half-cycles |
| Build a candidate exponential | `Math.exp(-(i-first)/(rateHz*tau))` | \(e^{-t_i/\tau}\) |
| Solve two coefficients | `solveAtTau()` | Compute \(V_\infty\), A and SSE at a fixed τ |
| Find best τ | `fitTransition()` | Coarse log search and golden-section refinement |
| Check independent edge directions | `rises`, `falls`, `transitions` | Average RISE and FALL τ and compare |
| Plot and record | `showCapture()`, `addResult()` | Distinct MODEL/AD3 trace and provenance |

**Source experiment:** Change only the synthetic noise slider first and inspect `residualRmsV`. As a second task, inspect the code for how increasing the number of candidate τ samples affects computational effort and accuracy. Keep hardware commands unchanged until the simulation is understood.

### 09 — Notebook and exports

The 12-column read-only Notebook stores synthetic and physically acquired summaries using their explicit source labels. It does not turn generated voltages into hardware data. Click **EXPORT CSV + JSON** to save a timestamped session under the common PanelData directory. A second export in the same second receives an additional sequence number.

The exported JSON includes algorithm result summaries and per-edge fit information; it does **not** duplicate the full 8192-point arrays. **EXPORT LAST REAL AD3 WAVEFORM** separately exports the physical CH1 and CH2 voltages together with their sample times. It refuses to export synthetic data as an AD3 waveform. The in-memory Notebook clears on runtime restart; existing files remain in shared PanelData unless the operator intentionally deletes them.

### 10 — Connect to the AC Network Laboratory

The previous physical experiment used a sinusoidal drive to estimate gain and phase near the cutoff. For R=980 Ω and C=571 nF, the component-derived predictions are

\[\boxed{\tau=980\cdot571\times10^{-9}=559.58\ \mu\mathrm{s}}\]

\[\boxed{f_c=\frac{1}{2\pi\cdot559.58\times10^{-6}}\approx284.42\ \mathrm{Hz}}.\]

Earlier WFScript and WaveForms sweeps measured a cutoff around 284–285 Hz. The **new physical step measurement has not yet been validated on a connected AD3**; that experiment is the next validation task, and agreement must not be claimed in advance.

Suggested comparison: divide a new time-domain measured cutoff from Page 05 by the earlier AC fitted cutoff. If those independently computed numbers differ, investigate finite square-wave rise time, sampling and scope range, component tolerance, capacitor frequency behavior, stray capacitance, contact resistance, and the fitting model.

## Student challenge questions

1. What physical property makes capacitor voltage continuous when a voltage step is applied through a resistor? What happens to the resistor voltage instantly after the step?
2. Starting from \(RC\, dv_C/dt+v_C=V_\infty\), derive the general exponential response without memorizing a formula.
3. With R = 980 Ω and C = 571 nF, calculate τ, 5τ, the fraction settled at 1τ, and the cutoff frequency.
4. Why should the physical square-wave half-period exceed 6τ for this experiment? What happens when the capacitor never reaches its nominal plateau?
5. Why can fitting an exponential with an unknown final voltage be more robust than fitting \(\ln(V_\infty-v(t))\)?
6. What does a high \(R^2\) or a low residual RMS actually establish—and what does it not establish?
7. Which is more stable under simulated noise: one threshold crossing or hundreds of least-squares samples? Why?
8. How would a finite W1 edge rise time distort the early part of the capacitor response?
9. What physical error could make τrise and τfall measurably different?
10. After the actual AD3 measurement, compare its \(f_c\) from fitted τ to the earlier AC sine-wave cutoff. Is the comparison independent of the AD3 hardware and calibration?

## Make one small change

In the simulation, set the noise parameter to 0 and run one synthetic measurement; record fitted τ and RMS residual. Repeat at 1 mV, 5 mV and 10 mV noise, without changing R or C. Explain why τ may remain stable even as the residual increases. Then inspect `Step.solveAtTau` and connect its `sumE`, `sumEE`, `sumY`, and `sumEY` to the normal equations above. This exercise requires no hardware and does not modify a single native C++ file.

## Safety and scope

- The square-wave functionality is supported by the existing Wavegen1 script API; actual AD3 square-wave capture and the new exponential-fitting integration still require physical user verification.
- The applied W1 output is limited in script to ±0.50 V peak (1 Vpp), no offset; changing this code removes that guarantee.
- This code requests W1 OFF after each physical capture and on errors; an SDK stop failure or a blocking API cannot be made safe by JavaScript alone.
- Scope CH1 and CH2 read a common time base; the input edge time is detected from **CH1 measured samples**, not assumed from a W1 software command.
- The fit estimates a first-order model. A real capacitor may have equivalent series resistance, leakage, frequency dependence, dielectric absorption, or other nonideal effects.
- Source labels distinguish MODEL from AD3. R and C are operator-entered component measurements; only the physical Scope samples are acquired by the AD3.
- File exports are explicit and go to PanelData, not inside the `.wfpanel`; a self-contained panel is not the same as an automatically persisted dataset.

## Instructor extension ideas

Ask students to compare their RC circuit with a 1-kΩ/100-nF circuit, measure response over several square frequencies, investigate R/C component tolerance, plot normal and logarithmic residuals for model diagnostics (do not fit logarithms by default), or implement an alternative nonlinear optimizer and compare accuracy/complexity. For advanced analysis, propagate resistor and capacitor measurement uncertainty into τ and fc, remembering that a fit's residual does **not** by itself describe those uncertainties.
