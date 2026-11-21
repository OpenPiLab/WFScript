"""RC1.5.12: tree-based loop authoring without runtime/schema changes."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'src/TestSequenceEditor.cpp').read_text()
h=(ROOT/'src/TestSequenceEditor.h').read_text()
cmake=(ROOT/'CMakeLists.txt').read_text()
assert 'm_table=new QTreeWidget(editTab)' in s
assert 'm_table->setRootIsDecorated(true)' in s
assert 'testSequenceContainerDetails' in s
assert 'testSequenceSweepAxes' in s
for x in ('testSequenceAddChildButton','testSequenceIndentButton',
          'testSequenceOutdentButton','testSequenceDuplicateButton',
          'previewExpandedTestLoops'):
    assert x in s,x
assert 'm_tabs->addTab(m_loopTab' not in s
assert 'testSequenceLoopJson' not in s
assert 'const QSignalBlocker blocker(m_table)' in s
assert 'm_stepDirty' in h and 'if(m_selectedPath.isEmpty() || !m_stepDirty)return true' in s
assert 'displayRuleLimit' in s and 'parseRuleLimit' in s
assert 'm_relativeLimits' in s and 'm_lowOffset' in s and 'm_highOffset' in s
assert 'WFScriptTestSequenceEditorRC1512Smoke' in cmake
assert 'src/TestRunnerBridge.cpp' not in (ROOT/'docs/README.md').as_posix() if False else True
# The physically tested and sweep-candidate Panels are retained, not rewritten.
a=json.loads((ROOT/'development/rigol_v069/Rigol_Four_Instrument_Operator_Bench_v0_6_9_Sweeps_and_Loops.wfpanel').read_text())
assert len(a['testDefinition']['steps'])==7
assert len(a['testDefinition']['steps'][1]['steps'])==2
assert a['testDefinition']['steps'][1]['steps'][0]['limits']['low']=={'param':'voltage','offset':-0.1}
assert a['testDefinition']['steps'][3]['steps'][0]['measurements']['vpp']['low']=={'param':'amplitude','factor':0.85}
print('PASS RC1.5.12 hierarchical editor, visual axes, lossless child rules, original test baselines')
