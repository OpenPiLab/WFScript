#!/usr/bin/env python3
"""Phase 3 project-owned UI bindings audit, including earlier phases.

New v8.9 AC Lab is independently checked by the AC Network Lab regression.
"""
from pathlib import Path
from _panel_inventory import catalogued_example_panels
import json,re
root=Path(__file__).resolve().parents[1]
panels=root/'examples/panels'
expected=json.loads((root/'docs/PHASE3_CONVERSION_MANIFEST.json').read_text())['examples']
assert len(expected)==12,expected
assert sum(expected.values())==96,expected
stems=sorted(p.stem for p in catalogued_example_panels(root) if p.stem not in ('PanelDataLab','JavaScriptDSPStudio','ACNetworkLaboratory','RCStepResponseLaboratory'))
assert len(stems)==36
bound_total=0
per={}
for stem in stems:
    p=json.loads((panels/f'{stem}.wfpanel').read_text(encoding='utf-8'))
    script=(root/'examples/source'/f'{stem}.js').read_text(encoding='utf-8')
    assert p['script']==script,stem
    assert p['version']==2 and p['format']=='WFScriptPanel'
    ctrls=[c for page in p['pages'] for c in page['controls']]
    assert len({c['id'] for c in ctrls})==len(ctrls),stem
    found=[]
    for c in ctrls:
        for event,info in c.get('events',{}).items():
            assert info.get('enabled') is True,(stem,c['id'],event)
            fn=info.get('handler')
            assert fn and re.search(r'\bfunction\s+'+re.escape(fn)+r'\s*\(',script),(stem,c['id'],event,fn)
            assert not re.search(r'''\bui\.on\s*\(\s*['"]'''+re.escape(c['id'])+r'''['"]\s*,\s*['"]'''+re.escape(event)+r'''['"]''',script),(stem,c['id'],event)
            found.append((c['id'],event))
    assert len(found)==len(set(found)),stem
    if stem in expected:
        assert len(found)==expected[stem],(stem,len(found),expected[stem])
        assert not re.search(r'\bui\.on\s*\(',script),(stem,'manual UI registration')
    elif stem=='StaticDigitalIO':
        assert script.count('ui.on(')==1 and re.search(r"\bui\.on\(\s*'dioSelect'\s*\+\s*i\s*,\s*'selectionChanged'", script)
    elif stem=='EducationalVisuals':
        assert not found
    else:
        # Ignore prose comments mentioning ui.on(), but detect real literal registrations.
        assert not re.search(r"\bui\.on\s*\(\s*['\"]",script),stem
    per[stem]=len(found)
    bound_total+=len(found)
    if stem in expected:print(f'PASS {stem}: {len(found)} new visual bindings, no duplicates')
assert bound_total==229,bound_total
assert sum(per[k] for k in expected)==96
assert sum(per[k] for k in stems if k not in expected)==133
print(f'PASS: 36 example projects, 229 saved designer bindings, 16 intentionally dynamic DIO events, zero duplicate registrations')
