# Building WFScript v0.2 on Windows

## Supported toolchains

The primary owner-validated development setup is **Qt Creator with Qt 6.9.2 MinGW 64-bit**, CMake 3.21+, C++17. The repository's GitHub Actions job uses **MSVC 2022** and the matching Qt `win64_msvc2022_64` build; validate CI separately from MinGW builds.

Install Qt components **Widgets, Qml, Sql, Network, SerialPort**. The SerialPort add-on must match the selected Qt kit.

## Qt Creator (recommended)

1. Extract or clone WFScript into a short folder such as `C:\Dev\WFScript-v02`. Avoid mixing the v0.1 and v0.2 build directories.
2. Open `CMakeLists.txt` in Qt Creator and choose your Qt 6.9.2 **Desktop MinGW 64-bit** kit.
3. Use a **new build directory** outside the source tree, such as `C:\Dev\WFBuild-v02`.
4. Under Projects → CMake, set `WFSCRIPT_BUILD_TESTS=ON`, reconfigure, and **Build All**.
5. Run `WFScript.exe`. The main application window should say exactly **WFScript v0.2**.
6. Open Examples → Getting Started and the AC/RC laboratories in **Simulation** mode before physical tests.

## PowerShell (matching Qt kit shell)

```powershell
cmake -S . -B C:\Dev\WFBuild-v02 -G Ninja -DCMAKE_BUILD_TYPE=Release -DWFSCRIPT_BUILD_TESTS=ON
cmake --build C:\Dev\WFBuild-v02 --parallel 2
ctest --test-dir C:\Dev\WFBuild-v02 --output-on-failure -j 2
```

Run the commands in a terminal with your matching Qt/CMake/C++ compiler paths configured; mixing MSVC libraries with MinGW is not supported. CMake cache from an older kit should not be reused.

## Offline repository checks

```powershell
python tools/check_repo.py
python tools/build_examples.py --check
python tools/check_release.py
python tools/run_offline_tests.py    # Requires Node.js
```

## AD3 / SDK runtime

WFScript dynamically loads the Digilent WaveForms SDK for supported physical instrument operations. Install WaveForms separately so the system can find `dwf.dll`. No Digilent runtime DLL is shipped in this repository. Verify real hardware output and shutdown with a known safe circuit.

## Portable Windows release (optional)

After building in Release, create a fresh deployment folder, copy `WFScript.exe`, run the **matching Qt kit's** `windeployqt` there, and verify on a Windows machine without the Qt SDK installed. Do not claim a portable package was tested until you have performed this clean-system verification. Review [Qt licensing notes](QT_OPEN_SOURCE_NOTES.md) and third-party license notices first.
