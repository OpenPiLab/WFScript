#include "SegmentedBarWidget.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QSizePolicy>
#include <QtMath>
#include <cmath>

SegmentedBarWidget::SegmentedBarWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
}
void SegmentedBarWidget::configure(const QString& title,double value,double minimum,double maximum,
                                   const QString& unit,const QString& accent,int decimals,
                                   int segments,int warningPct,int alarmPct,bool peakHold) {
    if(!std::isfinite(value)||!std::isfinite(minimum)||!std::isfinite(maximum)||maximum<=minimum)return;
    m_title=title;m_value=value;m_peak=value;m_min=minimum;m_max=maximum;
    m_unit=unit;m_accent=accent;m_decimals=qBound(0,decimals,4);
    m_segments=qBound(5,segments,48);m_warningPct=qBound(0,warningPct,100);
    m_alarmPct=qBound(0,alarmPct,100);m_peakHold=peakHold;
    update();
}
void SegmentedBarWidget::setValue(double value) {
    if(!std::isfinite(value))return;
    if(value==m_value)return;
    m_value=value;
    if(m_peakHold)m_peak=qMax(m_peak,value);
    else m_peak=value;
    update();
}
bool SegmentedBarWidget::alarmActive() const {
    return InstrumentVisuals::fraction(m_value,m_min,m_max)*100>=m_alarmPct;
}
void SegmentedBarWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    InstrumentVisuals::startPaint(p,this,355,140);
    InstrumentVisuals::background(p,QRectF(0,0,355,140),13,this);
    const QColor accent=InstrumentVisuals::accent(m_accent);
    const QFont titleFont=InstrumentVisuals::labelFont(font(),15,true);
    p.setFont(titleFont);p.setPen(InstrumentVisuals::tone(this, QColor(191,210,226)));
    p.drawText(QRectF(16,12,205,27),Qt::AlignLeft|Qt::AlignVCenter,
               InstrumentVisuals::elide(titleFont,m_title,203));
    const QString reading=InstrumentVisuals::number(m_value,m_decimals)+" "+m_unit;
    p.setFont(InstrumentVisuals::numericFont(font(),23,reading,119));
    p.setPen(alarmActive()?InstrumentVisuals::tone(this, QColor(255,109,111)):accent);
    p.drawText(QRectF(218,11,121,31),Qt::AlignRight|Qt::AlignVCenter,reading);

    // Each inactive segment is still visible for easy scale reading. Values
    // beyond the configured range affect text but cannot exceed the bar.
    const qreal start=18, width=319, top=57, height=31;
    const qreal gap=3, segmentWidth=(width-gap*(m_segments-1))/m_segments;
    const double ratio=InstrumentVisuals::fraction(m_value,m_min,m_max);
    for(int i=0;i<m_segments;i++) {
        const double frac=double(i+1)/m_segments;
        const QRectF r(start+i*(segmentWidth+gap),top,segmentWidth,height);
        QColor c;
        if(frac*100 >= m_alarmPct)c=InstrumentVisuals::tone(this, QColor(255,97,101));
        else if(frac*100 >= m_warningPct)c=InstrumentVisuals::tone(this, QColor(255,184,75));
        else c=accent;
        if(frac>ratio+1.e-10)c=InstrumentVisuals::tone(this, QColor(45,64,80));
        p.setBrush(c);p.setPen(Qt::NoPen);
        p.drawRoundedRect(r,2,2);
        if(frac<=ratio+1.e-10) {
            QColor highlight(255,255,255,55);
            p.setBrush(highlight);
            p.drawRoundedRect(QRectF(r.left(),r.top()+1,r.width(),4),1,1);
        }
    }
    if(m_peakHold) {
        const double f=InstrumentVisuals::fraction(m_peak,m_min,m_max);
        const qreal x=start+width*f;
        p.setPen(QPen(InstrumentVisuals::tone(this, QColor(235,244,255)),2));
        p.drawLine(QPointF(x,51),QPointF(x,96));
    }
    const QFont scale=InstrumentVisuals::labelFont(font(),11);
    p.setFont(scale);p.setPen(InstrumentVisuals::tone(this, QColor(117,146,165)));
    p.drawText(QRectF(17,105,118,19),Qt::AlignLeft,
               InstrumentVisuals::number(m_min,0)+" "+m_unit);
    p.drawText(QRectF(220,105,117,19),Qt::AlignRight,
               InstrumentVisuals::number(m_max,0)+" "+m_unit);
    if(m_peakHold) {
        const QString label="PEAK " + InstrumentVisuals::number(m_peak,m_decimals);
        p.setFont(InstrumentVisuals::labelFont(font(),10,true));
        p.setPen(InstrumentVisuals::tone(this, QColor(149,178,196)));
        p.drawText(QRectF(110,107,135,15),Qt::AlignCenter,label);
    }
}
