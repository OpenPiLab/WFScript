#!/usr/bin/env python3
"""Check native input event ordering and non-regression without a Qt SDK."""
from pathlib import Path
import json,re
r=Path(__file__).resolve().parents[1]
source=(r/'src/PanelRuntime.cpp').read_text()
fn=source.split('void PanelRuntime::eventFromPanel',1)[1].split('void PanelRuntime::cleanup',1)[0]
assert 'PanelDataEvents::bootstrapSource()' in source
assert 'PanelDataEvents.h' in (r/'CMakeLists.txt').read_text()
assert 'dataCallbackFailed' in source
assert 'QStringLiteral("Data.onChange(%1)")' in source
assert fn.index('setDataFromInput(id,name,value)') < fn.index('property(QStringLiteral("__panelDataDispatch"))')
assert fn.index('property(QStringLiteral("__panelDataDispatch"))') < fn.index('property("__panelDispatch")')
assert fn.index('property(QStringLiteral("__panelDataDispatch"))') < fn.index('m_managedHandlers.value(')
assert 'currentData!=previousData' in fn
bridge=(r/'src/PanelUiBridge.h').read_text()
assert 'dataValue(const QString& name)' in bridge
assert 'emit dataCallbackFailed(name,error)' in bridge
assert 'if(m_dataValues.value(name)==value)return true;' in bridge
assert 'emit dataValueChanged(name,value);' in bridge
assert 'src/PanelDataEvents.h' in (r/'CMakeLists.txt').read_text()
assert 'WFScriptPanelDataEventsSmoke' in (r/'CMakeLists.txt').read_text()
example=json.loads((r/'examples/panels/PanelDataLab.wfpanel').read_text())
assert example['script']==(r/'examples/source/PanelDataLab.js').read_text()
assert example['script'].count('Data.onChange(')>=2
assert example['script'].count('Data.offChange(')>=2
assert len(example['pages'])==1 and len(example['panelData'])==7
assert len(example['pages'][0]['controls'])==19
assert 'Data.onChange' in (r/'src/EditorAssist.cpp').read_text()
assert 'Data.onChange' in (r/'src/ApiKnowledge.cpp').read_text()
assert '"onChange","offChange"' in (r/'src/CompletionCatalog.cpp').read_text()
assert 'setWindowTitle(QStringLiteral("WFScript v0.2"))' in (r/'src/MainWindow.cpp').read_text()
print('PASS: event dispatch ordering, diagnostics, no-op dedup, production JS, catalog/F1, 19-widget expanded Lab')
