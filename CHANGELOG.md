## v0.1 warning-cleanup candidate — Windows Qt build pending

- Source-level correction of all seven warning categories from the initial Windows Debug build log (373 emitted warnings, including repeated compilation of shared files in native test targets).
- Default-initialize three trailing WidgetProperty metadata fields without changing aggregate initialization or project format.
- Replace one deprecated Qt regular-expression match option and a deprecated mouse-event test constructor.
- Clarify error-return indentation, make snippet index narrowing explicit, resolve signed counter comparison safely, and remove an unused intermediate.
- Preserve app version 0.1.0; this is **not** a separate public feature release.
- Source/offline checks only; **Windows Build All and all 66 native CTests must be rerun** before release.

# Changelog

## v0.1 — Initial public release

- First public source release of WFScript.
- Includes the multi-Page Panel Designer, JavaScript editor, API explorer, built-in examples, plotting widgets, experiment tables, and native Test Executive.
- Experiment Table values are exposed to JavaScript as native arrays through `ui.get()`.
- Example library is organized into six user-facing categories.
- Windows Qt Creator import path has been cleaned up for a normal kit-based workflow.
- Added a WFScript application icon and Windows executable icon resource.
- Release validation completed by the owner on Windows Qt 6.9.2 / MinGW 64-bit.
- Previous source baseline: **66 / 66 native CTests passed** on Windows. This renamed v0.1 packaging remains to be rebuilt and retested.
- Owner confirmed a successful run of the **AD3 Hardware Test Station** example on real hardware.
