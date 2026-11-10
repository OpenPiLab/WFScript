#include "StatusBannerWidget.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QLinearGradient>
#include <utility>

StatusBannerWidget::StatusBannerWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
}
void StatusBannerWidget::configure(QString title,QString okText,QString warningText,QString alarmText,int level) {
    m_title=std::move(title);m_ok=std::move(okText);m_warning=std::move(warningText);
    m_alarm=std::move(alarmText);setLevel(level);
}
void StatusBannerWidget::setLevel(int level) {level=qBound(0,level,2);if(m_level!=level){m_level=level;update();}else update();}
void StatusBannerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);InstrumentVisuals::startPaint(p,this,330,100);
    InstrumentVisuals::background(p,QRectF(0,0,330,100),13,this);
    QColor accent=m_level==2?InstrumentVisuals::tone(this, QColor(255,97,105)):m_level==1?InstrumentVisuals::tone(this, QColor(255,190,83)):InstrumentVisuals::tone(this, QColor(69,218,155));
    QString state=m_level==2?m_alarm:m_level==1?m_warning:m_ok;
    QFont f=InstrumentVisuals::labelFont(font(),15,true);
    p.setFont(f);p.setPen(InstrumentVisuals::tone(this, QColor(191,215,231)));
    p.drawText(QRectF(17,9,292,21),Qt::AlignLeft,
               InstrumentVisuals::elide(f,m_title,290));
    QLinearGradient bar(0,37,0,91);
    bar.setColorAt(0,accent.darker(160));bar.setColorAt(1,accent.darker(255));
    p.setBrush(bar);p.setPen(QPen(accent,1.8));
    p.drawRoundedRect(QRectF(13,37,304,51),9,9);
    p.setBrush(accent);p.setPen(Qt::NoPen);p.drawEllipse(QPointF(35,63),9,9);
    f=InstrumentVisuals::numericFont(font(),25,state,245);p.setFont(f);
    p.setPen(InstrumentVisuals::tone(this, QColor(238,249,255)));
    p.drawText(QRectF(56,40,242,46),Qt::AlignVCenter|Qt::AlignHCenter,state);
}
