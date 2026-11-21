# WFScript v0.2 release acceptance checklist

## Known-good baseline (already completed on Windows)

- [x] Release CMake identity `0.2.0` / main window title `WFScript v0.2`
- [x] RC1.5.14.10.8 built with Windows Qt 6.9.2 MinGW 64-bit Release
- [x] Full 116/116 native CTests passed, 0 failures, 41.57 s (2026-11-20)
- [x] C++ application and native test sources unchanged during repository cleanup
- [x] Built-in Panels and JavaScript example sources unchanged during cleanup
- [x] Documentation prepared with two illustrated manuals and offline HTML API help

## Release package checks (repeat on final repo)

- [ ] `py -3 tools/run_regression_gate.py` succeeds on the clean GitHub-ready tree
- [ ] `py -3 tools/build_examples.py --check` succeeds
- [ ] Release archive integrity, license/attribution, and privacy scans checked
- [ ] GitHub Actions MSVC 2022 + Qt 6.9.2 full build/CTest succeeds
- [ ] Fresh clone Windows Qt Creator MinGW Build All + 116/116 native CTests
- [ ] Manual Windows: open examples, run first Panel, save/reopen, F11, Console, Data Watch
- [ ] Click through HTML documentation in Windows browser and open PDF/Word manuals
- [ ] Physical hardware regression for workflows **actually claimed** in release materials
- [ ] Independent safety review if any additional live-output examples are published
- [ ] Fresh Windows deployment package smoke test **if** binary artifacts will be published

## Git and GitHub

- [ ] Check `git status`, `git diff --check`, sources, license notices and `.gitignore`
- [ ] Preserve historical v0.1 Git commits and tag (no `git init` / force push)
- [ ] Commit reviewed source/doc update to `main`
- [ ] Confirm GitHub Actions is green on the committed code
- [ ] Create `v0.2` annotated tag **only after acceptance**
- [ ] Publish GitHub Release using `RELEASE_NOTES_v0.2.md`

Record final Git commit: ______; CI run: ______; local Qt/compiler: ______;
physical checks performed: ______; known issues: ______.
