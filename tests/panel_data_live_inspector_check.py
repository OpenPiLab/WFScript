#!/usr/bin/env python3
"""Offline v0.2-dev.8.1 project/inspector contract checks.

Source-level regression guards complement the native Qt GUI test in
WFScriptStableWorkspaceSmoke. This does not substitute for the Windows build.
"""
from pathlib import Path
import json
import re

root=Path(__file__).resolve().parents[1]
panel=json.loads((root/'examples/panels/PanelDataLab.wfpanel').read_text(encoding='utf8'))
script=(root/'examples/source/PanelDataLab.js').read_text(encoding='utf8')
assert panel['script']==script
assert len(panel['pages'])==1 and panel['activePageId']==panel['pages'][0]['id']
page=panel['pages'][0]
controls={c['id']:c for c in page['controls']}
assert len(controls)==19 and len(panel['panelData'])==7
bindings={id:c['bindings']['value'] for id,c in controls.items() if c.get('bindings')}
assert len(bindings)==11
assert {bindings[k] for k in ('speedDisplay','speedDial','speedGauge')}=={'Motor.ActualRPM'}
for id in ('startButton','targetInput','loadSlider','readySwitch','speedDisplay','speedDial','speedGauge','targetReadout'):
    assert id in controls, f'missing {id}'
    c=controls[id]
    assert c['x']>=0 and c['y']>=0
    assert c['x']+c['width']<=page['width'] and c['y']+c['height']<=page['height']
# The editable controls and corresponding readouts are in distinct horizontal columns,
# allowing changes to be observed without navigating or vertical scrolling.
assert controls['targetInput']['x']+controls['targetInput']['width'] < controls['speedDisplay']['x']
assert controls['startButton']['x'] < controls['speedDial']['x']
assert sum(len(c.get('events',{})) for c in controls.values())==5
cpp=(root/'src/MainWindow.cpp').read_text(encoding='utf8')
editor=(root/'src/PanelDesigner.cpp').read_text(encoding='utf8')
assert 'm_panelProject.panelData=definitions;\n' in cpp
assert 'm_panelDesigner->setPanelData(definitions);' in cpp
assert 'void PanelDesigner::setPanelDataRuntimeValue(' in editor
assert 'm_runtimeData.insert(name,value);' in editor
assert 'QTreeWidgetItem* row=m_dataTree->topLevelItem(i);' in editor
assert 'if(row->text(2)!=display)row->setText(2,display);' in editor
assert 'QStringLiteral("Live Value")' in editor and 'QStringLiteral("Initial Value")' in editor
native=(root/'tests/stable_workspace_smoke.cpp').read_text(encoding='utf8')
assert 'Demo.NewVoltage' in native and 'Demo.Status' in native
assert 'panel->panelDataEdited(definitions);' in native
assert 'live updates preserve selection and ignore undefined names' in native
print('PASS: single-Page, 19-widget, seven-variable, 11-binding motor lab and new-variable inspector guards')
