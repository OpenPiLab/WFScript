# Known limitations and release boundaries

- **Windows-first release**: WFScript v0.1 has been validated on Windows with Qt 6.9.2 / MinGW 64-bit.
- **Independent API**: WFScript JavaScript resembles parts of WaveForms scripting, but it is its own API and is not guaranteed to be directly compatible with official WaveForms scripts.
- **Selective hardware validation**: Important workflows have been exercised, including the owner's successful AD3 Hardware Test Station run, but not every instrument mode and corner case has been exhaustively validated.
- **Simulation is not hardware proof**: a passing simulation run does not prove physical I/O correctness or electrical safety.
- **Not safety-rated**: WFScript is not intended for safety-critical, unattended, or protection-related use.
- **Equation widget**: only a subset of LaTeX-like notation is supported.
- **Runtime vs design-time values**: some runtime-edited values are intentionally separate from design-time defaults stored in the panel.
- **Binary distribution requires care**: if you later distribute application binaries, validate deployment and review Qt and other dependency licensing obligations.
