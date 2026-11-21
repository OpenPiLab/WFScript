#!/usr/bin/env python3
"""v8.9.8 source-level checks; native behavior requires Windows Qt smoke."""
from pathlib import Path
from _panel_inventory import catalogued_example_panels
import json
root=Path(__file__).resolve().parents[1]
s=(root/'src/MainWindow.cpp').read_text(encoding='utf-8')
h=(root/'src/MainWindow.h').read_text(encoding='utf-8')
t=(root/'tests/panel_operator_dock_watch_smoke.cpp').read_text(encoding='utf-8')
for entry in ('operatorLogScope','operatorLogLevel','operatorLogWrap',
              'tabifyDockWidget(m_operatorConsoleDock,m_panelDataWatchDock)',
              'toggleOperatorDockFloating', 'saveOperatorDiagnosticsSettings()',
              'refreshOperatorConsole()', 'queueOperatorConsoleRefresh()',
              'm_operatorSessionMarker', 'QPlainTextDocumentLayout',
              'm_panelDataWatchHighlightUntil', 'm_panelDataWatchHighlightTimer',
              'onOperatorClose', 'Qt::RightDockWidgetArea'):
    assert entry in s,entry
assert 'm_operatorConsoleOutput->setDocument(m_console->document());' in s
assert 'm_operatorConsoleOutput->setDocument(m_operatorFilteredDocument);' in s
assert 'm_console->clear();' not in s[s.index('void MainWindow::refreshOperatorConsole()'):s.index('void MainWindow::buildOperatorDocks()')]
assert 'm_panelDataWatchValues.insert(name,value);' in s
assert 'm_panelDataWatchTree->setEditTriggers(QAbstractItemView::NoEditTriggers)' in s
assert 'm_operatorRestoringDocks=true;' in s
assert 'saveOperatorDiagnosticsSettings();' in s[s.index('void MainWindow::leaveOperatorView()'):]
for key in ('consoleFloating','watchFloating','consoleGeometry','watchGeometry','rightWidth','logScope','logLevel','wordWrap'):
    assert 'QStringLiteral("'+key+'")' in s,key
for gui_check in ('tabifiedDockWidgets(consoleDock)', 'operatorLogScope', 'operatorLogLevel',
                  'operatorLogWrap', 'background(2)', 'operator/diagnostics/rightWidth'):
    assert gui_check in t,gui_check
# Run / worker data path stays strictly queued, do not introduce cross-thread JS observation.
assert 'connect(worker,&PanelRuntime::panelDataUpdate,this' in s
for section_name in ('void MainWindow::refreshOperatorConsole()',
                     'void MainWindow::saveOperatorDiagnosticsSettings()',
                     'void MainWindow::toggleOperatorDockFloating(',
                     'void MainWindow::flushPanelDataWatch()'):
    section=s.split(section_name,1)[1].split('\n}\n',1)[0]
    for forbidden in ('m_panelRuntime->requestStop()', 'Wavegen1', 'DwfRuntime', 'QJSEngine', 'PanelProject::saveFile'):
        assert forbidden not in section,(section_name,forbidden)
assert len(catalogued_example_panels(root))==40
for panel in catalogued_example_panels(root):
    json.loads(panel.read_text(encoding='utf-8'))
print('PASS: tabbed diagnostics, log scope/filter/wrap, watch highlight, saved user layout, runtime isolation')
