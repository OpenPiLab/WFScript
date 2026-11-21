#pragma once
// Shared, QtCore-only contract for Experiment DataTable updates. The worker
// and GUI validate identical operations without exchanging complete snapshots.
#include <QMetaType>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>
#include <cmath>
#include <deque>
#include <utility>

namespace ExperimentTableLimits {
inline constexpr int MaxColumns = 32;
inline constexpr int MaxRows = 100000;
inline constexpr int DefaultRows = 250;
inline constexpr int MaxHeaderCharacters = 100;
inline constexpr int MaxCellCharacters = 256;
inline constexpr int MaxInitialRowsCharacters = 256 * 1024;
inline constexpr int MaxPropertyBytes = 512 * 1024;
inline constexpr int MaxAppendBatch = 2048;
}

namespace ExperimentTableData {
using Row = QStringList;
using Store = std::deque<Row>;
enum class Kind { Replace, Append, Clear, Cell, RowUpdate, Title, Status, Follow, ImportCsv, ExportCsv, CopySelection };
struct Mutation {
    Kind kind = Kind::Replace;
    QVector<Row> rows;
    int row = -1;
    int column = -1;
    QString value;
    bool enabled = false;
};
inline bool cell(const QVariant& input,QString* output) {
    if(!output)return false;
    const QString str=input.toString();
    if(str.size()>ExperimentTableLimits::MaxCellCharacters || str.contains(QChar::Null))return false;
    *output=str;return true;
}
inline bool row(const QVariant& input,int columns,Row* output) {
    if(!output || input.metaType().id()!=QMetaType::QVariantList)return false;
    const QVariantList cells=input.toList();
    if(cells.size()!=columns)return false;
    Row result;result.reserve(columns);
    for(const QVariant& item:cells){QString value;if(!cell(item,&value))return false;result<<value;}
    *output=std::move(result);return true;
}
inline bool batch(const QVariant& value,int columns,int cap,QVector<Row>* output) {
    if(value.metaType().id()!=QMetaType::QVariantList)return false;
    const QVariantList input=value.toList();
    if(input.size()>cap)return false;
    QVector<Row> result;result.reserve(input.size());
    for(const QVariant& item:input){Row parsed;if(!row(item,columns,&parsed))return false;result.push_back(std::move(parsed));}
    *output=std::move(result);return true;
}
inline bool indexNumber(const QVariant& value,int* out) {
    if(!out)return false;
    bool valid=false;
    const double number=value.toDouble(&valid);
    if(!valid || !std::isfinite(number) || number<0 || number>ExperimentTableLimits::MaxRows ||
       number!=std::trunc(number))return false;
    *out=int(number);return true;
}
inline bool decode(const QVariant& value,int columns,int maxRows,Mutation* out) {
    if(!out || columns<1 || columns>ExperimentTableLimits::MaxColumns || maxRows<1 ||
       maxRows>ExperimentTableLimits::MaxRows)return false;
    Mutation m;
    if(value.metaType().id()==QMetaType::QVariantList) {
        if(!batch(value,columns,maxRows,&m.rows))return false;
        m.kind=Kind::Replace;
    } else if(value.metaType().id()==QMetaType::QVariantMap) {
        const QVariantMap op=value.toMap();
        if(op.size()!=1)return false;
        if(op.contains(QStringLiteral("append"))) {
            m.kind=Kind::Append;Row r;
            if(!row(op.value(QStringLiteral("append")),columns,&r))return false;
            m.rows.push_back(std::move(r));
        } else if(op.contains(QStringLiteral("appendRows"))) {
            m.kind=Kind::Append;
            if(!batch(op.value(QStringLiteral("appendRows")),columns,
                      ExperimentTableLimits::MaxAppendBatch,&m.rows))return false;
        } else if(op.contains(QStringLiteral("replace"))) {
            m.kind=Kind::Replace;
            if(!batch(op.value(QStringLiteral("replace")),columns,maxRows,&m.rows))return false;
        } else if(op.contains(QStringLiteral("clear")) && op.value(QStringLiteral("clear")).metaType().id()==QMetaType::Bool &&
                  op.value(QStringLiteral("clear")).toBool()) {
            m.kind=Kind::Clear;
        } else if(op.contains(QStringLiteral("title")) || op.contains(QStringLiteral("status"))) {
            const bool title=op.contains(QStringLiteral("title"));
            const QVariant value=op.value(title?QStringLiteral("title"):QStringLiteral("status"));
            if(value.metaType().id()!=QMetaType::QString || value.toString().size()>(title?120:200))return false;
            m.kind=title?Kind::Title:Kind::Status;m.value=value.toString();
        } else if(op.contains(QStringLiteral("autoScroll"))) {
            if(op.value(QStringLiteral("autoScroll")).metaType().id()!=QMetaType::Bool)return false;
            m.kind=Kind::Follow;m.enabled=op.value(QStringLiteral("autoScroll")).toBool();
        } else if(op.contains(QStringLiteral("importCsv")) || op.contains(QStringLiteral("exportCsv"))) {
            const bool importing=op.contains(QStringLiteral("importCsv"));
            const QVariant path=op.value(importing?QStringLiteral("importCsv"):QStringLiteral("exportCsv"));
            if(path.metaType().id()==QMetaType::Bool) {
                if(!path.toBool())return false;
            } else if(path.metaType().id()==QMetaType::QString) {
                if(path.toString().isEmpty() || path.toString().size()>2048)return false;
                m.value=path.toString();
            } else return false;
            m.kind=importing?Kind::ImportCsv:Kind::ExportCsv;
        } else if(op.contains(QStringLiteral("copySelection"))) {
            if(op.value(QStringLiteral("copySelection")).metaType().id()!=QMetaType::Bool ||
               !op.value(QStringLiteral("copySelection")).toBool())return false;
            m.kind=Kind::CopySelection;
        } else if(op.contains(QStringLiteral("setCell"))) {
            const QVariantMap cellOp=op.value(QStringLiteral("setCell")).toMap();
            if(cellOp.size()!=3 || !cellOp.contains(QStringLiteral("row")) ||
               !cellOp.contains(QStringLiteral("column")) || !cellOp.contains(QStringLiteral("value")) ||
               !indexNumber(cellOp.value(QStringLiteral("row")),&m.row) ||
               !indexNumber(cellOp.value(QStringLiteral("column")),&m.column) ||
               !cell(cellOp.value(QStringLiteral("value")),&m.value))return false;
            if(m.row<0 || m.column<0 || m.column>=columns)return false;
            m.kind=Kind::Cell;
        } else if(op.contains(QStringLiteral("setRow"))) {
            const QVariantMap rowOp=op.value(QStringLiteral("setRow")).toMap();
            if(rowOp.size()!=2 || !rowOp.contains(QStringLiteral("row")) ||
               !rowOp.contains(QStringLiteral("values")) ||
               !indexNumber(rowOp.value(QStringLiteral("row")),&m.row))return false;
            if(m.row<0)return false;
            Row r;if(!row(rowOp.value(QStringLiteral("values")),columns,&r))return false;
            m.kind=Kind::RowUpdate;m.rows.push_back(std::move(r));
        } else return false;
    } else return false;
    *out=std::move(m);return true;
}
inline bool apply(Store* rows,int maxRows,int columns,const Mutation& m) {
    if(!rows)return false;
    if(m.kind==Kind::Cell) {
        if(m.row>=int(rows->size()) || m.column>=columns)return false;
        (*rows)[std::size_t(m.row)][m.column]=m.value;
    } else if(m.kind==Kind::RowUpdate) {
        if(m.row>=int(rows->size()) || m.rows.size()!=1)return false;
        (*rows)[std::size_t(m.row)]=m.rows.front();
    } else if(m.kind==Kind::Replace) {
        if(m.rows.size()>maxRows)return false;
        rows->clear();for(const Row& r:m.rows)rows->push_back(r);
    } else if(m.kind==Kind::Clear)rows->clear();
    else if(m.kind==Kind::Append) {
        for(const Row& r:m.rows){
            if(int(rows->size())>=maxRows)rows->pop_front();
            rows->push_back(r);
        }
    }
    return true;
}
inline QVariantList snapshot(const Store& rows,int start=0,int count=-1) {
    QVariantList result;
    const int first=qBound(0,start,int(rows.size()));
    const int end=count<0?int(rows.size()):qMin(int(rows.size()),first+count);
    result.reserve(end-first);
    for(int index=first;index<end;++index) {
        QVariantList cells;const Row& r=rows[std::size_t(index)];cells.reserve(r.size());
        for(const QString& text:r)cells.append(text);
        result.append(QVariant(cells));
    }
    return result;
}
} // namespace ExperimentTableData
