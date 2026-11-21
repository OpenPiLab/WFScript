#!/usr/bin/env python3
"""Preserve an edited Rigol Test Station v0.3.1 Panel and enable 100k virtual log rows.
Usage: python Upgrade_Rigol_Test_Station_v0_3_1_to_v0_3_2.py "C:\\path\\to\\MyRigolPanel.wfpanel"
Creates a sibling *_v0_3_2.wfpanel; input is never modified.
Requires WFScript RC1.5.14.3 for the expanded event_log widget property.
"""
import sys, pathlib, json
if len(sys.argv)!=2:raise SystemExit(__doc__)
p=pathlib.Path(sys.argv[1]).expanduser().resolve()
if not p.is_file() or p.suffix.lower()!='.wfpanel':raise SystemExit('Input must be an existing .wfpanel')
data=json.loads(p.read_text(encoding='utf8'))
if not isinstance(data.get('testDefinition'),dict):raise SystemExit('No project-owned testDefinition; refusing upgrade')
if not isinstance(data.get('pages'),list):raise SystemExit('No Panel pages')
if 'Rigol' not in data.get('name',''):raise SystemExit('Expected the Rigol Automated Test Station; refusing other Panels')
count=0
for page in data['pages']:
 for c in page.get('controls',[]):
  if c.get('type')=='event_log':
   c.setdefault('properties',{})['maxEntries']=100000
   count+=1
if not count:raise SystemExit('No event_log widget found')
data['name']='Rigol Automated Test Station v0.3.2 - Virtual Long-Sweep Results'
# Keep custom sweeps, endpoints, test libraries, scripts and report policy intact.
out=p.with_name(p.stem+'_v0_3_2.wfpanel')
if out.exists():raise SystemExit('Output already exists; not overwriting: '+str(out))
out.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Created:',out)
print('Retained edited test definition, scripts, hardware addresses and all controls.')
print('Event Log widgets upgraded:',count,'; maxEntries=100000')
print('Open this Panel only in WFScript RC1.5.14.3 or later.')
