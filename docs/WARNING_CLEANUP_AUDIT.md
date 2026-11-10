# Windows Qt 6.9.2 / MinGW 13.1 warning-cleanup audit

## Source of evidence

The project owner provided a complete **Debug Build All** log of the newly branded WFScript v0.1 application. It finished successfully and confirmed that the application icon compiled and displayed. The log included **373 warning messages**, many repeated by smoke targets compiling the same code.

## Warning counts from the supplied log

| GCC warning category | Messages | Correction |
| --- | ---: | --- |
| `-Wmissing-field-initializers` | 298 | Explicit default member initializers on the three trailing `WidgetProperty` metadata fields; no constructor added |
| `-Wdeprecated-declarations` | 25 | Qt regular-expression option; smoke-test mouse-event constructor |
| `-Wmisleading-indentation` | 24 | Unconditional error-path returns and CSV error handling split onto clear statements |
| `-Wnarrowing` | 22 | Explicit `qsizetype` to `int` conversion for editor snippet cursor positions |
| `-Wsign-compare` | 2 | Count limits cast to signed 64-bit for comparison with signed `qRound64` results |
| `-Wrange-loop-construct` | 1 | Iterate over string literals as `const char*`, then construct `QString` only when required |
| `-Wunused-variable` | 1 | Remove unused Wavegen simulation intermediate |

These changes do **not** alter any Digilent SDK call sequence or the panel project schema. The signed-count fix avoids the potentially incorrect comparison of negative counts after integer promotion. That particular clock setup path should be retested with hardware as appropriate.

## Verification gate

This package has source-level/offline checks only. Before publishing:

1. Build All in Qt Creator from the modified source (native test option ON).
2. Record **remaining warnings**, if any, from the new log.
3. Run all 66 CTests and verify **66/66 passed**.
4. Launch the application and check the icon, editor snippets, and a representative Panel.
5. If available, rerun the known safe AD3 Hardware Test Station workflow.

No global suppression of `-Wall`, `-Wextra`, or `-Wpedantic` has been introduced.
