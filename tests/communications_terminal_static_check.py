#!/usr/bin/env python3
"""Focused source test for a transport-agnostic widget; no Qt SDK needed."""
from pathlib import Path
import json,re,sys
root=Path(__file__).resolve().parents[1]
factory=(root/'src/widgets/CommunicationsTerminalWidgetFactory.cpp').read_text()
header=(root/'src/widgets/CommunicationsTerminalWidgetFactory.h').read_text()
widget=(root/'src/widgets/CommunicationsTerminalWidget.cpp').read_text()
reg=(root/'src/widgets/BuiltinWidgets.cpp').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'communications_terminal' in header
assert 'sendRequested' in header and 'sendRequested' in factory
assert 'CommunicationsTerminalWidgetFactory' in reg
assert 'CommunicationsTerminalWidget.cpp' in cmake
assert 'WFScriptCommunicationsTerminalSmoke' in cmake
# Ensure BOTH combo controls use explicit popup views, colours and per-row ink.
for identifier in ['commTerminalViewModePopup', 'commTerminalSendModePopup']:
 assert identifier in widget,identifier
for token in ['applyTerminalComboContrast(m_viewMode)', 'applyTerminalComboContrast(m_sendMode)',
              'QPalette::Text', 'QPalette::Base', 'Qt::ForegroundRole',
              'QListView::item{background:', 'QListView::item:hover', 'selection-background-color:']:
 assert token in widget,token
assert 'maximumBlockCount' in widget or 'setMaximumBlockCount' in widget
for token in ['displayMode','maxLines','previewBytes','showTimestamp','Hex bytes','Text + LF','sendRequested','QRegularExpression','toUtf8','readOnly','RX %1 B']:
 if token=='readOnly': assert 'setReadOnly(true)' in widget
 else: assert token in widget+factory+header,token
pan=json.loads((root/'examples/panels/CommunicationsTerminalLab.wfpanel').read_text())
assert pan['script']==(root/'examples/source/CommunicationsTerminalLab.js').read_text()
assert len(pan['pages'])==3
for p in pan['pages']:
 assert p['width']>=1000 and p['height']>=740
 assert len([c for c in p['controls'] if c['type']=='communications_terminal'])==1
 assert all(set(('id','type','text','x','y','width','height','properties','events'))<=set(c) for c in p['controls'])
assert all('CommunicationsTerminalLab' in f.read_text() for f in [root/'src/resources/examples.qrc',root/'examples/catalog.json'])
assert 'if(type==QStringLiteral("communications_terminal"))' in (root/'src/PanelDesigner.cpp').read_text()
print('PASS communications terminal factory, docs, event, properties, example and build/source inclusion')
