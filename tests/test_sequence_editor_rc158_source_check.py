"""RC1.5.8 modeless editor and co-save contract, no Qt required."""
from pathlib import Path
import json
p=Path(__file__).resolve().parents[1]
ui=(p/'src/TestSequenceEditor.cpp').read_text()
main=(p/'src/MainWindow.cpp').read_text()
header=(p/'src/TestSequenceEditor.h').read_text()
run=(p/'src/RunInformationEditor.cpp').read_text()
cmake=(p/'CMakeLists.txt').read_text()
assert 'setWindowModality(Qt::NonModal)' in ui
assert 'showMaximized()' in ui
assert 'testSequenceDetailsColumns' in ui and 'testSequenceExecutionGroup' in ui
assert 'testSequenceAdvancedToggle' not in ui and 'testSequenceMainSplitter' not in ui
assert ui.index('detailsScroll->setWidget(detailsPanel)')<ui.index('editLayout->addWidget(m_table,1)')
assert 'testSequenceLibrarySourceEditor' in ui
assert 'testSequenceOpenProcedureCode' in ui and 'testSequenceOpenPollCode' in ui
assert 'testSequenceOpenFinishCode' in ui
assert 'void TestSequenceEditor::openFunction(' in ui
assert 'QRegularExpression::escape(name)' in ui
assert 'm_saveHandler(edited,runFields,policy,library)' in ui
assert 'm_runEditor->applyFields(&error)' in ui and 'bool RunInformationEditor::applyFields' in run
for tab in ('Step Details','Run Information','Report Storage','Sequence Settings','Test Library JavaScript','JSON Preview (read-only)'):
    assert f'StringLiteral("{tab}")' in ui
assert 'editor->setAttribute(Qt::WA_DeleteOnClose)' in main
assert 'editor->setSaveHandler(' in main and 'editor->show()' in main
assert 'editor.exec()' not in main[main.index('void MainWindow::editTestSequence()'):main.index('void MainWindow::exportReadablePanelSources()')]
assert 'm_testSequenceEditor' in main and 'm_testSequenceEditor' in (p/'src/MainWindow.h').read_text()
assert 'if(m_testSequenceEditor)' in main[main.index('void MainWindow::runPanel()'):main.index('void MainWindow::stopPanel()')]
assert 'if(m_testSequenceEditor)' in main[main.index('bool MainWindow::savePanelProjectToFile('):main.index('void MainWindow::savePanelProject()')]
assert 'source' in main[main.index('editor->setSaveHandler('):main.index('editor->show()')]
assert 'WFScriptTestSequenceEditorRC158Smoke' in cmake
panel=json.loads((p/'development/rigol_v066/Rigol_Four_Instrument_Operator_Bench_v0_6_6_Readable_Tests.wfpanel').read_text())
assert len(panel['testDefinition']['steps'])==62
assert panel['testLibrary']=='RigolTestSteps.mjs'
print('PASS RC1.5.8: modeless editor, full-width table, 3 columns, tabs, code navigation and save guards')
