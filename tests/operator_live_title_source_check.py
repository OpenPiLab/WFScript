#!/usr/bin/env python3
"""v8.9.8.2: operator project identity and current Page display contract."""
from pathlib import Path
from _panel_inventory import catalogued_example_panels
import json
import re
root = Path(__file__).resolve().parents[1]
cpp = (root / 'src/MainWindow.cpp').read_text(encoding='utf-8')
hdr = (root / 'src/MainWindow.h').read_text(encoding='utf-8')
native = (root / 'tests/panel_operator_view_smoke.cpp').read_text(encoding='utf-8')
for name in ('operatorPanelDisplayName', 'refreshOperatorTitle'):
    assert 'MainWindow::'+name in cpp and name in hdr
section=cpp.split('void MainWindow::refreshOperatorTitle() {',1)[1].split('void MainWindow::refreshPageTabs() {',1)[0]
for value in ('m_panelProject.activePageId', 'page->name.trimmed()',
              'operatorPanelDisplayName()', 'm_operatorTitle->setText(title)',
              'm_operatorTitle->setToolTip(title)'):
    assert value in section,value
for banned in ('Wavegen1', 'm_panelRuntime->start(', 'm_panelRuntime->stop(',
               'm_panelProject.activePageId =', 'saveFile(', 'm_panelDesigner->setRunning('):
    assert banned not in section,banned
refresh=cpp.split('void MainWindow::refreshPageTabs() {',1)[1].split('bool MainWindow::showPanelPage(',1)[0]
assert 'refreshOperatorTitle();' in refresh
entry=cpp.split('void MainWindow::enterOperatorView() {',1)[1].split('void MainWindow::leaveOperatorView()',1)[0]
assert 'refreshOperatorTitle();' in entry
assert 'pages.first().name' not in entry
for value in ('Getting Started / Guide','Getting Started / Events Tutorial',
              'pageTabs->setCurrentIndex(1)', '!panel->running()'):
    assert value in native,value
# No schema or .wfpanel migration: use the existing scriptName / path and active ID.
assert 'm_panelProject.scriptName' in cpp and 'm_panelProject.path' in cpp
panels=catalogued_example_panels(root)
assert len(panels)==40
for panel in panels:
    o=json.loads(panel.read_text(encoding='utf-8'))
    assert o.get('scriptName')
print('PASS: Operator title = readable Panel name / current selected Page, live Qt regression included')
