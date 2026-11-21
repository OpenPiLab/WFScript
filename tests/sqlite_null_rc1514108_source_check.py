#!/usr/bin/env python3
"""RC1.5.14.10.8: SQLite NULL/empty-string bridge source contract.

The definitive native test is WFScriptSQLiteSmoke on Windows Qt 6.9.2;
this source check does not claim native execution.
"""
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def read(path):return (R/path).read_text(encoding='utf-8')
sql=read('src/SqliteBridge.cpp')
assert 'cell=QVariant::fromValue(nullptr);' in sql
assert 'cell=QVariant(QByteArray());' not in sql
assert 'item.isNull()?QVariant(QMetaType::fromType<QString>())' in sql
assert 'if(!cell.isNull())' in sql
assert 'QVariantList row;' in sql
assert 'row.append(cell);' in sql
assert 'rows.append(QVariant(row));' in sql
assert 'ScriptValueConversion::toJavaScript(m_engine,reply)' in sql
conversion=read('src/ScriptValueConversion.h')
assert 'if (!engine || depth>32 || !value.isValid())return QJSValue(QJSValue::UndefinedValue);' in conversion
assert 'if(type==QMetaType::QString)return QJSValue(value.toString());' in conversion
assert 'return QJSValue(QJSValue::NullValue);' in conversion
smoke=read('tests/SqliteSmoke.cpp')
assert '#include "ScriptValueConversion.h"' in smoke
assert 'const QVariant nativeNull=QVariant::fromValue(nullptr);' in smoke
assert 'const QJSValue jsNull=ScriptValueConversion::toJavaScript(&engine,nativeNull);' in smoke
assert '!nativeNull.isValid() || !nativeNull.isNull() || !jsNull.isNull()' in smoke
assert 'jsEmpty.isString()' in smoke
assert 'observed===null' in smoke
assert 'SQL NULL round trip: observed ' in smoke
assert "check(next.ok,'insert null: '+next.error);" in smoke
assert 'disk persistence: ' in smoke
print('PASS RC1.5.14.10.8 SQLite NULL/empty-string source contracts')
