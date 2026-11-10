#!/usr/bin/env python3
"""Protect native TestRunner smoke fixtures against three observed false negatives.

These checks supplement (but cannot replace) compiling/running the Qt CTests.
"""
from pathlib import Path
import re


def check(root: Path) -> list[str]:
    errors = []
    context=(root/'tests/test_runner_context_smoke.cpp').read_text(encoding='utf-8')
    if "TestRunner.runInfo().uutId===expectedUutId" not in context or \
       "expectedUutId='ABC-0043'" not in context:
        errors.append('RunContext fixture must update the expected UUT identity for the second run')
    numeric=(root/'tests/test_project_native_executive_smoke.cpp').read_text(encoding='utf-8')
    if 'assert(!script(js,"TestRunner.run()").toBool());' not in numeric or \
       'assert(runner.state()=="failed");' not in numeric:
        errors.append('ProjectExecutive must expect false from a synchronous failed run and verify the failed verdict')
    archive=(root/'tests/test_report_archiving_566_smoke.cpp').read_text(encoding='utf-8')
    # Every archive test step is a simple object literal (no nested braces).
    defs=re.findall(r'TestRunner\.add\(\{([^{}]*)\}',archive)
    if len(defs)!=6:
        errors.append(f'Archive fixture: expected six explicit step definitions, got {len(defs)}')
    for index,definition in enumerate(defs,1):
        if not re.search(r"\bname\s*:\s*['\"]", definition):
            errors.append(f'Archive fixture step {index} missing required name')
    return errors


if __name__ == '__main__':
    root=Path(__file__).resolve().parents[1]
    problems=check(root)
    for p in problems:print('FAIL:',p)
    if problems:raise SystemExit(1)
    print('PASS: TestRunner smoke fixtures align with native TestRunner API contract')
