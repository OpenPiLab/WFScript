# WFScript Educational JavaScript Style — v0.2-dev.8.9.4

## Why this is a product requirement

WFScript is a visual engineering teaching environment. The JavaScript displayed in
**Edit JavaScript** and **Libraries** is part of the lesson, not hidden implementation
detail. A student should be able to read from the physics/equations Page through
the program, then modify a small safe piece and observe the result.

This standard applies to **all WFScript-authored student-facing examples and modules**.
It does not require modification of third-party vendor modules, runtime C++, old
regression fixtures, or users' separately saved `.wfpanel` projects.

## Formatting rules

1. Use four spaces for indentation; do not use tabs.
2. Use spaces around arithmetic, comparison, and assignment operators and
   after commas. Put successive statements on separate lines.
3. Put members of multi-property objects and long arrays on separate lines.
4. Prefer one variable declaration per line in **new code**. The formatting-only
   v8.9.4 revision does not split existing multi-declaration statements because
   that would change executable tokens and complicate verification.
5. Prefer descriptive names **with units** (`sampleRateHz`, `timeConstantS`,
   `capacitanceNf`) when adding or revising an algorithm. Preserve existing
   public API names and binding IDs in formatting-only changes.
6. In **new code**, put braces around all `if`, `for`, `while` and `else` bodies,
   including one-line conditions. Legacy one-line bodies are not rewritten
   automatically because this release guarantees identical code tokens.
7. Use small functions grouped by role: theory, simulation, device acquisition,
   numerical analysis, presentation, events, reporting, lifecycle.
8. Comments should explain **why**, the mathematics, units, assumptions,
   provenance of data, or safety implications. Do not narrate obvious syntax.
9. Keep **MODEL**, **THEORY**, and **AD3 MEASURED** distinguishable in comments,
   UI strings, results and exported files.
10. Do not introduce unsupported QJSEngine syntax. Node parser success alone is
    not proof of Windows Qt compatibility.

## The educational sequence

A teaching laboratory should let a student follow these steps:

1. **Theory** — identify the governing equation and define each variable.
2. **Predict** — compute what an ideal circuit should do; say when this is model-only.
3. **Acquire** — identify the actual instrument, pins, voltage, sample count and
   *read-back* sample rate. Explain when output can become energized.
4. **Analyze** — name the fitting basis, cost function, optimizer, assumptions,
   sample selection, and units. Expose intermediate calculations.
5. **Diagnose** — plot fitted vs captured samples, residuals, and model quality.
6. **Document** — preserve source labels and sufficient context for repetition.

### AC sinusoidal fit

At a known frequency, fit measured voltage samples using:

```text
v(t) = DC + a sin(2πft) + b cos(2πft)
A = sqrt(a²+b²)
phase = atan2(b,a)
residual[i] = v_measured[i] - v_fit[i]
```

Start at `examples/source/ACNetworkLaboratory.js`, follow `acquireOne()`, then
open `examples/libraries/CaptureAnalysis.mjs` and read `fitSinusoid()` and
`analyzeTwoChannel()`. Compare against the **theory-only** functions in
`CircuitLab.mjs`.

### RC exponential fit

For a fixed trial time constant, fit:

```text
v(t) = V∞ + A exp(-t/τ)
SSE(τ) = sum((v_measured[i] - v_fit[i,τ])²)
```

Start at `examples/source/RCStepResponseLaboratory.js`, follow
`capturePhysical()` to `Step.analyzeCapture()`, then open
`examples/libraries/RCStepAnalysis.mjs` and study `edgesFromInput()`,
`solveAtTau()`, and `fitTransition()` in that order.

`solveAtTau()` solves linear coefficients at a fixed trial τ. `fitTransition()`
searches over τ; no hidden optimizer is required. The 63.2% comparison is not
fully independent because its threshold uses fitted values.

## How to format without changing behavior

This source tree ships a **developer-only** checked formatter. Node.js and the
TypeScript development package are needed to run it; **WFScript itself does not
require either package**.

```powershell
npm install --no-save typescript@5.8.3
node tools/format_educational_javascript.js
node tools/format_educational_javascript.js --check
python tools/build_examples.py
python tools/build_examples.py --check
python tests/example_curriculum_check.py
python tests/educational_javascript_format_lock_check.py
```

The formatter uses TypeScript's syntax tree to introduce safe whitespace, then
uses TypeScript formatting rules. It **rejects changes** to the executable
JavaScript token stream or syntax-tree shape. Do **not** use it as an optimizer,
minifier, converter, or a substitute for executing the existing tests.

The `docs/EXAMPLE_JAVASCRIPT_FORMAT_LOCK.json` file records the expected SHA-256
hash of each formatted script and the independently checked executable token
signature. The previous curriculum behavior hash lock
`docs/EXAMPLE_EDUCATION_BASELINE_LOCK.json` remains preserved for provenance.
Intentional functional changes require corresponding test, documentation and
reviewed lock updates, not an automatic relaxation of this standard.

## Third-party libraries

The embedded `vendor/` files for Fraction.js, pako, and base64-js are external
libraries. Their upstream source, license notices, and byte-level identity are
preserved. Students should start with WFScript's readable adapter/module code
rather than an optimized third-party distribution. These dependencies are not
numerical implementations developed specifically for the lessons.

## Limitations

- Existing saved custom `.wfpanel` files are **not** rewritten by this revision;
  formatting is applied to the updated built-in examples and new standalone
  copies included with the release.
- The programmer's script editor itself does **not** gain a Format button here.
- No change in runtime, instrument safety, or acquisition behavior is claimed.
- The native QJSEngine smoke tests still need execution in Windows Qt; the
  formatting/token/Node checks are source-level and mock-level checks only.
