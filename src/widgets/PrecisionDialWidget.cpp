#include "PrecisionDialWidget.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QSizePolicy>
#include <QRadialGradient>
#include <cmath>

PrecisionDialWidget::PrecisionDialWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
}
void PrecisionDialWidget::configure(const QString& title,double value,double minimum,double maximum,
                                    const QString& unit,const QString& accent,int decimals,
                                    bool alarmEnabled,double alarmHigh,bool showTicks) {
    if(!std::isfinite(value)||!std::isfinite(minimum)||!std::isfinite(maximum)||
       !std::isfinite(alarmHigh)||maximum<=minimum)return;
    m_title=title;m_value=value;m_min=minimum;m_max=maximum;m_unit=unit;m_accent=accent;
    m_decimals=qBound(0,decimals,4);m_alarmEnabled=alarmEnabled;
    m_alarmHigh=alarmHigh;m_showTicks=showTicks;
    update();
}
void PrecisionDialWidget::setValue(double value) {
    if(!std::isfinite(value)||value==m_value)return;
    m_value=value;update();
}
bool PrecisionDialWidget::alarmActive() const {return m_alarmEnabled && m_value>=m_alarmHigh;}
void PrecisionDialWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    InstrumentVisuals::startPaint(p,this,320,260);
    InstrumentVisuals::background(p,QRectF(0,0,320,260),13,this);
    const QColor accent=InstrumentVisuals::accent(m_accent);
    const QColor lit=alarmActive()?InstrumentVisuals::tone(this, QColor(255,105,108)):accent;
    // Header is outside the gauge sweep: never obscures tick marks or readings.
    const QFont titleFont=InstrumentVisuals::labelFont(font(),16,true);
    p.setFont(titleFont);p.setPen(InstrumentVisuals::tone(this, QColor(197,214,228)));
    p.drawText(QRectF(18,12,248,26),Qt::AlignVCenter|Qt::AlignLeft,
               InstrumentVisuals::elide(titleFont,m_title,248));
    p.setPen(Qt::NoPen);p.setBrush(lit);
    p.drawEllipse(QRectF(287,17,9,9));

    constexpr qreal cx=160.0,cy=146.0;
    const qreal radius=91.0;
    // Angular sweep: lower left (585°) through the top to lower right
    // (315°). Low readings start on the LEFT, high readings on the RIGHT.
    const QRectF arc(cx-radius,cy-radius,2*radius,2*radius);
    p.setPen(QPen(InstrumentVisuals::tone(this, QColor(47,66,82)),13,Qt::SolidLine,Qt::RoundCap));
    p.setBrush(Qt::NoBrush);p.drawArc(arc,315*16,270*16);
    const double ratio=InstrumentVisuals::fraction(m_value,m_min,m_max);
    // Short arcs give a subtle gradient without storing rendered pixmaps.
    const int count=qMax(1,int(std::round(100*ratio)));
    for(int i=0;i<count && ratio>0;i++) {
        const double t=(i+0.5)/100.0;
        QColor c=accent;
        if(m_alarmEnabled && m_min+t*(m_max-m_min)>=m_alarmHigh)
            c=InstrumentVisuals::tone(this, QColor(255,105,108));
        c.setAlphaF(0.55+0.45*t);
        p.setPen(QPen(c,10,Qt::SolidLine,Qt::FlatCap));
        p.drawArc(arc,int((585-(i+1)*2.7)*16),int(2.85*16));
    }
    if(m_showTicks) {
        for(int i=0;i<=10;i++) {
            const qreal a=(585.0-i*27.0)*3.14159265358979323846/180.0;
            const QPointF a0(cx+std::cos(a)*109,cy-std::sin(a)*109);
            const QPointF a1(cx+std::cos(a)*116,cy-std::sin(a)*116);
            p.setPen(QPen(i==0||i==10?InstrumentVisuals::tone(this, QColor(190,213,228)):InstrumentVisuals::tone(this, QColor(102,130,149)),
                          i==0||i==10?2.2:1.3,Qt::SolidLine,Qt::RoundCap));
            p.drawLine(a0,a1);
        }
    }
    // Needle terminates near the arc; it uses the full linear range and clamps
    // graphically without clamping the underlying actual reading.
    const double radians=(585-270*ratio)*3.14159265358979323846/180.;
    const QPointF tip(cx+std::cos(radians)*77,cy-std::sin(radians)*77);
    p.setPen(QPen(lit,2.8,Qt::SolidLine,Qt::RoundCap));
    p.drawLine(QPointF(cx,cy),tip);
    QRadialGradient hub(QPointF(cx-2,cy-2),10);
    hub.setColorAt(0,InstrumentVisuals::tone(this, QColor(228,243,252)));hub.setColorAt(1,InstrumentVisuals::tone(this, QColor(75,101,118)));
    p.setBrush(hub);p.setPen(QPen(InstrumentVisuals::tone(this, QColor(14,28,39)),2));
    p.drawEllipse(QPointF(cx,cy),8,8);

    const QString reading=InstrumentVisuals::number(m_value,m_decimals);
    p.setFont(InstrumentVisuals::numericFont(font(),32,reading,174));
    p.setPen(lit);
    p.drawText(QRectF(73,163,174,38),Qt::AlignHCenter|Qt::AlignVCenter,reading);
    const QFont unitFont=InstrumentVisuals::labelFont(font(),12,true);
    p.setFont(unitFont);p.setPen(InstrumentVisuals::tone(this, QColor(162,184,200)));
    p.drawText(QRectF(105,198,110,19),Qt::AlignCenter,
               InstrumentVisuals::elide(unitFont,m_unit,108));
    const QFont f=InstrumentVisuals::labelFont(font(),12);
    p.setFont(f);p.setPen(InstrumentVisuals::tone(this, QColor(124,149,167)));
    p.drawText(QRectF(12,226,90,21),Qt::AlignLeft|Qt::AlignVCenter,
               InstrumentVisuals::number(m_min,1));
    p.drawText(QRectF(218,226,90,21),Qt::AlignRight|Qt::AlignVCenter,
               InstrumentVisuals::number(m_max,1));
    if(alarmActive()) {
        p.setFont(InstrumentVisuals::labelFont(font(),11,true));
        p.setPen(InstrumentVisuals::tone(this, QColor(255,121,116)));
        p.drawText(QRectF(112,231,96,16),Qt::AlignCenter,QStringLiteral("HIGH ALARM"));
    }
}
