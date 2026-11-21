"""RC1.5.10 structured result contract and panel data checks (Qt not required)."""
from pathlib import Path
import json,re,subprocess
root=Path(__file__).resolve().parents[1]
cpp=(root/'src/TestRunnerBridge.cpp').read_text()
h=(root/'src/TestRunnerBridge.h').read_text()
editor=(root/'src/TestSequenceEditor.cpp').read_text()
shared=(root/'src/StructuredTestResults.h').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'void TestRunnerBridge::finishStructuredResult(' in cpp
assert 'StructuredTestResults::evaluate(' in cpp and 'StructuredTestResults::validateResult(' in cpp
assert 'TestRunnerBridge::complete(' in cpp and 'm_stepToken!=token' in cpp
assert 'result' in cpp and 'measurementRules' in h
assert 'm_measurements.push_back(QVariantMap{' in cpp and 'resolvedPath' in cpp
assert 'results.size()+m_measurements.size()>TestSequenceCapacity::maxMeasurementsPerRun' in cpp
assert 'results.size()' in cpp and 'current.hasResult=true' in cpp
assert 'path.endsWith(QStringLiteral("[*]"))' in shared
assert 'One step exceeds 100 measurement results' in shared
assert 'Result nesting/element limit exceeded' in shared
assert '32 KiB' in shared
assert 'QJsonDocument::fromVariant' in shared
assert 'setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)' in editor
assert 'setSizeGripEnabled(true)' in editor
assert 'detailsScroll->setWidget(detailsPanel)' in editor
assert 'footer->addWidget(actions)' in editor
assert 'settingsLayout->insertLayout(1,dataTools)' in editor
assert 'testSequenceNamedResultsTable' in editor
assert 'testSequenceStructuredResults' in editor
assert 'Named Results & Limits' in editor
for test in ('WFScriptTestRunnerStructuredResultsSmoke','WFScriptTestSequenceEditorRC1510Smoke'):
    assert test in cmake

lab=json.loads((root/'development/structured_results_lab/WFScript_Structured_Results_Lab_RC1_5_10.wfpanel').read_text())
assert lab['testLibrary']=='StructuredResultSteps.mjs'
assert len(lab['testDefinition']['steps'])==6
assert sum(len(s.get('measurements',{})) for s in lab['testDefinition']['steps'])==7
assert any('temperatures[*]' in s.get('measurements',{}) for s in lab['testDefinition']['steps'])
assert any(s['mode']=='once' and s.get('measurements') for s in lab['testDefinition']['steps'])
assert any(s['type']=='numeric' and s.get('limits') for s in lab['testDefinition']['steps'])
assert lab['libraries'][0]['source']==(root/'development/structured_results_lab/StructuredResultSteps.mjs').read_text()

old=json.loads((root/'development/rigol_v067/Rigol_Four_Instrument_Operator_Bench_v0_6_7_Callback_Tests.wfpanel').read_text())
new=json.loads((root/'development/rigol_v068/Rigol_Four_Instrument_Operator_Bench_v0_6_8_Structured_Results.wfpanel').read_text())
assert old['pages']==new['pages']
assert old['testLibrary']==new['testLibrary']=='RigolTestSteps.mjs'
assert len(old['testDefinition']['steps'])==62
assert len(new['testDefinition']['steps'])==38
assert sum(s['type']=='numeric' and 'measurements' in s for s in new['testDefinition']['steps'])==8
assert sum(s['type']=='numeric' and 'limits' in s for s in new['testDefinition']['steps'])==26
assert sum(len(s.get('measurements',{})) or bool(s.get('limits')) for s in new['testDefinition']['steps'])==50
old_libs={x['name']:x['source'] for x in old['libraries']}
new_libs={x['name']:x['source'] for x in new['libraries']}
assert all(old_libs[name]==source for name,source in new_libs.items() if name!='RigolTestSteps.mjs')
assert 'generateAndVerifyWaveform(step)' in new_libs['RigolTestSteps.mjs']
assert "numeric==='structured'" in new_libs['RigolTestSteps.mjs']
assert 'function captureWaveform(' in new_libs['RigolTestSteps.mjs']
assert (root/'development/rigol_v068/RigolTestSteps.mjs').read_text()==new_libs['RigolTestSteps.mjs']
for file in ('development/rigol_v068/RigolTestSteps.mjs','development/structured_results_lab/StructuredResultSteps.mjs'):
    subprocess.run(['node','--check',str(root/file)],check=True,capture_output=True)
subprocess.run(['node',str(root/'development/rigol_v068/tests/test_v068.mjs')],check=True,capture_output=True,timeout=45)
print('PASS RC1.5.10: schema, native result mapping, editor resizing, 38-step Rigol/50 measurements, Node Rigol fault mocks')
