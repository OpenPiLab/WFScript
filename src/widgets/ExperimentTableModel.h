#pragma once
#include "ExperimentTableData.h"
#include <QAbstractTableModel>
#include <functional>

// Virtual table model. It owns only rows, never one QTableWidgetItem per cell.
// All mutation occurs on the GUI thread through Qt row notifications.
class ExperimentTableModel final : public QAbstractTableModel {
public:
    explicit ExperimentTableModel(QObject* parent=nullptr);
    int rowCount(const QModelIndex& parent=QModelIndex()) const override;
    int columnCount(const QModelIndex& parent=QModelIndex()) const override;
    QVariant data(const QModelIndex& index,int role=Qt::DisplayRole) const override;
    QVariant headerData(int section,Qt::Orientation orientation,int role=Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index,const QVariant& value,int role=Qt::EditRole) override;
    void configure(const QStringList& headers,int maxRows,bool editable);
    bool mutate(const ExperimentTableData::Mutation& mutation);
    QVariantList rows() const {return ExperimentTableData::snapshot(m_rows);}
    const ExperimentTableData::Store& retained() const {return m_rows;}
    void setEditedCallback(std::function<void(int,int,const QString&,const QString&)> handler) {m_edited=std::move(handler);}
    int fullResetCount() const {return m_resets;}
private:
    ExperimentTableData::Store m_rows;
    QStringList m_headers;
    int m_maxRows=ExperimentTableLimits::DefaultRows;
    bool m_editable=false;
    int m_resets=0;
    std::function<void(int,int,const QString&,const QString&)> m_edited;
};
