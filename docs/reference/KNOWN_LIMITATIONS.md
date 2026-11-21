# Known limitations — WFScript v0.2

- **Windows-first:** primary owner testing uses Windows with Qt 6.9.2/MinGW 64-bit. Other operating systems and compilers are not equally verified.
- **Selective physical validation:** several AD3 analog, digital and test-station workflows were exercised, including AC/RC measurements; this is not exhaustive testing of all AD3 pins, device models, timing, protocol modes or error cases.
- **Simulation is not hardware validation.** Synthetic captures and generated noise should never be presented as measured physical data.
- **Not safety-rated:** do not rely on WFScript for protection, interlocks or unattended critical output control. Check physical output state independently after faults and disconnections.
- **Native API coverage:** WaveForms SDK scripting interfaces are supported selectively; WFScript is neither the official WaveForms program nor a complete replacement for it.
- **Script debugging:** Console logs and named Panel Data Watch do not provide arbitrary QJSEngine local-variable inspection, breakpoints or stack stepping. The Watch receives declared data updates only.
- **Equation widget:** supports the documented subset of TeX-style markup used in bundled examples, not full LaTeX.
- **Fit Page:** scales controls to use the available Run viewport; a very dense Page may still require 100% view for comfortable text sizes.
- **Window and application settings:** dock positions and diagnostic choices are local preferences; they are not packed into `.wfpanel` files.
- **Distribution:** building a portable Windows executable requires Qt deployment and separate review of Qt, third-party and Digilent licensing obligations.
