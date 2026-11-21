#!/usr/bin/env python3
"""Reproduce the v0.6.6 Panel from the user-tested v0.6.5.1 layout and readable sources.

No C++ or Test Executive source is changed. The 62-step definition and original
five embedded SCPI driver libraries are taken unchanged from v0.6.5.1.
"""
from pathlib import Path
import json
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BASE = ROOT / 'rigol_v0651' / 'Rigol_Four_Instrument_Operator_Bench_v0_6_5_1_Responsive_Test_Page.wfpanel'
OUTPUT = HERE / 'Rigol_Four_Instrument_Operator_Bench_v0_6_6_Readable_Tests.wfpanel'

def build():
    panel = json.loads(BASE.read_text(encoding='utf-8'))
    panel['script'] = (HERE / 'RigolBench_4Instruments_v0_6_6.js').read_text(encoding='utf-8')
    panel['scriptName'] = 'RigolBench_4Instruments_v0_6_6.js'
    panel['name'] = 'Rigol Four-Instrument Operator Bench v0.6.6 — Readable Test Procedures'
    panel['description'] = ('Four unchanged Rigol operator pages and a fifth native Test Executive page. '
        'Real test procedures are in RigolTestSteps.mjs, with shared instrument sessions, '
        'safety checks and 62 individually reported steps.')
    assert panel['testLibrary'] == 'RigolTestSteps.mjs'
    assert panel['libraries'][-1]['name'] == 'RigolTestSteps.mjs'
    panel['libraries'][-1]['source'] = (HERE / 'libraries' / 'RigolTestSteps.mjs').read_text(encoding='utf-8')
    assert len(panel['testDefinition']['steps']) == 62
    result = json.dumps(panel, indent=2, ensure_ascii=False) + '\n'
    OUTPUT.write_text(result, encoding='utf-8')
    return OUTPUT

if __name__ == '__main__':
    print(build())
