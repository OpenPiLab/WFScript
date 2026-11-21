#include "ExperimentTableModel.h"
#include <QBrush>
#include <QtGlobal>

ExperimentTableModel::ExperimentTableModel(QObject* parent):QAbstractTableModel(parent){}
int ExperimentTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid()?0:int(m_rows.size());
}
int ExperimentTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid()?0:m_headers.size();
}
QVariant ExperimentTableModel::data(const QModelIndex& index,int role) const {
    if(!index.isValid() || index.row()<0 || index.column()<0 ||
       index.row()>=rowCount() || index.column()>=columnCount())return {};
    if(role==Qt::DisplayRole || role==Qt::EditRole || role==Qt::ToolTipRole)
        return m_rows[std::size_t(index.row())].at(index.column());
    if(role==Qt::TextAlignmentRole)return int(Qt::AlignLeft|Qt::AlignVCenter);
    return {};
}
QVariant ExperimentTableModel::headerData(int section,Qt::Orientation orientation,int role) const {
    if(role==Qt::DisplayRole && orientation==Qt::Horizontal)return m_headers.value(section);
    if(role==Qt::DisplayRole && orientation==Qt::Vertical)return section+1;
    return {};
}
Qt::ItemFlags ExperimentTableModel::flags(const QModelIndex& index) const {
    if(!index.isValid())return Qt::NoItemFlags;
    Qt::ItemFlags f=Qt::ItemIsEnabled|Qt::ItemIsSelectable;
    if(m_editable)f|=Qt::ItemIsEditable;
    return f;
}
bool ExperimentTableModel::setData(const QModelIndex& index,const QVariant& value,int role) {
    if(role!=Qt::EditRole || !m_editable || !index.isValid() ||
       index.row()<0 || index.row()>=rowCount() || index.column()<0 || index.column()>=columnCount())return false;
    QString text;if(!ExperimentTableData::cell(value,&text))return false;
    QString& current=m_rows[std::size_t(index.row())][index.column()];
    if(current==text)return true;
    const QString previous=current;
    current=text;
    emit dataChanged(index,index,{Qt::DisplayRole,Qt::EditRole});
    if(m_edited)m_edited(index.row(),index.column(),text,previous);
    return true;
}
void ExperimentTableModel::configure(const QStringList& headers,int maxRows,bool editable) {
    m_editable=editable;m_maxRows=qBound(1,maxRows,ExperimentTableLimits::MaxRows);
    if(m_headers!=headers) {
        beginResetModel();++m_resets;m_headers=headers;m_rows.clear();endResetModel();
    }
    if(int(m_rows.size())>m_maxRows) {
        const int count=int(m_rows.size())-m_maxRows;
        beginRemoveRows(QModelIndex(),0,count-1);
        for(int i=0;i<count;++i)m_rows.pop_front();
        endRemoveRows();
    }
}
bool ExperimentTableModel::mutate(const ExperimentTableData::Mutation& m) {
    using Kind=ExperimentTableData::Kind;
    if(m.kind==Kind::Cell) {
        if(m.row<0 || m.row>=rowCount() || m.column<0 || m.column>=columnCount())return false;
        m_rows[std::size_t(m.row)][m.column]=m.value;
        emit dataChanged(index(m.row,m.column),index(m.row,m.column));return true;
    }
    if(m.kind==Kind::RowUpdate) {
        if(m.row<0 || m.row>=rowCount() || m.rows.size()!=1)return false;
        m_rows[std::size_t(m.row)]=m.rows.front();
        emit dataChanged(index(m.row,0),index(m.row,columnCount()-1));return true;
    }
    if(m.kind==Kind::Clear || m.kind==Kind::Replace) {
        if(m.kind==Kind::Replace && m.rows.size()>m_maxRows)return false;
        beginResetModel();++m_resets;m_rows.clear();
        if(m.kind==Kind::Replace)for(const auto& r:m.rows)m_rows.push_back(r);
        endResetModel();return true;
    }
    if(m.kind==Kind::Append && !m.rows.isEmpty()) {
        const int incoming=int(m.rows.size());
        // A single valid batch may exceed a small widget's retention cap.
        // Remove only rows that actually exist, then keep the newest inputs.
        const int evict=qMin(rowCount(),qMax(0,rowCount()+incoming-m_maxRows));
        const int incomingStart=qMax(0,incoming-m_maxRows);
        const int accepted=incoming-incomingStart;
        // Each batch is <=2048: removal and insertion are both bounded.
        if(evict>0) {
            beginRemoveRows(QModelIndex(),0,evict-1);
            for(int i=0;i<evict;++i)m_rows.pop_front();
            endRemoveRows();
        }
        const int start=rowCount();
        beginInsertRows(QModelIndex(),start,start+accepted-1);
        for(int i=incomingStart;i<incoming;++i)m_rows.push_back(m.rows.at(i));
        endInsertRows();
    }
    return true;
}
