# Reading a WFScript Example: A Student's Map

1. Open a Panel from **Examples**, choose **Edit JavaScript**, and start at the
   `GOAL`, `TRY FIRST`, and `TRACE` comments near the top (where available).
2. Use **Properties → Events → Go to Handler** on a widget to jump to the function
   that receives its designer-owned event. The shared JavaScript runtime belongs
   to the whole `.wfpanel`, even when it has multiple Pages.
3. Look for a theory function, then a model or physical acquisition function.
   Not every example owns hardware; do not assume `MODEL` data were captured.
4. Study the small helpers that perform the mathematics. In the AC Laboratory,
   use `CaptureAnalysis.mjs` for sinusoidal fitting. In the RC Step Laboratory,
   use `RCStepAnalysis.mjs` for exponential fitting.
5. Compare a calculated value with a physical observation. Examine residuals and
   units. A small fit residual is **not** the same as calibrated accuracy.
6. Change a harmless parameter in **Simulation** first. Never remove an
   interlock, increase hardware limits, or energize an unknown circuit as a
   beginner exercise.

The 36 early learning examples have companion guides in `examples/guides/`.
The newer AC and RC laboratories include mathematical theory and annotated
code directly inside their Panels. The expanded style rules are in
`docs/JAVASCRIPT_EDUCATIONAL_STYLE.md`.

Third-party vendor modules are clearly identified dependencies and aren't
recommended starting points for studying the code.
