"""RC1.5.6 source and Panel layout guard. Does not substitute for native Qt smoke."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
widgets=root/'src/widgets'
s=(widgets/'TestSequenceWidget.cpp').read_text()
e=(widgets/'EventLogWidget.cpp').read_text()
m=(widgets/'MeasurementTableWidget.cpp').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'void TestSequenceWidget::refreshRow(int i)' in s
assert 'void TestSequenceWidget::showEvent(QShowEvent* event)' in s
update=s.split('bool TestSequenceWidget::updateStep(',1)[1].split('bool TestSequenceWidget::applySteps(',1)[0]
assert 'refreshRow(i)' in update and 'rebuild();' not in update
assert 'class EventLogModel final : public QAbstractTableModel' in e
append=e.split('bool EventLogWidget::appendEvents(',1)[1].split('bool EventLogWidget::applyEvents(',1)[0]
assert 'model->dropOldest(evict)' in append and 'model->appendRange(firstNew,m_nextId)' in append
assert 'm_viewDirty=true' in append
assert 'setRowCount(0)' not in append
assert 'void EventLogWidget::showEvent(QShowEvent* event)' in e
measure=m.split('bool MeasurementTableWidget::setMeasurements(',1)[1]
assert 'QAbstractTableModel' in m
assert 'table->append(std::move(parsed))' in measure
assert 'beginInsertRows' in m
assert 'scrollToBottom' in measure
assert 'QAbstractTableModel' in m and 'QTableView' in m
assert 'virtualSteps' in s and 'QTableView' in s
assert 'WFScriptResultWidgetsIncrementalSmoke' in cmake
old=json.loads((root/'development/rigol_v065/Rigol_Four_Instrument_Operator_Bench_v0_6_5_Automated_Tests.wfpanel').read_text(encoding='utf8'))
new=json.loads((root/'development/rigol_v0651/Rigol_Four_Instrument_Operator_Bench_v0_6_5_1_Responsive_Test_Page.wfpanel').read_text(encoding='utf8'))
for key in old:
    if key!='pages': assert old[key]==new[key],key
assert old['pages'][:4]==new['pages'][:4]
oldPage=old['pages'][-1];newPage=new['pages'][-1]
assert oldPage['id']==newPage['id']=='rigoltest'
assert oldPage['width']==newPage['width'] and oldPage['height']==newPage['height']
oldControl={c['id']:c for c in oldPage['controls']}
newControl={c['id']:c for c in newPage['controls']}
assert oldControl.keys()==newControl.keys()
for id in oldControl:
    for prop,val in oldControl[id].items():
        if id in ('rigolTestReport','rigolTestFooter') and prop in ('y','height'):continue
        assert newControl[id][prop]==val,(id,prop)
ids=['rigolTestLog','rigolTestReport','rigolTestFooter','rigolTestJson','rigolTestCsv']
for i,a in enumerate(ids):
    x,y,w,h=[newControl[a][p] for p in ('x','y','width','height')]
    assert 0<=x and 0<=y and x+w<=newPage['width'] and y+h<=newPage['height'],a
    for b in ids[i+1:]:
        xb,yb,wb,hb=[newControl[b][p] for p in ('x','y','width','height')]
        assert not (x<xb+wb and xb<x+w and y<yb+hb and yb<y+h),(a,b)
assert len(new['testDefinition']['steps'])==62
print('PASS RC1.5.6 incremental source invariants, native smoke registration, unchanged Rigol scripts and no test-page overlap')
