# Releasing WFScript

## v0.1 public release baseline

The previous source baseline was validated, but the **new v0.1 packaging still requires a clean Windows rebuild and CTest pass** before tagging. Prior evidence:

- application builds and runs on the validated Windows Qt environment,
- the full native CTest suite passes (**66 / 66**),
- manual editor checks passed,
- the owner successfully ran the AD3 Hardware Test Station example.

## Suggested Git tagging workflow

```powershell
git init -b main
git add .
git commit -m "WFScript v0.1 public release"
git tag -a v0.1 -m "WFScript v0.1"
```

Then create the remote repository and push normally.

## Before publishing binaries

If you publish binaries later, also:

1. build a clean **Release** configuration,
2. deploy Qt dependencies with `windeployqt`,
3. test on a clean Windows system,
4. review Qt open-source obligations and third-party notices.
