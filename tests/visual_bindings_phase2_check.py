#!/usr/bin/env python3
"""Phase 2 regressions for source-embedded, designer-owned UI event bindings.

Does not change the Panel format or require Qt. C++ parser smoke additionally
opens each embedded project and validates its event catalog at native runtime.
"""
import json
import re
from pathlib import Path
from _panel_inventory import catalogued_example_panels
root=Path(__file__).resolve().parents[1]
expected=json.loads((root/'docs/PHASE2_CONVERSION_MANIFEST.json').read_text())['examples']
assert len(expected)==15,expected
assert sum(expected.values())==78,expected
all_panels=list(p for p in catalogued_example_panels(root) if p.stem not in ('PanelDataLab','JavaScriptDSPStudio','ACNetworkLaboratory','RCStepResponseLaboratory'))
assert len(all_panels)==36
converted=0
for stem,number in expected.items():
    panel=json.loads((root/'examples/panels'/f'{stem}.wfpanel').read_text(encoding='utf-8'))
    src=(root/'examples/source'/f'{stem}.js').read_text(encoding='utf-8')
    assert panel['script']==src,stem
    assert panel['format']=='WFScriptPanel' and panel['version']==2,stem
    events=[]
    for page in panel['pages']:
        assert all(k in page for k in ('id','name','width','height','controls')),(stem,page)
        for control in page['controls']:
            for event,binding in control.get('events',{}).items():
                assert binding.get('enabled') is True,(stem,control['id'],event)
                assert isinstance(binding.get('handler'),str),(stem,control['id'],event)
                assert re.search(r'\bfunction\s+'+re.escape(binding['handler'])+r'\s*\(',src),(stem,control['id'],event,binding)
                assert not re.search(r'\bui\.on\s*\(\s*[\'\"]'+re.escape(control['id'])+r'[\'\"]\s*,\s*[\'\"]'+re.escape(event)+r'[\'\"]',src),(stem,control['id'],event)
                events.append((control['id'],event))
    assert len(events)==number,(stem,len(events),number)
    assert len(events)==len(set(events)),stem
    if stem=='StaticDigitalIO':
        assert re.search(r"\bui\.on\(\s*'dioSelect'\s*\+\s*i\s*,\s*'selectionChanged'", src)
        assert src.count('ui.on(')==1,stem
    else:
        assert 'ui.on(' not in src,(stem,'unexpected manual UI registration')
    converted+=number
    print(f'PASS {stem}: {number} designer events, zero duplicates, source synchronized')
assert converted==78
# Preserve the previous phase unchanged: 55 + 78 designer bindings in total.
phase1=['GettingStarted','WidgetPlayground','PanelTimers','SQLiteDataLab',
        'SerialCommsLab','TcpCommsLab','UdpCommsLab','CommunicationsTerminalLab']
old=sum(len(c.get('events',{})) for s in phase1 for p in json.loads((root/'examples/panels'/f'{s}.wfpanel').read_text())['pages'] for c in p['controls'])
assert old==55,old
print(f'PASS 15 new examples; {converted} new + {old} retained = {converted+old} bindings; 36 bundled projects')
