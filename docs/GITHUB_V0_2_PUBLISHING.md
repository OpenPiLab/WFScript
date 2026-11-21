# Publish WFScript v0.2 without losing v0.1 history

The expected remote is `https://github.com/OpenPiLab/WFScript`.
The current public v0.1 tag and Git history must be preserved. Do not run `git init`
for this release or force-push the new source over the old repository.

## 1. Stage from a fresh clone

This release ZIP is a clean **source snapshot**, not a `.git` repository.
In PowerShell (adjust your paths):

```powershell
$repo    = 'C:\Dev\WFScript-Publish'
$release = 'C:\Dev\WFScript-v0.2-Source'  # Extracted release tree

git clone https://github.com/OpenPiLab/WFScript.git $repo
Set-Location $repo
git switch main
git pull --ff-only origin main
git status --short
```

Copy the **contents** of the extracted release root into the cloned folder,
**without replacing `.git`**. Example:

```powershell
robocopy $release $repo /E /XD .git build build-debug build-release .vs .idea /XF '*.user' '*.user.*'
# Robocopy exit codes 0–7 mean success; >=8 indicate a copy error.
```

Copying with `/E` does not delete obsolete tracked files already present in the
remote checkout. Review them individually with `git status`, `git ls-files`, and
`git diff`; use `git rm` on obsolete tracked files only after checking the change.
Never use `/MIR` or a blanket deletion against your original Git clone.

## 2. Validate the staged working tree

```powershell
git status --short
git diff --check
py -3 tools/check_repo.py
py -3 tools/build_examples.py --check
py -3 tools/check_release.py
py -3 tools/run_regression_gate.py
```

Open the GitHub-ready `README.md` and `docs/site/index.html`.
Review README images, legal notices and documentation links.

## 3. Build the release from the staged checkout

Open `CMakeLists.txt` with Qt Creator's Qt 6.9.2 MinGW64 kit,
set `WFSCRIPT_BUILD_TESTS=ON`, build all and execute the entire CTest suite.
See [`BUILD_WINDOWS.md`](BUILD_WINDOWS.md) and [`TESTING.md`](TESTING.md).
The RC1.5.14.10.8 checkpoint passed all 116 tests locally, but source cleanup and
MSVC GitHub Actions still require their own verification.

## 4. Publish after the checks pass

```powershell
git add -A
git diff --cached --stat
git diff --cached --check
git commit -m "Release WFScript v0.2 with complete illustrated documentation"
git push origin main
```

Review the GitHub Actions workflow on `main`. If green, create and push
an annotated tag and GitHub Release:

```powershell
git tag -a v0.2 -m "WFScript v0.2"
git push origin v0.2
```

Create the GitHub Release from the `v0.2` tag and paste or adapt
`RELEASE_NOTES_v0.2.md`. Add compiled Windows binaries only if separately deployed
and tested, with correct Qt LGPL and third-party notices. GitHub generates
source ZIP/tarballs from the tag automatically.
