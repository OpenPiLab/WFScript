#include "MeasurementResultWidget.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QVariantMap>
#include <QFontMetrics>
#include <QMetaType>
#include <cmath>

MeasurementResultWidget::MeasurementResultWidget(QWidget* parent):QWidget(parent) {
    setMinimumSize(170,75);
    setAttribute(Qt::WA_OpaquePaintEvent,false);
}
void MeasurementResultWidget::configure(const QString& title,const QString& unit,
                                         double low,double high,int decimals) {
    m_title=title;m_unit=unit;
    m_low=low;m_high=high;m_decimals=qBound(0,decimals,6);
    // Changing design settings must not reuse a stale PASS from previous limits.
    if(m_hasValue)m_state=(m_value>=m_low && m_value<=m_high)?"PASS":"FAIL";
    update();
}
void MeasurementResultWidget::reset() {
    m_hasValue=false;m_value=0.0;m_state=QStringLiteral("NOT TESTED");update();
}
bool MeasurementResultWidget::applyMeasurement(const QVariant& input) {
    QVariant value=input;
    const QVariantMap map=input.toMap();
    if(!map.isEmpty()) {
        if(map.value("reset").toBool()) {reset();return true;}
        if(map.value("status").toString().compare("ERROR",Qt::CaseInsensitive)==0) {
            m_hasValue=false;m_state="ERROR";update();return true;
        }
        if(!map.contains("value"))return false;
        value=map.value("value");
    }
    bool ok=false;
    const double measured=value.toDouble(&ok);
    if(!ok || !std::isfinite(measured))return false;
    m_value=measured;m_hasValue=true;
    m_state=measured>=m_low && measured<=m_high?"PASS":"FAIL";
    update();return true;
}
void MeasurementResultWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    InstrumentVisuals::startPaint(p,this,310,138);
    const QRectF box(1,1,308,136);
    InstrumentVisuals::background(p,box,11,this);
    const QColor ink=InstrumentVisuals::tone(this,QColor(208,225,238));
    QColor state=m_state=="PASS"?QColor(67,221,157):m_state=="FAIL" || m_state=="ERROR"?
                 QColor(255,105,108):QColor(170,181,194);
    // Keep semantic accent colours readable on both Panel themes.
    p.setPen(Qt::NoPen);p.setBrush(state);
    p.drawRoundedRect(QRectF(13,13,4,110),2,2);
    QFont title=InstrumentVisuals::labelFont(font(),13,true);
    p.setFont(title);p.setPen(ink);
    p.drawText(QRectF(28,13,180,21),Qt::AlignVCenter,
               InstrumentVisuals::elide(title,m_title,178));
    p.setPen(Qt::NoPen);p.setBrush(state.darker(240));
    p.drawRoundedRect(QRectF(213,13,85,25),7,7);
    p.setPen(state);p.setFont(InstrumentVisuals::labelFont(font(),11,true));
    p.drawText(QRectF(213,13,85,25),Qt::AlignCenter,m_state);
    const QString numeric=m_hasValue?InstrumentVisuals::number(m_value,m_decimals):QStringLiteral("—");
    const QString shown=numeric+(m_unit.isEmpty()?QString():QStringLiteral(" ")+m_unit);
    QFont digits=InstrumentVisuals::numericFont(font(),33,shown,260);
    p.setFont(digits);p.setPen(m_hasValue?state:ink);
    p.drawText(QRectF(27,43,264,51),Qt::AlignVCenter|Qt::AlignHCenter,shown);
    const QString limits=QStringLiteral("LIMITS  %1 – %2 %3").arg(
        InstrumentVisuals::number(m_low,m_decimals),
        InstrumentVisuals::number(m_high,m_decimals),m_unit);
    QFont detail=InstrumentVisuals::labelFont(font(),11);
    p.setFont(detail);p.setPen(InstrumentVisuals::tone(this,QColor(148,170,187)));
    p.drawText(QRectF(23,99,274,23),Qt::AlignCenter,
               InstrumentVisuals::elide(detail,limits,270));
}
