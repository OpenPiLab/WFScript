#include "PanelGauge.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QFontMetricsF>
#include <algorithm>
#include <cmath>
#include <utility>

PanelGauge::PanelGauge(QWidget* parent):QWidget(parent) {
    setMinimumSize(80,70);
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
}
void PanelGauge::configure(double lo,double hi,double value,QString title) {
    m_low=lo;m_high=hi;m_value=value;m_title=std::move(title);update();
}
void PanelGauge::setValue(double value) {m_value=value;update();}
void PanelGauge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    // Match the later instrument widgets: real rounded in-bounds border,
    // theme-aware surface and explicit legible dark/light typography.
    InstrumentVisuals::background(p,QRectF(rect()),10,this);
    const QRectF outer=QRectF(rect()).adjusted(10,10,-10,-10);
    if(outer.width()<10 || outer.height()<10)return;
    const QPointF c(outer.center().x(),outer.center().y()+outer.height()*.09);
    const double r=std::min(outer.width(),outer.height())*.38;
    const QRectF circle(c.x()-r,c.y()-r,2*r,2*r);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(InstrumentVisuals::tone(this,QColor("#728aa1")),qMax(2.0,r*.12),
                  Qt::SolidLine,Qt::RoundCap));
    p.drawArc(circle,225*16,-270*16);
    const double fraction=(std::isfinite(m_value) && m_high>m_low)
        ? qBound(0.0,(m_value-m_low)/(m_high-m_low),1.0):0;
    p.setPen(QPen(QColor("#2da8ef"),qMax(2.0,r*.12),Qt::SolidLine,Qt::RoundCap));
    p.drawArc(circle,225*16,int(-270*16*fraction));

    const QString reading=std::isfinite(m_value)?QString::number(m_value,'f',2)
                                                   :QStringLiteral("--");
    const QRectF valueRect(c.x()-r,c.y()-r*.25,2*r,r*.68);
    QFont number=InstrumentVisuals::numericFont(font(),qBound(10,qRound(r*.34),48),
                                                reading,valueRect.width());
    p.setPen(InstrumentVisuals::tone(this,QColor(232,245,252)));
    p.setFont(number);
    p.drawText(valueRect,Qt::AlignCenter,reading);

    const QRectF titleRect(c.x()-r,c.y()+r*.41,2*r,r*.43);
    QFont title=InstrumentVisuals::labelFont(font(),qBound(8,qRound(r*.17),20),true);
    while(title.pixelSize()>8 && QFontMetricsF(title).horizontalAdvance(m_title)>titleRect.width())
        title.setPixelSize(title.pixelSize()-1);
    p.setFont(title);
    p.setPen(InstrumentVisuals::tone(this,QColor(190,212,230)));
    p.drawText(titleRect,Qt::AlignCenter,
               InstrumentVisuals::elide(title,m_title,titleRect.width()));
}
