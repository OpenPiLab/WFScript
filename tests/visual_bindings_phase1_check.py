#!/usr/bin/env python3
"""Offline validation: true project-owned UI bindings, exact once, and source sync."""
from pathlib import Path
from _panel_inventory import catalogued_example_panels
import json,re
root=Path(__file__).resolve().parents[1]
expected={
 'GettingStarted':1,'WidgetPlayground':8,'PanelTimers':10,
 'SQLiteDataLab':8,'SerialCommsLab':6,'TcpCommsLab':4,
 'UdpCommsLab':4,'CommunicationsTerminalLab':14,
}
all_panels=sorted(p for p in catalogued_example_panels(root) if p.stem not in ('PanelDataLab','JavaScriptDSPStudio','ACNetworkLaboratory','RCStepResponseLaboratory'))
assert len(all_panels)==36,len(all_panels)
count=0
for stem,n in expected.items():
 p=json.loads((root/'examples/panels'/f'{stem}.wfpanel').read_text(encoding='utf8'))
 js=(root/'examples/source'/f'{stem}.js').read_text(encoding='utf8')
 assert p['script']==js,stem
 assert p.get('version')==2 and p['pages'],stem
 controls=[c for pg in p['pages'] for c in pg['controls']]
 ids=[c['id'] for c in controls]
 assert len(ids)==len(set(ids)),stem
 actual=0
 for c in controls:
  for ev,b in c.get('events',{}).items():
   assert b=={'handler':b['handler'],'enabled':True},(stem,c['id'],ev)
   assert re.search(r'\bfunction\s+'+re.escape(b['handler'])+r'\s*\(',js),(stem,c['id'],b)
   assert not re.search(r'''\bui\.on\s*\(\s*['"]'''+re.escape(c['id'])+r'''['"]\s*,\s*['"]'''+re.escape(ev)+r'''['"]''',js),(stem,c['id'],ev)
   assert c['type'] in ('button','rotary_input','numeric_stepper','slider','switch_button','momentary_button','option_selector','communications_terminal'),(stem,c['type'])
   actual+=1
 assert actual==n,(stem,actual,n)
 if stem!='GettingStarted':
  # These phase-one examples should no longer register any UI events manually.
  assert not re.search(r'''\bui\.on\s*\(\s*['"]''',js),stem
 count+=actual
 # Fatal regression from v0.2-dev.3: missing required Page dimensions.
 for pg in p['pages']:
  assert all(k in pg for k in ('id','name','width','height','controls')),(stem,pg.get('id'))
 print(f'PASS {stem}: {actual} visible designer assignments, no duplicated ui.on')
assert count==55,count
p=json.loads((root/'examples/panels/GettingStarted.wfpanel').read_text(encoding='utf8'))
assert len(p['pages'])==2 and p['pages'][1]['name']=='Events Tutorial'
assert p['pages'][1]['controls'][2]['events']['clicked']['handler']=='onTourButtonClicked'
# The other examples are counted here; later phases may convert them.
untouched=[p.stem for p in all_panels if p.stem not in expected]
assert len(untouched)==28
print(f'PASS 55 Phase 1 events, 8 original Phase 1 projects, {len(untouched)} other examples')
