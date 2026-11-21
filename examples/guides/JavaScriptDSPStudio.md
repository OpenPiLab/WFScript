# Lab 38 — JavaScript DSP Studio: Import, Analyze, Package

**Level:** Intermediate | **Category:** Graphics & Charts | **Pages:** One

**Safety:** Simulation only. This example does not use any instrument outputs or sample real hardware.

## Objective

Turn WFScript into a small signal-processing application using *reusable JavaScript modules*. Compare a clean signal with noise/harmonics, apply a two-pole Butterworth lowpass filter, view the FFT spectrum, and package a complete signal in a portable lossless string.

## Try it

1. Open **Examples → Graphics & Charts → JavaScript DSP Studio — Third-Party Libraries** and Run Panel in Simulation.
2. The top chart shows raw and digitally filtered sample values. Change **WAVEFORM** from Sine to Square or Triangle.
3. Increase **3rd HARMONIC** and observe the new spectral peak near three times the tone frequency (subject to Nyquist and sampling).
4. Increase **NOISE FLOOR** and watch the spectral background rise.
5. Decrease **LOWPASS CUTOFF** to reduce high-frequency content. The bottom chart shows the predicted magnitude response of the *digital biquad*, not an analog physical filter.
6. Use **NEW NOISE** to produce a different seeded noise sequence.
7. Use **PACK + VERIFY** to test the imported Pako and base64-js modules. Compression and exact round-trip verification run entirely in JavaScript. The Panel reports compressed bytes relative to the original JSON bytes.
8. In Design mode, choose a Button and inspect **Properties → Events** to open its JavaScript handler; inspect `DSPCore.mjs` and `SignalPackage.mjs` in Library Manager.

## What each third-party library actually does

* **Fraction.js** gives a rational samples/cycle ratio (`16384/440 = 2048/55` exactly), which avoids displaying an approximate frequency-grid ratio as an exact integer.
* **Pako** performs actual Deflate compression and decompression on a JSON capture.
* **base64-js** converts compressed bytes into text suitable for JSON/transport and back.

`DSPCore.mjs` contains **WFScript-authored** FFT and filter demonstrations. They are not redistributed FFT.js or Fili and are not certified for metrology. The modules are stored inside the `.wfpanel`, so the example doesn't require installing Node.js or npm.

## Explore the package

```javascript
var DSP = modules.load('DSPCore.mjs');
var Package = modules.load('SignalPackage.mjs');

// The Package module itself uses relative ES imports:
// import Fraction from './vendor/fraction.mjs';
// import { deflate, inflate } from './vendor/pako.mjs';
// import { fromByteArray, toByteArray } from './vendor/base64.mjs';
```

The `modules.load()` return value is a namespace of named exports. The example intentionally avoids manual `ui.on()` wiring: its Button/input events are stored in the Designer's event properties.

## Next experiment

Replace `DSPCore.mjs` with a vetted third-party FFT or filter library, preserving the same small interface. Compare numerical outputs and CPU timing against the educational implementation, then repeat using real AD3 samples once a dedicated acquisition adapter has been validated.

Full implementation notes: `docs/JAVASCRIPT_DSP_STUDIO_V02_DEV8_6.md`.
