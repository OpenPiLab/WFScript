#pragma once

#include "TestResultsRepository.h"
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVector>

// RC1.5.14.7. Read-only archived results models. Data is retained as parsed
// JSON once per selected report, not as one QTableWidgetItem per table cell.
// No dependency on JavaScript, instrument drivers, or live TestRunner state.
class ArchivedRunsModel final : public QAbstractTableModel {
public:
    explicit ArchivedRunsModel(QObject* parent=nullptr);
    void setEntries(const QList<ArchivedTestSummary>* entries);
    void filter(const QString& search,const QString& state);
    QString pathAt(int visibleRow) const;
    int countForState(const QString& state) const;
    int rowForPath(const QString& path) const;
    int rowCount(const QModelIndex& parent=QModelIndex()) const override;
    int columnCount(const QModelIndex& parent=QModelIndex()) const override;
    QVariant data(const QModelIndex& index,int role=Qt::DisplayRole) const override;
    QVariant headerData(int section,Qt::Orientation orientation,int role=Qt::DisplayRole) const override;
private:
    const QList<ArchivedTestSummary>* m_entries=nullptr;
    QVector<int> m_visible;
};

class ArchivedRecordRowsModel final : public QAbstractTableModel {
public:
    explicit ArchivedRecordRowsModel(const QStringList& headers,
                                    const QStringList& keys,QObject* parent=nullptr);
    void setRows(const QJsonArray& rows);
    void setObjectPairs(const QJsonObject& values);
    int rowCount(const QModelIndex& parent=QModelIndex()) const override;
    int columnCount(const QModelIndex& parent=QModelIndex()) const override;
    QVariant data(const QModelIndex& index,int role=Qt::DisplayRole) const override;
    QVariant headerData(int section,Qt::Orientation orientation,int role=Qt::DisplayRole) const override;
private:
    QStringList m_headers;
    QStringList m_keys;
    QJsonArray m_rows;
};
