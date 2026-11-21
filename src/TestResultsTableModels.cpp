#include "TestResultsTableModels.h"
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonValue>
#include <QVariant>

namespace {
QString displayValue(const QJsonValue& value) {
    if(value.isUndefined() || value.isNull())return {};
    if(value.isString())return value.toString();
    if(value.isBool())return value.toBool()?QStringLiteral("true"):QStringLiteral("false");
    if(value.isDouble())return QString::number(value.toDouble(),'g',15);
    return QString::fromUtf8(QJsonDocument::fromVariant(value.toVariant()).toJson(QJsonDocument::Compact));
}
QString stateLabel(const QString& state) {
    if(state==QLatin1String("passed"))return QStringLiteral("PASS");
    if(state==QLatin1String("failed"))return QStringLiteral("FAIL");
    if(state==QLatin1String("error"))return QStringLiteral("ERROR");
    if(state==QLatin1String("aborted"))return QStringLiteral("ABORTED");
    return state.toUpper();
}
QString localTime(const QString& iso) {
    const QDateTime date=QDateTime::fromString(iso,Qt::ISODateWithMs);
    return date.isValid()?date.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")):iso;
}
QColor stateColor(const QString& state) {
    if(state==QLatin1String("passed"))return QColor(QStringLiteral("#2b9b70"));
    if(state==QLatin1String("failed") || state==QLatin1String("error"))
        return QColor(QStringLiteral("#d95b69"));
    return QColor(QStringLiteral("#c18a37"));
}
}

ArchivedRunsModel::ArchivedRunsModel(QObject* parent):QAbstractTableModel(parent){}
void ArchivedRunsModel::setEntries(const QList<ArchivedTestSummary>* entries) {
    beginResetModel();m_entries=entries;m_visible.clear();
    if(entries) {
        m_visible.reserve(entries->size());
        for(int i=0;i<entries->size();++i)m_visible.append(i);
    }
    endResetModel();
}
void ArchivedRunsModel::filter(const QString& search,const QString& state) {
    beginResetModel();m_visible.clear();
    if(m_entries) {
        m_visible.reserve(m_entries->size());
        for(int i=0;i<m_entries->size();++i) {
            const auto& e=m_entries->at(i);
            if(state!=QLatin1String("all") && e.state!=state)continue;
            if(!search.isEmpty() && !e.searchText.contains(search,Qt::CaseInsensitive))continue;
            m_visible.append(i);
        }
    }
    endResetModel();
}
QString ArchivedRunsModel::pathAt(int row) const {
    if(!m_entries || row<0 || row>=m_visible.size())return {};
    return m_entries->at(m_visible.at(row)).path;
}
int ArchivedRunsModel::rowForPath(const QString& path) const {
    if(path.isEmpty() || !m_entries)return -1;
    for(int i=0;i<m_visible.size();++i)
        if(m_entries->at(m_visible.at(i)).path==path)return i;
    return -1;
}
int ArchivedRunsModel::countForState(const QString& state) const {
    if(!m_entries)return 0;
    int count=0;
    for(int index:m_visible)if(m_entries->at(index).state==state)++count;
    return count;
}
int ArchivedRunsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid()?0:m_visible.size();
}
int ArchivedRunsModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid()?0:7;
}
QVariant ArchivedRunsModel::headerData(int section,Qt::Orientation orientation,int role) const {
    static const QStringList headers={QStringLiteral("Completed (local)"),QStringLiteral("UUT"),
        QStringLiteral("Result"),QStringLiteral("Test"),QStringLiteral("Duration"),
        QStringLiteral("Mode"),QStringLiteral("Execution ID")};
    return role==Qt::DisplayRole && orientation==Qt::Horizontal?QVariant(headers.value(section)):QVariant{};
}
QVariant ArchivedRunsModel::data(const QModelIndex& index,int role) const {
    if(!index.isValid() || !m_entries || index.row()<0 || index.row()>=m_visible.size() ||
       index.column()<0 || index.column()>=7)return {};
    const auto& e=m_entries->at(m_visible.at(index.row()));
    if(role==Qt::UserRole)return e.path;
    if(role==Qt::ForegroundRole && index.column()==2)return QBrush(stateColor(e.state));
    if(role!=Qt::DisplayRole && role!=Qt::ToolTipRole)return {};
    switch(index.column()) {
    case 0:return localTime(e.finishedAtUtc);
    case 1:return e.uutId;
    case 2:return stateLabel(e.state);
    case 3:return e.testName;
    case 4:return QString::number(e.elapsedMs)+QStringLiteral(" ms");
    case 5:return e.instrumentMode;
    default:return e.executionId;
    }
}

ArchivedRecordRowsModel::ArchivedRecordRowsModel(const QStringList& headers,
                                                  const QStringList& keys,QObject* parent)
    :QAbstractTableModel(parent),m_headers(headers),m_keys(keys){}
void ArchivedRecordRowsModel::setRows(const QJsonArray& rows) {
    beginResetModel();m_rows=rows;endResetModel();
}
void ArchivedRecordRowsModel::setObjectPairs(const QJsonObject& values) {
    QJsonArray rows;
    for(auto it=values.constBegin();it!=values.constEnd();++it)
        rows.append(QJsonObject{{QStringLiteral("key"),it.key()},{QStringLiteral("value"),it.value()}});
    setRows(rows);
}
int ArchivedRecordRowsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid()?0:m_rows.size();
}
int ArchivedRecordRowsModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid()?0:m_headers.size();
}
QVariant ArchivedRecordRowsModel::headerData(int section,Qt::Orientation orientation,int role) const {
    return role==Qt::DisplayRole && orientation==Qt::Horizontal?QVariant(m_headers.value(section)):QVariant{};
}
QVariant ArchivedRecordRowsModel::data(const QModelIndex& index,int role) const {
    if(!index.isValid() || index.row()<0 || index.row()>=m_rows.size() ||
       index.column()<0 || index.column()>=m_keys.size() ||
       (role!=Qt::DisplayRole && role!=Qt::ToolTipRole))return {};
    const QJsonObject obj=m_rows.at(index.row()).toObject();
    return displayValue(obj.value(m_keys.at(index.column())));
}
