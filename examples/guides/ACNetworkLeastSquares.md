# WFScript AC Laboratory — From physics to JavaScript least-squares measurement

**Applies to WFScript v0.2-dev.8.9.2.** The embedded `.wfpanel` includes Pages 07 (derivation), 08 (annotated code) and 09 (synthetic / real fit explorer). This companion explanation expands the lessons. The calculation is visible in `examples/libraries/CaptureAnalysis.mjs`, embedded byte-for-byte in the `.wfpanel`.

## 1. Why estimate a sine wave instead of reading one peak?

A real Scope records discrete voltages. Even a clean RC filter produces signals with offsets, quantization, interference, harmonic distortion and some noise. A single maximum depends on where the samples land relative to the waveform peak. A zero crossing is similarly sensitive to offsets and noise. Instead, use every sample to estimate the single-frequency sinusoid that best explains the capture.

The AD3 generates a nominal sine at a **commanded** frequency `f`. The existing `acquireOne(f)` in `ACNetworkLaboratory.js` captures both channels during one Scope acquisition and reads back the **actual Scope sampling rate** as `actualRate`. The fitting routine treats `f` as known: **it does not independently measure the generator frequency**. It also assumes one sample time base for both channels. We fit CH1 and CH2 separately, so each can have an independent offset, amplitude and phase.

For sample index `i`, `t_i=i/f_s` and `θ_i=2π f i/f_s`. Write

\[
y_i=d+a\sin\theta_i+b\cos\theta_i+e_i.
\]

Unknowns: `d` = DC offset (V), `a` = sine coefficient (V), `b` = cosine coefficient (V), `e_i` = residual (V). This is **linear in the three unknown coefficients** even though the waveform is periodic in time. There is no iterative nonlinear optimizer and no need to detect an integer number of periods.

## 2. Why do sine and cosine encode phase?

Using the angle-addition identity,

\[
A\sin(\theta+\phi)=A\cos\phi\sin\theta+A\sin\phi\cos\theta.
\]

Therefore `a = A cos φ` and `b = A sin φ`; after fitting,

\[
\boxed{A=\sqrt{a^2+b^2},\quad \phi=\operatorname{atan2}(b,a)}.
\]

**Warning about argument order:** our model is *sine plus cosine*, so the phase is `atan2(cosCoef,sinCoef)`. This is a frequent source of mistakes. `atan2` also handles the correct quadrant, unlike a plain `atan` quotient.

A nonzero `d` changes neither `A` nor `φ` in the correctly fitted model.

## 3. Deriving least squares

Define `s_i = sin θ_i`, `c_i = cos θ_i`, and prediction `ŷ_i=d+a s_i+b c_i`. Minimize the sum of squared errors:

\[
S(d,a,b)=\sum_{i=0}^{N-1}(y_i-d-a s_i-b c_i)^2.
\]

The optimal coefficients satisfy `∂S/∂d = ∂S/∂a = ∂S/∂b = 0`. These three equations are equivalent to the normal equations

\[
\begin{bmatrix}
N&\sum s_i&\sum c_i\\
\sum s_i&\sum s_i^2&\sum s_i c_i\\
\sum c_i&\sum s_i c_i&\sum c_i^2
\end{bmatrix}
\begin{bmatrix}d\\a\\b\end{bmatrix}=
\begin{bmatrix}\sum y_i\\\sum s_i y_i\\\sum c_i y_i\end{bmatrix}.
\]

This is a linear-algebra problem: the three columns `[1,s,c]` form a design matrix `X`. We solve `(XᵀX) β = Xᵀy` with `β=[d,a,b]ᵀ`. The 3×3 matrix is never actually allocated; `fitSinusoid()` only accumulates the necessary sums.

### 3.1 Eliminate the offset

Let `s̄ = Σs_i/N`, `c̄ = Σc_i/N`, `ȳ = Σy_i/N`. Center both basis vectors and the data. Define

\[
S_{ss}=\sum(s_i-\bar s)^2,\quad S_{sc}=\sum(s_i-\bar s)(c_i-\bar c),\quad S_{cc}=\sum(c_i-\bar c)^2,
\]

\[
S_{sy}=\sum(s_i-\bar s)(y_i-\bar y),\quad S_{cy}=\sum(c_i-\bar c)(y_i-\bar y).
\]

Then solve only two equations:

\[
S_{ss}a+S_{sc}b=S_{sy},\qquad S_{sc}a+S_{cc}b=S_{cy}.
\]

With `D=Sss*Scc−Ssc²`,

\[
a=\frac{S_{sy}S_{cc}-S_{cy}S_{sc}}{D},\quad
b=\frac{S_{cy}S_{ss}-S_{sy}S_{sc}}{D},\quad
d=\bar y-a\bar s-b\bar c.
\]

This is **exactly** the technique implemented in the embedded JavaScript. A small `D` implies that the basis functions cannot be independently resolved for the supplied sample geometry; the code rejects degenerate fits. Normal equations are suitable for this tiny fixed-frequency basis in our bounded captures, but are not a universal numerical method for badly conditioned large least-squares problems.

## 4. Map mathematical objects to real code

Open `CaptureAnalysis.mjs` (or Page 08 in the Panel):

| Mathematics | JavaScript | Meaning |
|---|---|---|
| `θ` increment | `step=2*Math.PI*f/rate` | Angular step per actual sample |
| `Σs`, `Σc`, `Σs²`, `Σc²`, `Σsc` | `s0`, `c0`, `ss`, `cc`, `sc` | Basis Gram-matrix entries |
| `Σy`, `Σys`, `Σyc` | `y0`, `ys`, `yc` | Data and basis correlations |
| `Sss`, `Ssc`, `Scc` | `a`, `b`, `d` (locally **matrix entries**) | Centered 2×2 system; distinct from sine/cosine coefficients |
| `Ssy`, `Scy` | `rhsS`, `rhsC` | Centered right-hand side |
| `D` | `det` | 2×2 determinant |
| `a`, `b` (fitted coefficients) | `sinCoef`, `cosCoef` | The actual voltage coefficients |
| `d` (voltage offset) | `offset` | Recovered mean after solving |
| `A`, `φ` | `amplitude`, `angle` | Voltage peak and sine-convention phase |
| `ŷ_i` | `predictSample(fit,rate,f,i)` | Reconstructed sample, without changing raw data |

**Implementation note:** `a`, `b` and `d` inside the centered-equations code refer to matrix entries, *not* the physical coefficients with the same mathematical letters. The source uses `sinCoef`, `cosCoef`, `offset` for the actual fitted physical parameters. Students should trace this naming carefully.

The physical path still calls `analyzeTwoChannel()` as before. The v8.9.2 release adds `inputFit` and `outputFit` to its return value, so the student Pages can display coefficients and residuals without doing a second, different kind of fit. Older outputs (`gain`, `db`, `phaseDeg`, `inputVpk`, etc.) are preserved.

## 5. From two fitted channels to the RC transfer function

For each channel, let `(Ain, φin)` and `(Aout, φout)` be the fitted peak amplitude and phase. The measured complex transfer at the test frequency is represented by

\[
|H|=\frac{A_{out}}{A_{in}},\qquad G_{\mathrm{dB}}=20\log_{10}|H|,\qquad \phi_H=\phi_{out}-\phi_{in}.
\]

The code wraps the result into (−180°, 180°]. Both channel phases are referenced to the same acquisition origin; the reference phase cancels on subtraction. The *theoretical* first-order RC models in `CircuitLab.mjs` are

\[
H_{LP}(j\omega)=\frac{1}{1+j\omega RC},\qquad H_{HP}(j\omega)=\frac{j\omega RC}{1+j\omega RC}.
\]

These are predictions, not data. At `f_c=1/(2πRC)`, both have gain `1/√2` = −3.0103 dB and phase −45° (low pass) or +45° (high pass).

**Reference experiment, not built-in AD3 data:** our previously documented physical resistor was **980 Ω** and capacitance **571 nF at 1 kHz**. Their theoretical `f_c` is about **284.4 Hz**. The earlier WFScript sweep estimated about **284.7 Hz**, close to Digilent WaveForms. These historical observations are deliberately *not preloaded* as measured rows in this `.wfpanel`: actual AD3 rows must be acquired live.

The physical preflight now permits **R ≥ 900 Ω**, so a measured nominal 1 kΩ resistor such as 980 Ω can be entered directly, while retaining a conservative lower bound. W1 is still limited to 1 V peak, zero DC offset, and explicit wiring verification.

## 6. Diagnostics: what a good fit does and does not prove

With `ŷ_i = d+a sinθ_i+b cosθ_i`, define residual `e_i=y_i−ŷ_i` and

\[
\mathrm{RMS}_{res}=\sqrt{\frac{1}{N}\sum e_i^2},\qquad
\mathrm{Residual\%}=100\frac{\mathrm{RMS}_{res}}{A},\qquad
R^2=1-\frac{\sum e_i^2}{\sum(y_i-\bar y)^2}.
\]

The residual RMS is in volts. The percent uses fitted **peak** amplitude, not AC RMS, and is simply a scale-normalized goodness-of-fit number. `R²` is descriptive goodness of fit, not a calibration confidence or a guarantee of accurate gain and phase. A near-zero residual is possible even when the probes are swapped or their absolute gain is uncalibrated.

Real residuals can contain unmodeled harmonics, broadband noise, imperfect settling, offset drift, timing error, finite precision, and mismatch between the commanded and actual excitation frequency. The method estimates a sine *at the specified frequency*; it does not determine or correct sample-clock errors, instrument channel delay, or generator-frequency uncertainty.

## 7. Page 09 lab exercises

1. **Pure sine:** Set noise and 2nd harmonic both to zero. Keep `2.75` cycles. Observe practically zero residual, accurate gain/phase and non-integer-cycle support. Change to `1.25` or `7.50` cycles. Does the fit remain accurate?
2. **Noise:** Raise noise from `0%` to `5%` to `15%` peak, holding cycles constant. Observe increasing RMS residual and variations in gain and phase. Noise uses a deterministic pseudo-random generator so repeated runs with the same settings agree.
3. **Harmonics:** Reset noise to zero; increase second harmonic. What does the residual plot show? Why does a very good single-frequency fit not capture the waveform's harmonic shape?
4. **DC offset:** Read the `d` term of both fitted channels. Why are the offsets nonzero even with no noise?
5. **Real AD3:** On Page 04, perform a real two-channel capture, then click **SHOW LAST REAL AD3 CAPTURE** on Page 09. Compare physical waveform and fitted traces, residual RMS (mV), percent-of-peak, `R²`, gain, phase and the RC model. No wavegen action is triggered by Page 09 itself.
6. **Edit the source:** Change the deterministic synthetic signal's harmonic magnitude or the model's phase and explain the difference between a predicted physical response, the synthetic sample source and the fitted estimate.

The Page 09 plots decimate for rendering only. All fit coefficients and diagnostic statistics use **all** paired samples. CSV/JSON exports of physical rows now include the CH1/CH2 fit RMS residual and percent-of-peak; CSV columns were appended without changing the existing 11-column Notebook widget.

## 8. Limits, safety and repeatability

- No actual physical signal is created in Simulation/fit demo mode; opening the project does not start W1.
- The real fit explorer requires an actual prior Scope capture. A synthetic trace is always labeled `SYNTHETIC`; it is never added as an `AD3` Notebook record.
- Real measurements require explicit Device selection, operator wiring verification and W1 ARM. Emergency cleanup and the 1 V peak ceiling remain in place. The waveform must come from a low-voltage supervised breadboard circuit, not mains.
- The existing export filenames are fixed and can overwrite a previous export; rename or copy files before another export if you need multiple sessions. Timestamped sessions are a separate future task, not part of v8.9.2.
- The new fit diagnostics are educational and cannot replace traceable metrology, instrument calibration or a formal uncertainty budget.

## Files and testing

- `examples/libraries/CaptureAnalysis.mjs`: production least-squares fit, reconstruction, residual diagnostics.
- `examples/source/ACNetworkLaboratory.js`: acquisition unchanged except optional fit-page refresh; synthetic demo and UI plots added.
- `examples/panels/ACNetworkLaboratory.wfpanel`: complete self-contained ten-Page lab with exact embedded source.
- `tests/offline/ac_network_modules.mjs`: fractional-cycle, offset, harmonic-residual, prediction and gain/phase assertions.
- `tests/offline/ac_network_lab_mock.mjs`: no-output startup, synthetic and physical-fit routing, AD3 gate/sweep/export tests.
- `tests/ac_network_lab_smoke.cpp`: native Qt/`QJSEngine` smoke expectations; requires Windows Qt compilation.

The source and Node mocks are checked here. **The new GUI Pages and the physical AD3 integration in v8.9.2 are not yet Windows hardware validated.**
