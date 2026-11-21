# WFScript AC Network Laboratory — third-party library attributions

This Panel embeds six ECMAScript modules. Two are authored specifically for this example (`CircuitLab.mjs`, `CaptureAnalysis.mjs`) and one was originally created for WFScript DSP Studio (`SignalPackage.mjs`).

Three bundled third-party algorithm modules are redistributed **unchanged from the v0.2-dev.8.8 source tree**:

- Fraction.js v4.3.7, Robert Eisele, MIT license.
- Pako v1.0.11, Vitaly Puzrin, Andrei Tuputcyn and zlib contributors, MIT and Zlib licenses.
- base64-js v0.0.8, MIT license.

The complete license and attribution text is embedded at the head of each `vendor/*.mjs` module, since a standalone `.wfpanel` redistributes those modules without an external LICENSES folder. The repository's full notices are also at `LICENSES/THIRD_PARTY_DSP_STUDIO.md` and accompanying vendor license files.

WFScript's RC math and least-squares capture fitting are **not claimed to use an imported Complex.js, Math.js, or FFT.js implementation**. Imported third-party Fraction.js is used for sample grid ratios; Pako and base64-js are used for optional lossless physical-capture packaging.
