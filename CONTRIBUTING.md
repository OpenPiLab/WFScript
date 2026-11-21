# Contributing to WFScript

Thank you for helping improve WFScript. Contributions are welcome in the Qt6/C++
application, embedded JavaScript APIs, panels, examples and documentation.

## Development workflow

1. Create a branch from the latest Git `main` or a tagged release, preserving the public history.
2. Keep changes small, with a clear motivation and validation evidence.
3. Keep `examples/source/` JavaScript in sync with the `.wfpanel` projects in
   `examples/panels/` using `py -3 tools/build_examples.py`.
4. Keep demonstration scripts readable: descriptive names, comments where they
   clarify behavior, accurate real-vs-simulated measurement labels and safe I/O defaults.
5. Run `py -3 tools/run_regression_gate.py` and `py -3 tools/build_examples.py --check`.
6. For C++ changes, configure `WFSCRIPT_BUILD_TESTS=ON`, build all native targets
   with Qt 6.9.2, and run the full CTest suite. Describe any test environment limitations.
7. Document new public APIs and include a minimal example. Test hardware-dependent
   workflows on actual hardware when available; don't describe simulations as physical validation.

The published repository includes test fixtures in `development/` because several
offline regressions compare earlier program shapes. See `development/README.md`.

Never commit user reports, passwords, proprietary vendor DLLs, Qt build folders
or temporary/generated files. Review [the tests](docs/TESTING.md),
[Windows setup](docs/BUILD_WINDOWS.md) and [documentation home](docs/README.md).
