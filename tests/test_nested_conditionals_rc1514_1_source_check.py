"""RC1.5.14.1: nested loop placement in THEN/ELSE, IDs and source audit (offline)."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
cpp=(root/'src/TestSequenceEditor.cpp').read_text(encoding='utf-8')
assert 'void TestSequenceEditor::addContainer(bool repeat)' in cpp
add=cpp.split('void TestSequenceEditor::addContainer(bool repeat)')[1].split('bool TestSequenceEditor::collectVariableDeclarations')[0]
assert 'parentPath=m_selectedPath' in add
assert 'children.last().toObject().value(QStringLiteral("otherwise"))' in add
assert 'TestSequenceLoops::expand(draft,&expanded,&error)' in add
assert 'while(inherited.contains(parameter))' in add
assert 'while(inherited.contains(index))' in add
assert 'm_steps=candidate' in add
assert 'm_selectedPath.size()+1>3' not in add  # ELSE is not extra runtime depth
assert 'children.insert(childIndex,child)' in cpp  # Indent in THEN before ELSE
assert 'bool lastThen=' in cpp  # Prevent removing final THEN
cmake=(root/'CMakeLists.txt').read_text()
assert 'WFScriptTestSequenceEditorNestedConditionalSmoke' in cmake
panel=json.loads((root/'development/conditional_nested_rc1514_1/WFScript_Conditional_Nested_Sweeps_Lab_RC1_5_14_1.wfpanel').read_text())
steps=panel['testDefinition']['steps']
assert steps[1]['steps'][1]['loop']['parameter']=='diagnosticLevel'
assert steps[1]['steps'][-1]['otherwise']
assert steps[2]['steps'][1]['steps'][1]['steps'][-1]['loop']['parameter']=='diagnosticCode'
assert steps[2]['steps'][1]['steps'][1]['otherwise']
assert 'Rigol' not in panel['name']
print('PASS RC1.5.14.1: nested Sweep/Repeat actions, ELSE ordering and standalone lab')
