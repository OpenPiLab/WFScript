#!/usr/bin/env python3
"""WFScript RC1.5.14.10.8 cross-platform offline release/regression gate.

Native Windows CTests and physical hardware validation are separate gates.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
CHECKS=[
    ('Repository integrity', 'tools/check_repo.py'),
    ('Release contracts', 'tools/check_release.py'),
    ('API reference contracts', 'tools/check_api_knowledge.py'),
    ('Complete Python/Node offline regression', 'tools/run_offline_tests.py'),
]
for description, relative in CHECKS:
    print(f'\n=== {description}: {relative} ===',flush=True)
    # Force UTF-8 for all child file reads and console output, regardless of
    # the Windows ANSI code page (for example cp1252) or Python locale.
    env = dict(os.environ)
    env['PYTHONIOENCODING'] = 'utf-8'
    code=subprocess.call([sys.executable,'-X','utf8',str(ROOT/relative)],cwd=ROOT,env=env)
    if code:
        print(f'FAIL: {description} (exit {code})',file=sys.stderr,flush=True)
        raise SystemExit(code)
print('\nPASS: RC1.5.14.10.8 offline regression gate (Qt/CTests and hardware not exercised)',flush=True)
