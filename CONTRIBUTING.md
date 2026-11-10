# Contributing to WFScript

Thank you for your interest in WFScript.

## Baseline

Please branch from the **v0.1** public baseline (or from a newer tagged release once they exist), not from historical patch ZIPs.

## Expectations

- Keep runtime/threading/instrument ownership changes small and well-tested.
- Update examples and docs when user-facing behavior changes.
- Preserve the readable example source in `examples/source/` and rebuild embedded examples with `python tools/build_examples.py`.
- Run the checks in `docs/TESTING.md` before proposing changes.
