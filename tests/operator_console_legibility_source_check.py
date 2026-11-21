#!/usr/bin/env python3
"""Offline safety net for v8.9.8.1; Windows Qt native smoke required."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = (root / 'src/MainWindow.cpp').read_text(encoding='utf-8')
native = (root / 'tests/panel_operator_dock_watch_smoke.cpp').read_text(encoding='utf-8')

section = source.split('void MainWindow::buildOperatorDocks()', 1)[1].split('void MainWindow::', 1)[0]
for required in (
    'configureLogPopup(m_operatorLogScope);',
    'configureLogPopup(m_operatorLogLevel);',
    'QAbstractItemView* popup=combo->view();',
    'palette.setColor(QPalette::Base,',
    'palette.setColor(QPalette::Text,',
    'palette.setColor(QPalette::Highlight,',
    'palette.setColor(QPalette::HighlightedText,',
    'popup->setStyleSheet(',
    'm_console->document()->setDefaultFont(consoleLogFont);',
    'm_operatorFilteredDocument->setDefaultFont(consoleLogFont);',
    'm_operatorConsoleOutput->setFont(consoleLogFont);',
    'm_operatorConsoleOutput->setDocument(m_console->document());',
):
    assert required in section, f'Missing Console legibility rule: {required}'
for required in ('combo->view()', 'palette.color(group,QPalette::Text)',
                 'console->document()->defaultFont()',
                 'consoleView->document()->defaultFont()',
                 'returning to This Run preserves Console log font'):
    assert required in native, f'Missing native verification: {required}'
# This is a rendering-only release; no script, SDK or runtime mutation.
for forbidden in ('m_panelRuntime->requestStop()', 'Wavegen1', 'DwfRuntime', 'QJSEngine'):
    assert forbidden not in section, forbidden
print('PASS: popup normal/hover contrast and consistent original/filtered Console document fonts')
