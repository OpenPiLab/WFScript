# v0.2 repository cleanup and documentation audit

**Baseline:** WFScript RC1.5.14.10.8, with the maintainer-verified Windows
Qt 6.9.2 MinGW64 Release build and **116/116 native CTests** (41.57 s).
**Release state:** GitHub-ready source candidate, not yet tagged or published.

## Preservation and cleanup

- All production `src/` files, CMakeLists.txt, native/offline tests, and executable
  built-in example `.wfpanel`/JavaScript sources are identical to the baseline.
- Historic root changelogs and hundreds of release-candidate instructions are not
  copied to the public root. Their original bytes remain in the prior full-source
  checkpoint; use Git history for future release changes.
- `development/` retains only regression fixtures needed by offline tests,
  not all old demos or a large unreferenced 100k-measurement report.
- `docs/` now contains an illustrated Getting Started book, a User Guide,
  the offline HTML website, a 361-entry API catalog, source Markdown,
  and narrowly selected architecture/advanced reference notes.
- Broken example-to-reference links and Markdown image links were repaired.

## Verified in this Linux preparation environment

- Repository/check_release/API knowledge/source-warning checks: PASS.
- `tools/build_examples.py --check`: 40 synchronized built-in examples.
- 80/80 independent Python/Node tests: PASS on the cleaned release tree.
- 65 static HTML pages: zero missing local links.
- Native sources, tests, examples/CMake: SHA-256 byte-exact match to baseline.
- Source/HTML documentation privacy scan: no local account paths found beyond
  generic placeholder paths such as `C:\Users\<you>`.

## Still required on Windows before the GitHub v0.2 tag

- Build this exact cleaned source in Qt Creator / Qt 6.9.2; rerun 116 CTests.
- Confirm GitHub Actions MSVC build + native CTest workflow passes on `main`.
- Open locally served/offline HTML documentation and review example workflows.
- Review release licensing/attribution and any Windows binary deployment.
- Physical AD3 and external instrument verification only for workflows
  specifically included in release claims. Rigol synchronized/RAW remains untested.
