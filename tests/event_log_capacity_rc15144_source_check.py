"""Offline source/Panel contract checks for RC1.5.14.4. Not a Qt runtime test."""
from pathlib import Path
import json,re
r=Path(__file__).resolve().parents[1]
cpp=(r/'src/widgets/EventLogWidget.cpp').read_text(encoding='utf-8')
h=(r/'src/widgets/EventLogWidget.h').read_text(encoding='utf-8')
cmake=(r/'CMakeLists.txt').read_text(encoding='utf-8')
panel=json.loads((r/'development/eventlog_rc15144/WFScript_EventLog_50k_Rollover_Lab.wfpanel').read_text())
assert 'std::deque<Entry> m_entries;' in h
assert 'std::deque<Id> m_visible;' in cpp
assert 'void dropOldest(int count)' in cpp and 'beginRemoveRows' in cpp and 'endRemoveRows' in cpp
assert 'void appendRange(Id first,Id last)' in cpp and 'beginInsertRows' in cpp and 'endInsertRows' in cpp
append=cpp.split('bool EventLogWidget::appendEvents(',1)[1].split('bool EventLogWidget::applyEvents(',1)[0]
assert 'model->dropOldest(evict)' in append and 'model->appendRange(firstNew,m_nextId)' in append
assert 'refresh();' not in append.split('auto* model=static_cast<EventLogModel*>',1)[1]
assert 'beginResetModel()' not in append and 'm_entries.remove(0,' not in cpp
assert 'QHeaderView::ResizeToContents' not in cpp
assert 'std::lower_bound' in cpp and 'indexAtOrAfter(anchorId)' in cpp
assert 'qBound(10,maxEntries,100000)' in cpp
assert 'WFScriptEventLogCapacityRC15144Smoke' in cmake
assert panel['version']==1 and panel['format']=='WFScriptPanel'
log=next(c for c in panel['controls'] if c['type']=='event_log')
assert log['id']=='stationLog' and log['properties']['maxEntries']==50000
assert 'callbackEpochMs' in panel['script'] and all(f"logPair('{key}')" in panel['script'] for key in ['id','id1','id2'])
assert len(re.findall(r'\bsetInterval\(',panel['script']))>=4
print('PASS Event Log RC1.5.14.4 offline source and Panel contracts')
