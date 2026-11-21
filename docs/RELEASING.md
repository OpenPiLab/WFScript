# Releasing WFScript

The current release target is **v0.2**; v0.1 remains the previous public release. Preserve Git history and existing tags.

1. Review [v0.2 checklist](RELEASE_V0_2_CHECKLIST.md) and complete clean Windows build/native tests and manual acceptance checks.
2. Publish verified source from the existing `main` history (see [GitHub workflow](GITHUB_V0_2_PUBLISHING.md)); do **not** initialize an unrelated repository or force-push over v0.1.
3. Review the GitHub Actions build/test status on `main`.
4. Only after the gates pass, create annotated tag `v0.2`, push the tag, and draft the GitHub Release using `RELEASE_NOTES_v0.2.md`.
5. Binary releases are optional: a portable EXE requires matching Qt `windeployqt` assets, licensing review, and clean-system smoke testing before distribution.

Historical v0.1/revision-specific notes are retained in the repository under `docs/development-history/` and `docs/` as reference, not active release instructions.
