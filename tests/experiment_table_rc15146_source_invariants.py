#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
view=(root/'src/widgets/ExperimentWidgets.cpp').read_text(encoding='utf-8')
model=(root/'src/widgets/ExperimentTableModel.cpp').read_text(encoding='utf-8')
bridge=(root/'src/PanelUiBridge.h').read_text(encoding='utf-8')
runtime=(root/'src/PanelRuntime.cpp').read_text(encoding='utf-8')
factory=(root/'src/widgets/ExperimentTableData.h').read_text(encoding='utf-8')
for name in ('experimentTableToolbar','experimentTableTitle','experimentTableRowCount',
             'experimentTableImport','experimentTableExport','experimentTableCopy','experimentTableClear',
             'experimentTableFollow'):
    assert name in view,name
for name in ('cellEdited','selectionChanged','importCompleted','exportCompleted','cleared','valueChanged'):
    assert f'QStringLiteral("{name}")' in view or f'"{name}"' in view,name
assert 'QAbstractItemView::AnyKeyPressed' in view
assert 'if(m_emitFullChange)m_sink' in view
assert 'm_countTimer->setInterval(100)' in view
assert 'm_view->setStyleSheet' in view
assert 'beginRemoveRows' in model and 'beginInsertRows' in model
assert 'm_edited(index.row(),index.column(),text,previous)' in model
assert 'receiveTableSelection' in bridge
assert 'name==QStringLiteral("cellEdited")' in runtime
assert 'm_ui->receiveTableSelection' in runtime
for key in ('Kind::Title','Kind::Status','Kind::Follow','Kind::ImportCsv','Kind::ExportCsv'):
    assert key in view,key
for key in ('title','status','autoScroll','importCsv','exportCsv','copySelection'):
    assert f'QStringLiteral("{key}")' in factory,key
print('PASS RC1.5.14.6 table toolbar, event, editable, streaming source invariants')
