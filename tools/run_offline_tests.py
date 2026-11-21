#!/usr/bin/env python3
"""Run all repository Python and Node offline regression tests.

Native Qt/CTest and physical AD3 testing remain separate release gates.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
NODE = shutil.which('node')
if not NODE:
    raise SystemExit('Node.js is required for the offline JavaScript regression suite.')

# Native Windows Python defaults to a legacy ANSI code page unless explicitly
# configured. Child processes must be deterministic across Windows and Linux.
child_env = dict(os.environ)
child_env['PYTHONIOENCODING'] = 'utf-8'

files = sorted((ROOT / 'tests').glob('*.py'))
files += sorted((ROOT / 'tests').glob('*.js'))
files += sorted((ROOT / 'tests').glob('*.mjs'))
failed = []
for path in files:
    command = [sys.executable, '-X', 'utf8', str(path)] if path.suffix == '.py' else [NODE, str(path)]
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                encoding='utf-8', errors='replace', env=child_env, timeout=60)
    except subprocess.TimeoutExpired:
        print('FAIL', path.name, '(timeout)', flush=True)
        failed.append(path.name)
        continue
    if result.returncode:
        print('FAIL', path.name, flush=True)
        print((result.stdout + result.stderr)[-4000:], flush=True)
        failed.append(path.name)
    else:
        print('PASS', path.name, flush=True)
print(f'Offline tests: {len(files)-len(failed)}/{len(files)} passed')
if failed:
    sys.exit(1)
