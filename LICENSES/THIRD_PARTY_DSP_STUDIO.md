# v0.2-dev.8.6 third-party JavaScript license inventory

The new demonstration packages three existing upstream JavaScript implementations.

- Fraction.js version 4.3.7, Copyright (c) 2023 Robert Eisele: MIT. Unmodified ES module sourced from a locally installed published package; packaged as `vendor/fraction.mjs`.
- Pako version 1.0.11, (C) 2014–2017 Vitaly Puzrin, Andrei Tuputcyn and zlib contributors: MIT AND Zlib. The published `dist/pako.min.js` UMD bundle is embedded in `vendor/pako.mjs` behind local CommonJS-style `module`/`exports` bindings and ES named exports. The underlying compression code is not rewritten.
- base64-js version 0.0.8, (C) 2014 the upstream authors: MIT. Published CommonJS `lib/b64.js` is embedded in `vendor/base64.mjs` with a local `exports` object and ES named exports; underlying algorithms are unmodified.

Complete original MIT notices are included as `Fraction_LICENSE.txt`, `Pako_LICENSE.txt`, and `Base64JS_LICENSE.txt`. The original Pako zlib-origin attribution and license notice is in `Pako_ZLIB_LICENSE_AND_ATTRIBUTION.txt`. These notices are also **included in comments inside the embeddable vendor `.mjs` files**, because `.wfpanel` redistributes the JavaScript sources without a separate license directory.

This is not a declaration that these releases are the most recent versions. Upgrading them should be done deliberately with license, compatibility, and performance testing. The code in `DSPCore.mjs` and `SignalPackage.mjs` was authored specifically for WFScript and is **not claimed to be FFT.js, Fili, or Complex.js**.
