#!/usr/bin/env python3
"""Preflight: every PanelRuntime smoke target links its direct runtime dependencies.

Qt AUTOGEN only generates QObject meta-object code for sources/headers listed in
that target. Keeping compile sources and MOC headers together prevents numerous
Windows ld undefined-reference failures.
"""
from __future__ import annotations
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TARGETS = ("WFScriptPanelHooksSmoke", "WFScriptPanelRuntimeLifecycleSmoke",
           "WFScriptPanelEventRuntimeSmoke")
REQUIRED = (
    "src/PanelUiBridge.h",
    "src/ScriptWorker.h", "src/ScriptWorker.cpp",
    "src/TestRunnerBridge.h", "src/TestRunnerBridge.cpp",
    "src/TestResultsRepository.cpp",
    "src/StaticIOBridge.h", "src/StaticIOBridge.cpp",
    "src/PatternsBridge.h", "src/PatternsBridge.cpp",
)


def check(cmake: str | None = None) -> list[str]:
    content = cmake if cmake is not None else (ROOT/"CMakeLists.txt").read_text(encoding="utf-8")
    problems = []
    sources = re.search(
        r"set\(WFSCRIPT_PANEL_RUNTIME_SMOKE_DEPENDENCIES\s+(.+?)\s*\)",
        content, re.DOTALL)
    if not sources:
        return ["Missing WFSCRIPT_PANEL_RUNTIME_SMOKE_DEPENDENCIES source group"]
    deps = set(re.findall(r"\bsrc/[A-Za-z0-9_./-]+\.(?:cpp|h)\b", sources.group(1)))
    for required in REQUIRED:
        if required not in deps:
            problems.append(f"Runtime smoke dependency missing: {required}")
        elif not (ROOT/required).is_file():
            problems.append(f"Runtime smoke dependency file missing: {required}")
    for target in TARGETS:
        expr = rf"qt_add_executable\({target}\s+(.+?)\s*\)"
        found = re.search(expr, content, re.DOTALL)
        if not found or "src/PanelRuntime.cpp" not in found.group(1):
            problems.append(f"{target}: missing PanelRuntime.cpp in target")
        if not re.search(
            rf"target_sources\({target}\s+PRIVATE\s+\$\{{WFSCRIPT_PANEL_RUNTIME_SMOKE_DEPENDENCIES\}}\s*\)",
            content):
            problems.append(f"{target}: missing shared PanelRuntime/MOC dependencies")
    return problems


if __name__ == "__main__":
    failures = check()
    for item in failures:
        print("FAIL:", item, file=sys.stderr)
    if failures:
        sys.exit(1)
    print("PASS: all three PanelRuntime smoke targets include runtime and Qt AUTOMOC sources")
