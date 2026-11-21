"""Static source contract for RC1.5.14.8, not a C++ compile."""
from pathlib import Path
import json
r=Path(__file__).resolve().parents[1]
h=(r/'src/widgets/MeasurementTableWidget.h').read_text()
cpp=(r/'src/widgets/MeasurementTableWidget.cpp').read_text()
f=(r/'src/widgets/MeasurementTableWidgetFactory.cpp').read_text()
cmake=(r/'CMakeLists.txt').read_text()
for name in ['showToolbar','showRowCount','showFilter','showCopyButton','showExportButton','showClearButton','autoScroll']:
 assert name in f,name
assert 'd.text' in f
assert 'class MeasurementTableWidget final : public QWidget' in h
assert 'QSortFilterProxyModel' in cpp and 'm_view->setModel(m_source)' in cpp
assert 'table->append(std::move(parsed))' in cpp and 'beginInsertRows' in cpp
assert 'QHeaderView::ResizeToContents' not in cpp
assert 'm_countTimer->setInterval(100)' in cpp
assert 'QSaveFile' in cpp and 'csvCell' in cpp
assert 'WFScriptMeasurementHeaderRC15148Smoke' in cmake
assert (r/'src/widgets/MeasurementTableWidget.cpp').stat().st_size<25000
panel=json.loads((r/'development/performance/Measurement_100k_Header_Lab_RC15148.wfpanel').read_text())
assert panel['format']=='WFScriptPanel'
assert panel['controls'] and panel['script']
measure=next(c for c in panel['controls'] if c['id']=='measurementTable')
assert measure['properties']['showClearButton'] is False
assert '500 deterministic FAIL' in panel['script']
print('PASS RC1.5.14.8: toolbar/property contract, incremental source model, panel schema')
