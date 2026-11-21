"""RC1.5.13 preview is read-only and driven by the existing native expander."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/TestSequenceEditor.cpp').read_text()
h=(root/'src/TestSequenceEditor.h').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'testSequenceOpenExecutionPreview' in s
assert 'm_executionTable=new QTableWidget' in s
assert 'm_executionTable->setEditTriggers(QAbstractItemView::NoEditTriggers)' in s
assert 'm_executionTable->setSortingEnabled(false)' in s
assert 'm_tabs->addTab(m_executionTab,QStringLiteral("Execution Preview"))' in s
assert 'm_executionTable->setHorizontalHeaderLabels' in s
assert 'TestSequenceLoops::expand(authored,&expanded,&error)' in s
assert 'step.value(QStringLiteral("templateId")).toString(id)' in s
assert 'selectedLeafIds.contains(templateId)' in s
assert 'displayRuleLimit(rule.value(QStringLiteral("low")))' in s
assert 'displayRuleLimit(rule.value(QStringLiteral("high")))' in s
assert 'm_executionSummary->setText' in s
assert 'void TestSequenceEditor::showExecutionPreview()' in s
assert 'void showExecutionPreview();' in h
assert 'WFScriptTestSequenceEditorRC1513Smoke' in cmake
# No replacement of execution logic or in-panel Rigol scripting is included in RC1.5.13.
assert 'TestRunner.complete(' not in s[s.index('void TestSequenceEditor::showExecutionPreview()'):s.index('void TestSequenceEditor::showPreview()')]
print('PASS RC1.5.13 execution preview UI, scope, expander reuse, resolved limits, and read-only contract')
