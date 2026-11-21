import json,hashlib
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=r/'development/rigol_test_station_v032/Rigol_Automated_Test_Station_v0_3_2_Virtual_Log_RC1_5_14_3.wfpanel'
assert p.exists(),str(p)
d=json.loads(p.read_text(encoding='utf8'))
assert len(d['pages'])==5
assert len(d['libraries'])==7
logs=[c for page in d['pages'] for c in page['controls'] if c.get('type')=='event_log']
assert len(logs)==1 and logs[0]['properties']['maxEntries']==100000
assert len(d['testDefinition']['steps'])==8
assert d['scriptName']=='RigolAutomatedTestStation.js'
print('PASS Rigol v0.3.2: 5 pages, one 100k virtual log, original embedded libraries and test definition')
