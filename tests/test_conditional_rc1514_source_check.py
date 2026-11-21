"""Check RC1.5.14 conditional authoring, runtime guards and bundled lab (offline)."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
runner=(root/'src/TestRunnerBridge.cpp').read_text()
expander=(root/'src/TestSequenceLoops.h').read_text()
editor=(root/'src/TestSequenceEditor.cpp').read_text()
header=(root/'src/TestSequenceConditions.h').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'm_conditionOutcomes' in runner and 'Conditional branch not selected' in runner
assert 'm_conditionOutcomes.clear()' in runner and 'm_declaredVariables=declared;' in runner
assert 'm_variables.insert(d.value(QStringLiteral("name"))' in runner
assert 'TestSequenceConditions::compare' in runner
assert 'm_measurements.at(n)' in runner and 'm_steps.at(index)' in runner
assert 'elseGuards.append(' in expander and 'thenGuards.append(' in expander
assert 'Maximum loop nesting depth is 3' in expander
assert 'Expanded sequence exceeds %1 executable steps' in expander
assert 'ctx.variables=definition.value(QStringLiteral("variables")).toArray()' in expander
assert 'TestSequenceConditions::validateVariables' in expander
assert 'Condition data is unavailable (not false)' in header
assert 'm_conditionDetails=new QWidget' in editor
assert 'm_variablesTable=new QTableWidget' in editor
assert 'testSequenceAddIf' in editor and 'testSequenceAddElse' in editor
assert 'Select a preceding step ID' in header
assert 'WFScriptTestConditionalRC1514Smoke' in cmake
assert 'WFScriptTestSequenceEditorRC1514Smoke' in cmake
p=root/'development/conditional_rc1514/WFScript_Conditional_Flow_Lab_RC1_5_14.wfpanel'
d=json.loads(p.read_text(encoding='utf-8'))
t=d['testDefinition'];assert len(t['variables'])==3
assert t['steps'][1]['condition']['source']=='variable'
assert t['steps'][2]['loop']['parameter']=='voltage'
assert t['steps'][2]['steps'][1]['condition']['source']=='measurement'
assert t['steps'][2]['steps'][1]['steps'][1]['otherwise'] is True
assert 'TestRunner.setVariable(' in d['libraries'][0]['source']
assert d['testLibrary']==d['libraries'][0]['name']
print('PASS RC1.5.14: condition source/guard, variable catalog, IF/ELSE editor, tests and standalone example')
