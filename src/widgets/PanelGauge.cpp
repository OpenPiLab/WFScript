#include "PanelGauge.h"
#include <QPainter>
#include <algorithm>
#include <utility>

PanelGauge::PanelGauge(QWidget* parent):QWidget(parent) {setMinimumSize(80,70);}
void PanelGauge::configure(double lo,double hi,double value,QString title) {
    m_low=lo;m_high=hi;m_value=value;m_title=std::move(title);update();
}
void PanelGauge::setValue(double value) {m_value=value;update();}
void PanelGauge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF outer=rect().adjusted(9,9,-9,-9);
    if(outer.width()<10 || outer.height()<10)return;
    const QPointF c(outer.center().x(),outer.center().y()+outer.height()*.10);
    const double r=std::min(outer.width(),outer.height())*.38;
    const QRectF circle(c.x()-r,c.y()-r,2*r,2*r);
    p.setPen(QPen(QColor("#64748b"),qMax(2.0,r*.12),Qt::SolidLine,Qt::RoundCap));
    p.drawArc(circle,225*16,-270*16);
    const double fraction=m_high>m_low ? qBound(0.0,(m_value-m_low)/(m_high-m_low),1.0):0;
    p.setPen(QPen(QColor("#2da8ef"),qMax(2.0,r*.12),Qt::SolidLine,Qt::RoundCap));
    p.drawArc(circle,225*16,int(-270*16*fraction));
    p.setPen(palette().color(QPalette::WindowText));
    QFont f=p.font();f.setBold(true);f.setPointSizeF(qMax(9.0,r*.22));p.setFont(f);
    p.drawText(QRectF(c.x()-r,c.y()-r/4,2*r,r*.65),Qt::AlignCenter,QString::number(m_value,'f',2));
    f.setBold(false);f.setPointSizeF(qMax(8.0,r*.12));p.setFont(f);
    p.drawText(QRectF(c.x()-r,c.y()+r*.33,2*r,r*.4),Qt::AlignCenter,m_title);
}
