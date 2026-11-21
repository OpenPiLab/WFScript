"""RC1.5.7 editor presentation / schema invariants, runnable without Qt."""
from pathlib import Path
import json
import re
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/'src/TestSequenceEditor.cpp').read_text(encoding='utf8')
header=(ROOT/'src/TestSequenceEditor.h').read_text(encoding='utf8')
cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf8')
assert 'void TestSequenceEditor::showEvent(QShowEvent* event)' in src
assert 'showMaximized()' in src and 'm_firstShow=false;' in src
assert 'm_table->setColumnCount(7)' in src
assert 'QString procedureSummary(const QJsonObject& step)' in src
assert 'QString acceptanceSummary(const QJsonObject& step)' in src
assert 'step.value(QStringLiteral("description"))' in src
assert 'const QString start=handlers.value(QStringLiteral("start")).toString();' in src
assert 'return start+QStringLiteral("()");' in src
assert 'QStringLiteral("Poll: %1()")' in src  # explicit fallback only for poll-only cases
assert 'QStringLiteral("Limits")' in src
assert 'QStringLiteral("JSON Preview (read-only)")' in src
assert 'm_tabs->addTab(m_preview,' in src
assert 'm_preview->setMaximumHeight(' not in src
assert 'm_stepOverview->setText(' in src
assert 'm_description->toPlainText()' in src
assert 'm_description->setPlainText(' in src
assert 'm_start->setObjectName(QStringLiteral("testSequenceStartHandler"))' in src
assert 'm_poll->setObjectName(QStringLiteral("testSequencePollHandler"))' in src
assert 'testSequenceExecutionGroup' in src and 'testSequenceDetailsColumns' in src
assert 'testSequenceAdvancedToggle' not in src
assert 'testSequenceSettingsToggle' not in src
assert 'm_table->header()->setStretchLastSection(true)' in src
assert 'm_tabs->addTab(editTab,QStringLiteral("Step Details"))' in src
assert 'showPreview();m_tabs->setCurrentWidget(m_preview);' in src
assert 'QShowEvent* event' in header
assert 'WFScriptTestSequenceEditorUXSmoke' in cmake
assert 'tests/test_sequence_editor_rc157_smoke.cpp' in cmake
# The Rigol Panel is a regression fixture, not an edited application input.
panel=json.loads((ROOT/'development/rigol_v066/Rigol_Four_Instrument_Operator_Bench_v0_6_6_Readable_Tests.wfpanel').read_text(encoding='utf8'))
assert len(panel['testDefinition']['steps'])==62
steps=panel['testDefinition']['steps']
psu=next(s for s in steps if s['id']=='psu_500')
assert psu['handlers']=={'start':'power500','poll':'pollValue'}
assert psu['limits']=={'low':4.9,'high':5.1}
assert psu['description']
assert panel['testLibrary']=='RigolTestSteps.mjs'
print('PASS RC1.5.8: readable editor source, maximization, schema, and original 62-step Rigol Panel preserved')
