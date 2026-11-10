#include "StatusBeaconWidget.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QSizePolicy>
#include <QRadialGradient>
#include <cmath>

StatusBeaconWidget::StatusBeaconWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
}
void StatusBeaconWidget::configure(const QString& title,double value,const QString& activeColor,
                                   const QString& onText,const QString& offText,
                                   bool invert,bool showGlow) {
    if(!std::isfinite(value))return;
    m_title=title;m_rawActive=(value!=0.0);m_color=activeColor;
    m_onText=onText;m_offText=offText;m_invert=invert;m_showGlow=showGlow;
    update();
}
void StatusBeaconWidget::setValue(double value) {
    if(!std::isfinite(value))return;
    const bool state=(value!=0.0);
    if(m_rawActive==state)return;
    m_rawActive=state;update();
}
void StatusBeaconWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    InstrumentVisuals::startPaint(p,this,290,120);
    InstrumentVisuals::background(p,QRectF(0,0,290,120),13,this);
    const bool on=active();
    const QColor color=on?InstrumentVisuals::accent(m_color):InstrumentVisuals::tone(this, QColor(88,105,119));
    constexpr QPointF center(56,60);
    if(on && m_showGlow) {
        QRadialGradient glow(center,52);
        QColor edge=color;edge.setAlpha(0);
        QColor mid=color;mid.setAlpha(47);
        QColor core=color;core.setAlpha(100);
        glow.setColorAt(0,core);glow.setColorAt(.45,mid);glow.setColorAt(1,edge);
        p.setPen(Qt::NoPen);p.setBrush(glow);p.drawEllipse(center,52,52);
    }
    p.setBrush(InstrumentVisuals::tone(this, QColor(8,16,24)));p.setPen(QPen(InstrumentVisuals::tone(this, QColor(87,106,124)),2.5));
    p.drawEllipse(center,32,32);
    QRadialGradient led(QPointF(48,51),40);
    if(on) {
        led.setColorAt(0,color.lighter(168));led.setColorAt(.55,color);
        led.setColorAt(1,color.darker(240));
    } else {
        led.setColorAt(0,InstrumentVisuals::tone(this, QColor(87,106,119)));led.setColorAt(.66,InstrumentVisuals::tone(this, QColor(44,59,71)));
        led.setColorAt(1,InstrumentVisuals::tone(this, QColor(21,34,45)));
    }
    p.setBrush(led);p.setPen(QPen(InstrumentVisuals::tone(this, QColor(11,26,37)),2));
    p.drawEllipse(center,25.5,25.5);
    if(on) {
        QColor shine(240,252,255,150);
        p.setBrush(shine);p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(42,40,12,6));
    }
    const QFont header=InstrumentVisuals::labelFont(font(),14,true);
    p.setFont(header);p.setPen(InstrumentVisuals::tone(this, QColor(185,207,224)));
    p.drawText(QRectF(108,19,166,24),Qt::AlignVCenter,
               InstrumentVisuals::elide(header,m_title,166));
    const QString state=on?m_onText:m_offText;
    const QFont stateFont=InstrumentVisuals::numericFont(font(),24,state,166);
    p.setFont(stateFont);p.setPen(on?color:InstrumentVisuals::tone(this, QColor(126,147,166)));
    p.drawText(QRectF(107,44,170,39),Qt::AlignLeft|Qt::AlignVCenter,
               InstrumentVisuals::elide(stateFont,state,169));
    p.setFont(InstrumentVisuals::labelFont(font(),10,true));
    p.setPen(InstrumentVisuals::tone(this, QColor(103,132,151)));
    p.drawText(QRectF(108,86,161,16),Qt::AlignLeft|Qt::AlignVCenter,
               on?QStringLiteral("STATUS  •  ACTIVE"):QStringLiteral("STATUS  •  STANDBY"));
}
