# WFScript v0.2 — Release Notes

**Release candidate:** v0.2 · CMake project version `0.2.0` · title **WFScript v0.2**.
The public v0.1 tag and Git history are preserved. No Windows binary or Digilent SDK
DLL is included in this source package.

## A visual environment for instrument and test applications

WFScript v0.2 is a broad update to the Qt/C++ application and its embedded JavaScript
workflow. It is suitable for experimentation, custom instrumentation dashboards,
engineering utilities, and standalone operator-facing test applications.

### Highlights

- **Build and run Panels:** multi-Page visual editing, practical widgets, event-bound
  JavaScript, reusable embedded libraries, images, tables, and an uninterrupted
  full-screen Operator View.
- **Live application state:** named, computed and change-notified Panel Data; widget
  bindings and a live data inspector; dockable, filterable, per-run console.
- **Measurements and analysis:** analog/digital plots, native sample and binary paths,
  cursor tools, DSP examples, AC network and RC step-response experiments.
- **Data and connectivity:** CSV/JSON file utilities, SQLite, serial/COM, TCP/UDP,
  and supported Digilent DWF instrument functions.
- **Automated tests:** native Test Executive with Setup/Main/Cleanup, step parameters,
  structured/numeric measurements and limits, sweeps and repeats, conditional
  branches, async token-aware completion, run information, reporting and archives.
- **Forty built-in examples** and an illustrated documentation set: two books,
  65-page local HTML help system, searchable API index and example walkthroughs.

### First use

Open [Getting Started](docs/manuals/WFScript_Getting_Started.pdf), or open
[`docs/site/index.html`](docs/site/index.html) locally after downloading the source.
Review [Build on Windows](docs/BUILD_WINDOWS.md) and [Testing](docs/TESTING.md).

### Compatibility

Source targets Qt 6.9.2 / Windows and the listed Qt modules, including SerialPort.
Existing `.wfpanel` projects should be backed up before resaving in v0.2.
WaveForms/DWF is loaded dynamically and must be installed separately for real
Digilent hardware. External Rigol SCPI instrument libraries are user Panel modules,
not built-in WFScript globals.

### Test evidence and limits

On November 20, 2026 the maintainer built development baseline RC1.5.14.10.8
using Windows Qt 6.9.2 MinGW 64-bit Release and ran **116 of 116 native CTests
successfully** (41.57 seconds, zero failures). Repository source checks and
Python/Node regressions are additional gates.

This cleaned v0.2 packaging preserves that baseline's application source, native
CTest source and examples. It has not independently been compiled with Windows
MSVC CI, verified from a fresh GitHub clone, or deployed on a clean PC.
Previously reported real AD3 and Rigol hardware tests apply only to the specified
workflows. The newer v0.7.2 Rigol synchronized CH1/CH2 and RAW acquisition
workflow remains hardware-unverified.

### Hardware safety

This software is not a safety controller. Instrument-reported OFF is not independent
proof of zero terminal voltage; do not rely on process Stop or TCP disconnect as a
hardware interlock. Use correct wiring, limits and independently verified safeguards.

### Publishing

Use the [release checklist](docs/RELEASE_V0_2_CHECKLIST.md) and
[GitHub publishing guide](docs/GITHUB_V0_2_PUBLISHING.md). Preserve existing v0.1
Git history and publish only after reviewing the staged changes and CI outcome.
