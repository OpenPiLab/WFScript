#!/usr/bin/env python3
"""No-hardware contract for reversible, non-destructive operator presentation.

This complements the native WFScriptOperatorViewSmoke (Qt offscreen) test.
It intentionally checks source structure, not the visual result on Windows.
"""
from pathlib import Path
from _panel_inventory import catalogued_example_panels
import json
import re

root=Path(__file__).resolve().parents[1]
main=(root/'src/MainWindow.cpp').read_text(encoding='utf-8')
header=(root/'src/MainWindow.h').read_text(encoding='utf-8')
designer=(root/'src/PanelDesigner.cpp').read_text(encoding='utf-8')
cmake=(root/'CMakeLists.txt').read_text(encoding='utf-8')

for needle in ('operatorHeader','operatorStop','operatorConsoleToggle',
               'operatorFitPage','operatorFullScreenAction',
               'operatorAutoOnRunAction','operatorF11Shortcut',
               'showFullScreen();','showNormal();','restoreGeometry(',
               'm_mainSplitter->setSizes(', 'm_operatorSavedSplitterSizes',
               'QShortcut(QKeySequence(Qt::Key_Escape)',
               'm_operatorViewActive)leaveOperatorView()',
               'QSettings(QStringLiteral("WFScript")',
               'm_panelDesigner->setOperatorView(true)',
               'm_panelDesigner->setOperatorView(false)'):
    assert needle in main, f'Missing operator interaction: {needle}'
for needle in ('void MainWindow::enterOperatorView()',
               'void MainWindow::leaveOperatorView()',
               'void MainWindow::stopPanel()',
               'void MainWindow::setOperatorConsoleVisible(bool visible)',
               'void MainWindow::setOperatorError(const QString& detail)'):
    assert needle in main

operator_enter=main.split('void MainWindow::enterOperatorView() {',1)[1].split('void MainWindow::leaveOperatorView() {',1)[0]
operator_leave=main.split('void MainWindow::leaveOperatorView() {',1)[1].split('void MainWindow::toggleOperatorView() {',1)[0]
for section in (operator_enter,operator_leave):
    for forbidden in ('m_panelRuntime->start(', 'm_panelRuntime->stop(',
                      'm_panelDesigner->setRunning(', 'm_panelProject.saveFile(',
                      'm_panelProject.pages.clear(', 'Wavegen1', 'DwfRuntime'):
        assert forbidden not in section, f'Operator display must not change runtime or hardware: {forbidden}'
assert 'm_operatorConsoleDock->show();' in main
assert 'm_operatorConsoleDock->hide();' in main
assert 'm_operatorConsoleOutput->setDocument(m_console->document());' in main
assert 'if(!m_operatorViewActive)m_problemsDock->show();' in main
assert 'm_panelDesigner->setRunning(false);' in main
assert 'updateOperatorHeader(QStringLiteral("STOPPING"));' in main
assert 'm_operatorAutoAction && m_operatorAutoAction->isChecked()' in main
assert 'void PanelDesigner::setOperatorView(bool enabled)' in designer
assert 'm_editorToolbar->setVisible(!enabled);' in designer
assert 'm_canvas->setPresentationScale(scale);' in designer
assert 'setOperatorView(bool enabled)' in (root/'src/PanelDesigner.h').read_text()
assert 'bool m_operatorViewActive = false;' in header
assert 'WFScriptOperatorViewSmoke' in cmake

panels=catalogued_example_panels(root)
assert len(panels)==40, len(panels)
for p in panels:
    data=json.loads(p.read_text(encoding='utf-8'))
    assert data.get('pages') or data.get('controls')
print('PASS: operator source contract, exit/stop/error/console rules, 40 panels structurally valid')
