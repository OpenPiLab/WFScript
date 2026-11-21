"""Offline source-contract checks; native Qt smoke still requires Windows Qt."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
loop=(r/'src/TestSequenceLoops.h').read_text()
cap=(r/'src/TestSequenceCapacity.h').read_text()
runner=(r/'src/TestRunnerBridge.cpp').read_text()
sequence=(r/'src/widgets/TestSequenceWidget.cpp').read_text()
measure=(r/'src/widgets/MeasurementTableWidget.cpp').read_text()
preview=(r/'src/TestSequenceEditor.cpp').read_text()
event=(r/'src/widgets/EventLogWidget.cpp').read_text()
checks={
 'axis limit 10k': 'maxSweepAxisPoints = 10000' in cap and 'input.size()>TestSequenceCapacity::maxSweepAxisPoints' in loop,
 'range limit 10k': 'n>double(TestSequenceCapacity::maxSweepAxisPoints-1)+1e-8' in loop,
 'repeat limit 10k': 'maxRepeatCount = 10000' in cap and 'count.toDouble()>TestSequenceCapacity::maxRepeatCount' in loop,
 'total expansion guard': 'maxExpandedSteps = 10000' in cap,
 'full sequence once': 'virtualSteps' in runner and 'stepsAsVariant()' in runner,
 'incremental step updates': '"update"' in runner and 'patchStep(const Step& step)' in runner,
 'incremental measurement payloads': 'QStringLiteral("append"),tableBatch' in runner,
 'measurement model': 'QAbstractTableModel' in measure and 'beginInsertRows' in measure,
 'scroll behavior': 'atBottom' in measure and 'scrollToBottom()' in measure,
 'virtual step model': 'class VirtualStepModel final : public QAbstractTableModel' in sequence,
 'virtual preview': 'class ExpandedPreviewModel final : public QAbstractTableModel' in preview,
 'virtual event log': 'class EventLogModel final : public QAbstractTableModel' in event and 'model->dropOldest(evict)' in event and 'model->appendRange(firstNew,m_nextId)' in event,
 'log capacity 100k configurable': 'qBound(10,maxEntries,100000)' in event,
 'native 10k GUI test registered': 'WFScriptLargeSweepVirtualRC15143Smoke' in (r/'CMakeLists.txt').read_text(),
}
for label,good in checks.items():
 assert good,label
print(f'PASS RC1.5.14.3 offline source checks: {len(checks)} / {len(checks)}')
