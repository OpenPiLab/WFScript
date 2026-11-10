# Building WFScript on Windows

The primary validated environment for **WFScript v0.1** is:

- **Qt Creator**
- **Qt 6.9.2**
- **Desktop MinGW 64-bit kit**
- **CMake 3.21+**

## Recommended Qt Creator workflow

1. Extract or clone the repository to a short path such as `C:\Dev\WFScript`.
2. Open `CMakeLists.txt` with **File → Open File or Project...** in Qt Creator.
3. At **Configure Project**, choose your real installed **Desktop Qt 6.9.2 MinGW 64-bit** kit.
4. Use a fresh build folder, for example `C:\Dev\WFBuild`.
5. Configure and build the `WFScript` target.
6. Run the application and start with **Panel Designer → Examples → Getting Started** in **Simulation** mode.

## Enable the native tests

To build the native smoke/regression tests, enable the CMake option:

- `WFSCRIPT_BUILD_TESTS=ON`

In Qt Creator this can be done from the project's CMake configuration, then **Run CMake** again.

## Command-line build

Application only:

```powershell
cmake -S . -B C:\Dev\WFBuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DWFSCRIPT_BUILD_TESTS=OFF
cmake --build C:\Dev\WFBuild --parallel
```

Application plus tests:

```powershell
cmake -S . -B C:\Dev\WFBuild -G Ninja -DCMAKE_BUILD_TYPE=Debug -DWFSCRIPT_BUILD_TESTS=ON
cmake --build C:\Dev\WFBuild --parallel
```

## Run the full validated test suite

```powershell
ctest --test-dir C:\Dev\WFBuild -j 2 --output-on-failure
```

The validated underlying source baseline passed **66/66 native CTests**. The repackaged v0.1 source has not yet been independently rebuilt; please run all 66 tests again in a fresh build directory.

## Digilent runtime note

WFScript loads the Digilent WaveForms SDK dynamically at runtime for physical-device use. Install Digilent WaveForms separately so `dwf.dll` is available. No Digilent runtime binaries are shipped in this repository.

## Deploying a Windows build

After a successful Release build, use the matching Qt kit's `windeployqt` to gather runtime Qt dependencies for a distributable folder. Then test that deployed folder on a clean Windows machine before sharing binaries.

## Final warning-cleanup gate

The initial v0.1 Debug build succeeded with compiler warnings. The cleanup
candidate has no global warning suppression; the revised source must be
compiled on Windows before release. Enable `WFSCRIPT_BUILD_TESTS=ON`, run
**Build All**, review any new `warning:` lines, then run the complete native
CTest suite. See [the audit](WARNING_CLEANUP_AUDIT.md).
