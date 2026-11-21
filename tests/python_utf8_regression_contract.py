#!/usr/bin/env python3
"""Guard cross-platform UTF-8 subprocess and direct-file test behavior."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
gate = (root/'tools/run_regression_gate.py').read_text(encoding='utf-8')
runner = (root/'tools/run_offline_tests.py').read_text(encoding='utf-8')
for name, source in [('gate', gate), ('runner', runner)]:
    assert "'-X'" in source and "'utf8'" in source, name
    assert "'PYTHONIOENCODING'" in source and "'utf-8'" in source, name
assert "encoding='utf-8'" in runner, 'Captured Node/Python output must decode as UTF-8'
for name in (
    'experiment_table_rc15146_source_invariants.py',
    'panel_page_ids_static_check.py',
    'theme_lifecycle_rc15149_source_check.py',
    'test_sequence_loops_rc1511_offline.py',
):
    source = (root/'tests'/name).read_text(encoding='utf-8')
    assert '.read_text()' not in source, name
assert '7->38 Rigol steps' in (root/'tests/test_sequence_loops_rc1511_offline.py').read_text(encoding='utf-8')

# Exercise the actual Python flags and Unicode input/output under a simulated
# legacy-codepage parent; no implicit Windows-specific globals are required.
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)/'utf8-fixture.txt'
    path.write_text('waveform → voltage – ✓',encoding='utf-8')
    script = 'import sys,pathlib; assert sys.flags.utf8_mode == 1; print(pathlib.Path(sys.argv[1]).read_text())'
    env = dict(os.environ, PYTHONUTF8='0', PYTHONIOENCODING='utf-8')
    result = subprocess.run([sys.executable,'-X','utf8','-c',script,str(path)],
                            env=env, capture_output=True, text=True,
                            encoding='utf-8', timeout=15)
    assert result.returncode==0, result.stderr
    assert result.stdout.strip()=='waveform → voltage – ✓', repr(result.stdout)
print('PASS Windows UTF-8 subprocess, console, source and Panel file contracts')
