#!/usr/bin/env python3
"""Source-level contracts for docked diagnostics and GUI-thread live Panel Data.

This does not replace the Qt6 offscreen integration test on Windows.
"""
from pathlib import Path
from _panel_inventory import catalogued_example_panels
import json

root=Path(__file__).resolve().parents[1]
source=(root/'src/MainWindow.cpp').read_text(encoding='utf-8')
header=(root/'src/MainWindow.h').read_text(encoding='utf-8')
cmake=(root/'CMakeLists.txt').read_text(encoding='utf-8')

for name in ['operatorConsoleDock','operatorConsoleOutput','operatorDataWatchDock',
             'operatorDataWatchTree','operatorDataWatchToggle','operatorDataWatchFilter',
             'operatorDataWatchPause','operatorConsoleFloatToggle',
             'operatorDataWatchFloatToggle','operatorDataWatchAction',
             'setOperatorConsoleVisible(bool visible)','setOperatorWatchVisible(bool visible)',
             'resetPanelDataWatch()','setPanelDataWatchValue(',
             'flushPanelDataWatch()','filterPanelDataWatch()']:
    assert name in source, name

assert 'm_operatorConsoleOutput->setDocument(m_console->document());' in source
assert 'QDockWidget::DockWidgetFloatable' in source
assert 'QDockWidget::DockWidgetMovable' in source
assert 'QDockWidget::DockWidgetClosable' in source
assert 'dock->setFloating(true)' in source
assert 'toggleOperatorDockFloating(m_operatorConsoleDock,true);' in source
assert 'toggleOperatorDockFloating(m_panelDataWatchDock,false);' in source
assert 'm_panelDataWatchTree->setEditTriggers(QAbstractItemView::NoEditTriggers)' in source
assert 'QTimer::singleShot(100,this,' in source
assert 'PanelDataSchema::initialValue(definition)' in source
assert 'm_panelDataWatchValues.insert(name,value);' in source
assert 'setPanelDataWatchValue(name,value);' in source
assert 'Qt::QueuedConnection' in source
assert 'if(!m_panelThread || m_panelRunGeneration!=panelGeneration)return;' in source
assert 'm_panelProject.panelData' in source
assert 'm_panelDataWatchRows.contains(name)' in source
# New tools must not change the run worker's entry or lifetime.
for fn in ['void MainWindow::buildOperatorDocks() {',
           'void MainWindow::resetPanelDataWatch() {',
           'void MainWindow::setPanelDataWatchValue(',
           'void MainWindow::flushPanelDataWatch() {']:
    body=source.split(fn,1)[1].split('\n}\n',1)[0]
    for forbidden in ['Wavegen1','DwfRuntime','PanelRuntime::start',
                      'QJSEngine','m_panelRuntime->requestStop()','PanelProject::saveFile']:
        assert forbidden not in body,(fn,forbidden)
for name in ('m_operatorConsoleDock','m_panelDataWatchDock','m_panelDataWatchTree'):
    assert name in header
assert 'WFScriptOperatorDockWatchSmoke' in cmake
panels=catalogued_example_panels(root)
assert len(panels)==40
for path in panels:
    json.loads(path.read_text(encoding='utf-8'))
print('PASS: shared log, reversible floating docks, read-only watch, queued Panel Data, 40 untouched panels')
