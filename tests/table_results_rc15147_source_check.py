"""RC1.5.14.7 offline source/integration contract checks; not Qt compilation."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
models=(r/'src/TestResultsTableModels.cpp').read_text()
header=(r/'src/TestResultsTableModels.h').read_text()
browser=(r/'src/TestResultsBrowserDialog.cpp').read_text()
meas=(r/'src/widgets/MeasurementTableWidget.cpp').read_text()
history=(r/'src/TestResultsHistory.cpp').read_text()
cmake=(r/'CMakeLists.txt').read_text()
smoke=(r/'tests/test_results_browser_567_smoke.cpp').read_text()
assert 'QAbstractTableModel' in header and 'ArchivedRunsModel' in header and 'ArchivedRecordRowsModel' in header
assert 'beginResetModel()' in models and 'm_visible.append(i)' in models
assert 'QJsonArray m_rows' in header and 'm_rows.at(index.row()).toObject()' in models
assert 'QTableView* makeTable' in browser and 'QTableWidget' not in browser
assert 'm_measurementsModel->setRows(record.value(QStringLiteral("measurements")).toArray())' in browser
assert 'm_stepsModel->setRows(record.value(QStringLiteral("steps")).toArray())' in browser
assert 'm_json->setPlainText' not in browser.split('void TestResultsBrowserDialog::showDetail(',1)[1].split('void TestResultsBrowserDialog::renderRawJsonIfNeeded(',1)[0]
assert 'm_details->currentWidget()!=m_json' in browser
assert 'QHeaderView::ResizeToContents' not in meas
assert 'table->append(std::move(parsed))' in meas and 'beginInsertRows' in meas
assert '128LL*1024*1024' in history and '128 MB' in history
assert cmake.count('src/TestResultsTableModels.cpp')>=3
assert 'WFScriptLargeResultsVirtualRC15147Smoke' in cmake
assert 'findChild<QTableView*>' in smoke
print('PASS RC1.5.14.7 source contracts: virtual models, lazy JSON, fixed widths, larger guarded archive, native tests')
