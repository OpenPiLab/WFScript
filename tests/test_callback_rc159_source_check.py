"""Offline guard for the RC1.5.9 callback/parameter schema and Rigol variant.

Does not substitute for native Windows CTest or physical Rigol tests.
"""
from pathlib import Path
import json
import re
import subprocess
root=Path(__file__).resolve().parents[1]
cpp=(root/'src/TestRunnerBridge.cpp').read_text()
h=(root/'src/TestRunnerBridge.h').read_text()
ui=(root/'src/TestSequenceEditor.cpp').read_text()
knowledge=(root/'src/ApiKnowledge.cpp').read_text()
cmake=(root/'CMakeLists.txt').read_text()
for key in ('Q_INVOKABLE bool complete','Q_INVOKABLE bool fail','Q_INVOKABLE bool isActive'):
    assert key in h,key
assert 'm_stepToken=QUuid::createUuid()' in cpp
assert 'token==m_stepToken' in cpp
assert 'm_stepToken.clear();m_stepCompletionQueued=false;' in cpp
assert 'QTimer::singleShot(0,this,[this,token,retained]' in cpp
assert 'stepContext(step)' in cpp
assert 'QStringLiteral("parameters")' in cpp
assert 'QStringLiteral("callback")' in cpp
assert 'ScriptEditor(codeTab)' in ui
assert 'testSequenceParametersJson' in ui
assert 'Create missing handler?' in ui
assert 'TestRunner.complete' in knowledge
assert 'WFScriptTestRunnerCallbackSmoke' in cmake
assert 'WFScriptTestSequenceEditorRC159Smoke' in cmake

old=json.loads((root/'development/rigol_v066/Rigol_Four_Instrument_Operator_Bench_v0_6_6_Readable_Tests.wfpanel').read_text())
new=json.loads((root/'development/rigol_v067/Rigol_Four_Instrument_Operator_Bench_v0_6_7_Callback_Tests.wfpanel').read_text())
assert old['pages']==new['pages']
assert old['testDefinition']['config']==new['testDefinition']['config']
assert len(old['testDefinition']['steps'])==len(new['testDefinition']['steps'])==62
assert sum(s['type']=='numeric' for s in new['testDefinition']['steps'])==50
assert all(a['id']==b['id'] and a['type']==b['type'] and a.get('limits')==b.get('limits') for a,b in zip(old['testDefinition']['steps'],new['testDefinition']['steps']))
assert sum(s['mode']=='callback' for s in new['testDefinition']['steps'])==38
assert sum(s['mode']=='once' for s in new['testDefinition']['steps'])==24
assert all(s.get('parameters') for s in new['testDefinition']['steps'] if s['id'].startswith(('psu_','dmm_')) and s['id']!='psu_off')
libs={x['name']:x['source'] for x in new['libraries']}
oldlibs={x['name']:x['source'] for x in old['libraries']}
assert all(libs[k]==v for k,v in oldlibs.items() if k!='RigolTestSteps.mjs')
code=libs['RigolTestSteps.mjs']
assert 'function startStep(' not in code
assert 'function pollStep(' not in code
for name in ('setAndReadPsu','readDmmAt','generateAndCapture','readCaptureMetric','releaseGenerator','releaseSupply'):
    assert re.search(r'export function '+name+r'\s*\(',code),name
assert 'TestRunner.complete(step.token' in code
assert 'TestRunner.isActive(step.token)' in code
for f in ('WFScpiCore.mjs','RigolTestSteps.mjs'):
    assert (root/'development/rigol_v067'/f).exists()
subprocess.run(['node','--check',str(root/'development/rigol_v067/RigolTestSteps.mjs')],check=True)
subprocess.run(['node','--check',str(root/'development/callback_lab/CallbackTestSteps.mjs')],check=True)
subprocess.run(['node',str(root/'development/rigol_v067/tests/test_v067.mjs')],check=True,timeout=45)
print('PASS RC1.5.9: token-guarded callback API, reusable parameterized Rigol steps, original hardware drivers/pages preserved, Node fault mock')
