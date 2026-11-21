"""RC1.5.11: compact test plans, bounded expansion and physical Rigol regression contract."""
from pathlib import Path
import json
import re
import hashlib

root=Path(__file__).resolve().parents[1]
new=json.loads((root/'development/rigol_v069/Rigol_Four_Instrument_Operator_Bench_v0_6_9_Sweeps_and_Loops.wfpanel').read_text(encoding='utf-8'))
old=json.loads((root/'development/rigol_v068/Rigol_Four_Instrument_Operator_Bench_v0_6_8_Structured_Results.wfpanel').read_text(encoding='utf-8'))
preview=json.loads((root/'development/rigol_v069/expanded_reference.json').read_text(encoding='utf-8'))
lab=json.loads((root/'development/loop_lab/WFScript_Test_Executive_Sweeps_Loops_Lab_RC1_5_11.wfpanel').read_text(encoding='utf-8'))
assert len(new['testDefinition']['steps'])==7
assert len(preview['steps'])==len(old['testDefinition']['steps'])==38
assert sum(3 if x.get('measurements') else 1 if x.get('type')=='numeric' else 0 for x in preview['steps'])==50
assert len(lab['testDefinition']['steps'])==5
for i in range(4):assert new['pages'][i]==old['pages'][i],f'Physical operator Page {i} changed'
for i in range(5):assert new['libraries'][i]==old['libraries'][i],f'Validated Rigol SCPI driver #{i} changed'
assert len(new['pages'])==5
assert len(new['libraries'])==6
old_handlers=[s.get('handler') or s.get('handlers',{}).get('start') for s in old['testDefinition']['steps']]
new_handlers=[s.get('handler') or s.get('handlers',{}).get('start') for s in preview['steps']]
assert old_handlers==new_handlers
for a,b in zip(preview['steps'],old['testDefinition']['steps']):
 assert a['mode']==b['mode'] and a['type']==b['type'] and a['group']==b['group']
 if 'parameters' in b:
  for key,value in b['parameters'].items():
   if key!='key':assert a['parameters'][key]==value
 if 'limits' in b:
  for key in ('low','high'):assert abs(a['limits'][key]-b['limits'][key])<1e-10
 if 'measurements' in b:
  for path in b['measurements']:
   for key in ('low','high'):
    assert abs(a['measurements'][path][key]-b['measurements'][path][key])<1e-10
assert new['testDefinition']['steps'][1]['loop']['increment']==-0.25
assert new['testDefinition']['steps'][3]['loop']['axes'][0]['values']==[1,2]
assert 'summary.passedMeasurements' in new['script'] and 'summary.failedMeasurements' in new['script']
assert 'summary.passedMeasurements' not in old['script']
assert "var key=p.key ||" in new['libraries'][5]['source']
assert 'var key=p.key ||' not in old['libraries'][5]['source']
assert all('steps' not in group or group.get('group')=='main' for group in new['testDefinition']['steps'])
header=(root/'src/TestSequenceLoops.h').read_text(encoding='utf-8')
for item in ('namespace TestSequenceLoops','inline bool expand','Maximum loop nesting depth',
    'Expanded sequence exceeds %1 executable steps','Limit references missing',
    'Loops and sweeps are permitted in Main only','Sweep stop must be reached exactly'):
 assert item in header,item
runner=(root/'src/TestRunnerBridge.cpp').read_text(encoding='utf-8')
assert 'TestSequenceLoops::expand(' in runner and 'm_loadedDefinition=data' in runner
editor=(root/'src/TestSequenceEditor.cpp').read_text(encoding='utf-8')
assert 'testSequenceSweepAxes' in editor and 'testSequenceAddChildButton' in editor
assert 'm_table=new QTreeWidget(editTab)' in editor
assert 'm_tabs->addTab(m_loopTab' not in editor
assert 'TestSequenceLoops::expand' in editor
print('PASS RC1.5.11: 7->38 Rigol steps, 50 measurements, unchanged physical drivers and command order, loop/editor contracts')
