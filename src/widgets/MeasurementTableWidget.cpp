#include "MeasurementTableWidget.h"
#include "InstrumentVisuals.h"
#include <QHeaderView>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QColor>
#include <QVariantMap>
#include <cmath>
#include <utility>

MeasurementTableWidget::MeasurementTableWidget(QWidget* parent):QTableWidget(parent) {
    setColumnCount(4);setRowCount(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);setWordWrap(false);
    setAlternatingRowColors(true);setShowGrid(false);
    verticalHeader()->setVisible(false);
    horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    horizontalHeader()->setMinimumSectionSize(65);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    configure(3,100,m_headers);
}
void MeasurementTableWidget::configure(int decimals,int maxRows,const QString& headers) {
    m_decimals=qBound(0,decimals,6);
    m_maxRows=qBound(1,maxRows,100);
    const QStringList titles=headers.split('|');
    if(titles.size()==4) {
        m_headers=headers;
        setHorizontalHeaderLabels(titles);
    }
    if(rowCount()>m_maxRows)setRowCount(m_maxRows);
    setStyleSheet(InstrumentVisuals::light(this)?QStringLiteral(
        "QTableWidget {background:#f3f8fc;color:#263b50;alternate-background-color:#e4edf5;border:1px solid #b3c6d5;}"
        "QHeaderView::section {background:#d8e7f1;color:#263b50;padding:5px;border:0px;}"):
        QStringLiteral(
        "QTableWidget {background:#0e1c2b;color:#d5e4f2;alternate-background-color:#16293b;border:1px solid #3c566e;}"
        "QHeaderView::section {background:#243a4e;color:#d5e4f2;padding:5px;border:0px;}"));
}
namespace {
struct Row {
    QString name,actual,limits,status;
};
}
bool MeasurementTableWidget::setMeasurements(const QVariant& input) {
    QVariantList rows=input.toList();
    // A reset object is supported without a new ui API surface.
    if(input.toMap().value("reset").toBool()) {setRowCount(0);return true;}
    if(input.metaType().id()!=QMetaType::QVariantList && input.metaType().id()!=QMetaType::QStringList)
        return false;
    if(rows.size()>m_maxRows)return false; // reject, do not truncate silently
    QVector<Row> parsed;parsed.reserve(rows.size());
    for(const auto& rowValue:rows) {
        const QVariantMap row=rowValue.toMap();
        if(row.isEmpty() || !row.contains("name"))return false;
        Row out;
        out.name=row.value("name").toString().left(160);
        const QString unit=row.value("unit").toString().left(16);
        const QString forced=row.value("status").toString().toUpper();
        if(forced=="ERROR" || forced=="SKIP") {
            out.status=forced;out.actual=QStringLiteral("—");out.limits=QStringLiteral("—");
        } else if(!row.contains("value") || row.value("value").isNull()) {
            out.status=QStringLiteral("NOT TESTED");out.actual=QStringLiteral("—");out.limits=QStringLiteral("—");
        } else {
            bool a=false,l=false,h=false;
            const double actual=row.value("value").toDouble(&a);
            const double low=row.value("low").toDouble(&l);
            const double high=row.value("high").toDouble(&h);
            if(!a || !l || !h || !std::isfinite(actual) || !std::isfinite(low) ||
               !std::isfinite(high) || low>high)return false;
            const QString suffix=unit.isEmpty()?QString():QStringLiteral(" ")+unit;
            out.actual=InstrumentVisuals::number(actual,m_decimals)+suffix;
            out.limits=InstrumentVisuals::number(low,m_decimals)+QStringLiteral(" .. ")+
                       InstrumentVisuals::number(high,m_decimals)+suffix;
            out.status=actual>=low && actual<=high?QStringLiteral("PASS"):QStringLiteral("FAIL");
        }
        parsed.append(std::move(out));
    }
    // Only mutate the visible widget once every incoming row is validated.
    setRowCount(parsed.size());
    for(int i=0;i<parsed.size();++i) {
        const auto& row=parsed.at(i);
        const QStringList cells{row.name,row.actual,row.limits,row.status};
        for(int col=0;col<4;++col) {
            auto* item=new QTableWidgetItem(cells.at(col));
            item->setFlags(Qt::ItemIsEnabled);
            item->setTextAlignment(col==0?Qt::AlignLeft|Qt::AlignVCenter:
                                    Qt::AlignCenter);
            if(col==3) {
                QColor color=row.status=="PASS"?QColor(56,207,139):
                             row.status=="FAIL" || row.status=="ERROR"?QColor(255,114,115):
                             QColor(168,181,193);
                item->setForeground(color);
                auto font=item->font();font.setBold(true);item->setFont(font);
            }
            setItem(i,col,item);
        }
    }
    return true;
}
