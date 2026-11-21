#!/usr/bin/env python3
"""Guard against treating QJsonArray iteration results as PanelControl objects.

The AC lab smoke test must compile under Qt6, and must still validate the
real notebook schema. This static test does not replace a native CTest build.
"""
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parents[1]
test=(ROOT/'tests/ac_network_lab_smoke.cpp').read_text(encoding='utf-8')
header=(ROOT/'src/PanelProject.h').read_text(encoding='utf-8')
assert 'QJsonArray controls;' in header, 'PanelPage::controls must remain JSON-backed'
assert 'for(const auto& value:page.controls)' in test
assert 'value.isObject()' in test
assert 'const QJsonObject control=value.toObject();' in test
assert 'const QJsonObject properties=control.value(QStringLiteral("properties")).toObject();' in test
for invalid in ('c.id','c.type','c.properties'):
    assert invalid not in test, f'stale struct member access: {invalid}'
for key in ('id','type','columns','maxRows'):
    assert f'StringLiteral("{key}")' in test
example=json.loads((ROOT/'examples/panels/ACNetworkLaboratory.wfpanel').read_text(encoding='utf-8'))
notebooks=[control for page in example['pages'] for control in page['controls'] if control.get('id')=='resultsTable']
assert len(notebooks)==1
notebook=notebooks[0]
assert notebook['type']=='experiment_table'
assert len(notebook['properties']['columns'].split('|'))==11
assert notebook['properties']['maxRows']==500
print('PASS RC1.5.14.10.3: AC lab native smoke reads QJsonObject and checks live notebook schema')
