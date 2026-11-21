#!/usr/bin/env python3
"""Check built-in Panel page IDs with the rule used by PanelProject::validPageId().

This guards against examples that pass JSON/Node tests but cannot be loaded
because a page ID begins with a numeral or is repeated.
"""
import collections
import json
import pathlib
import re

root=pathlib.Path(__file__).resolve().parents[1]
source=(root/'src/PanelProject.cpp').read_text(encoding='utf-8')
rule=r'^[A-Za-z_][A-Za-z0-9_-]{0,63}$'
assert rule in source, 'Native page-ID validation has changed; update this test'
pattern=re.compile(rule)
count=0
for file in sorted((root/'examples/panels').glob('*.wfpanel')):
    project=json.loads(file.read_text(encoding='utf-8'))
    pages=project['pages']
    assert 1 <= len(pages) <= 64, f'{file.name}: invalid page count'
    ids=[page['id'] for page in pages]
    names=[page['name'].strip().casefold() for page in pages]
    assert all(pattern.fullmatch(id) for id in ids),f'{file.name}: invalid page IDs {ids}'
    assert len(set(ids))==len(ids),f'{file.name}: duplicate page IDs'
    assert all(name for name in names) and len(set(names))==len(names),f'{file.name}: duplicate or empty names'
    assert project['activePageId'] in ids,f'{file.name}: invalid activePageId'
    widget_ids=[control['id'] for page in pages for control in page.get('controls',[])]
    assert len(set(widget_ids))==len(widget_ids),f'{file.name}: widget IDs reused across pages'
    count+=1
assert count>=40, f'Unexpected number of examples: {count}'
assert json.loads((root/'examples/panels/RCStepResponseLaboratory.wfpanel').read_text(encoding='utf-8'))['pages'][6]['id']=='method63'
print(f'PASS native page-ID/page-name/active-ID/widget-ID checks: {count} examples')
