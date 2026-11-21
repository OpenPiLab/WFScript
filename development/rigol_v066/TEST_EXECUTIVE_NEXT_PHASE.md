# Future native Test Executive improvements (design note, not implemented)

## Problem exposed by the four-instrument test

The saved native v1 definition can describe a numeric step and its limits, but it cannot pass named step parameters into one reusable handler or await a Promise returned from that handler. The v0.6.6 test module contains per-voltage/per-waveform fixed-value exports and a short start/poll adapter solely because of those two API limitations.

## Suggested minimal next iteration

1. Add optional `parameters` object in v2 step schema; reject unsupported types and never evaluate arbitrary script expressions from it.
2. Call a step handler with an immutable, typed context (`stepId`, `parameters`, optional helpers) and archive the parameter values with each result. Keep original v1 definitions valid; do not silently repurpose the older handler signature.
3. Support a Promise-returning step mode: resolve a finite numeric result or explicit action success/failure; bind Promise completion to an execution/step token and ignore stale completions after stop/timeout.
4. Preserve output safety in the test procedure and existing per-instrument SCPI driver serialization. Native timeout only controls *logical step state*; a timed-out physical command requires isolation/cancellation before cleanup commands and physical OFF readback.
5. Test successful/failed/rejected/late-resolving Promises; step changes during execution; manual termination, timeout, process shutdown, cleanup despite failed main steps; and native persisted reports.
6. Only then consider editor-defined voltage sweeps and waveform cartesian products that expand into individually reportable steps. Avoid a second runtime or separate TestRunner configuration inside a Panel.

## Human acceptance criterion

A developer should be able to change 0.25 V steps to 0.50 V and add a 10 kHz waveform case in the visual Test Sequence Editor without adding new JavaScript exports, and then inspect a complete report with parameters, limits and measurement provenance.
