#!/usr/bin/env python3
"""Source-level integration safety guard for the computed Panel Data release."""
from pathlib import Path
import json
r=Path(__file__).resolve().parents[1]
js=(r/'src/PanelDataEvents.h').read_text()
bridge=(r/'src/PanelUiBridge.h').read_text()
cmake=(r/'CMakeLists.txt').read_text()
example=json.loads((r/'examples/panels/PanelDataLab.wfpanel').read_text())
assert example['script']==(r/'examples/source/PanelDataLab.js').read_text()
assert len(example['pages'])==1 and len(example['pages'][0]['controls'])==19
assert len(example['panelData'])==7
assert any(d['name']=='Motor.TargetRPS' and d['type']=='number' for d in example['panelData'])
display=next(c for c in example['pages'][0]['controls'] if c['id']=='targetReadout')
assert display['bindings']['value']=='Motor.TargetRPS'
assert "Data.computed('Motor.TargetRPS'" in example['script']
assert 'isComputed: function(name)' in js
assert '__dataRecomputeAfter' in js and '__dataEvaluating' in js
assert 'circular dependency' in js and 'Data.computed limit' in js
assert 'validateComputedValue' in bridge and 'registerComputedData' in bridge
assert 'setComputedData' in bridge and 'm_computedNames.contains(name)' in bridge
assert 'm_computedNames.contains(name))return false;' in bridge
assert 'PanelDataEvents::bootstrapSource()' in (r/'src/PanelRuntime.cpp').read_text()
assert 'WFScriptPanelDataComputedSmoke' in cmake
assert 'tests/panel_data_computed_smoke.cpp' in cmake
assert '"onChange","offChange","computed","isComputed"' in (r/'src/CompletionCatalog.cpp').read_text()
for name in ('Data.computed','Data.isComputed'):
    assert name in (r/'src/EditorAssist.cpp').read_text()
    assert name in (r/'src/ApiKnowledge.cpp').read_text()
assert 'setWindowTitle(QStringLiteral("WFScript v0.2"))' in (r/'src/MainWindow.cpp').read_text()
print('PASS: v8.5 project, runtime, native bridge, CTest, docs, help/completion and 19-widget single-page lab')
