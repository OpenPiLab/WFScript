#include "DigitalDisplayWidget.h"
#include "InstrumentVisuals.h"
#include <QFontMetrics>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QtGlobal>
#include <cmath>

namespace {
QColor accentColor(const QString& name) {
    if(name==QStringLiteral("green")) return QColor(54, 220, 153);
    if(name==QStringLiteral("amber")) return QColor(255, 186, 80);
    if(name==QStringLiteral("blue")) return QColor(114, 153, 255);
    if(name==QStringLiteral("white")) return QColor(229, 238, 247);
    return QColor(75, 215, 237); // cyan
}
}
DigitalDisplayWidget::DigitalDisplayWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}
void DigitalDisplayWidget::configure(const QString& title, double value, int decimals,
                                     const QString& unit, const QString& accent,
                                     bool showPlus, bool groupThousands,
                                     bool alarmEnabled, double alarmHigh) {
    m_title=title;
    m_unit=unit;
    m_accent=accent;
    m_decimals=qBound(0,decimals,6);
    m_showPlus=showPlus;
    m_groupThousands=groupThousands;
    m_alarmEnabled=alarmEnabled;
    if(std::isfinite(alarmHigh))m_alarmHigh=alarmHigh;
    if(std::isfinite(value))m_value=value;
    update();
}
void DigitalDisplayWidget::setValue(double value) {
    if(!std::isfinite(value) || m_value==value)return;
    m_value=value;
    update();
}
QString DigitalDisplayWidget::formattedValue() const {
    // Use fixed notation for ordinary instrument readings, scientific notation
    // for very large values. This also avoids creating enormous text strings.
    QString number;
    if(std::abs(m_value)>=1.0e12)
        number=QString::number(m_value,'e',m_decimals);
    else if(m_groupThousands)
        number=QLocale(QLocale::English,QLocale::UnitedStates).toString(m_value,'f',m_decimals);
    else
        number=QString::number(m_value,'f',m_decimals);
    if(m_showPlus && m_value>0)number.prepend(QLatin1Char('+'));
    return number;
}
bool DigitalDisplayWidget::alarmActive() const {
    return m_alarmEnabled && m_value>=m_alarmHigh;
}
void DigitalDisplayWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF outer=QRectF(rect()).adjusted(1.0,1.0,-1.0,-1.0);
    if(outer.width()<4 || outer.height()<4)return;
    QLinearGradient background(outer.topLeft(),outer.bottomRight());
    if(InstrumentVisuals::light(this)) {
        background.setColorAt(0,QColor(255,255,255));
        background.setColorAt(1,QColor(230,240,248));
    } else {
        background.setColorAt(0,QColor(30,43,59));
        background.setColorAt(1,QColor(13,23,36));
    }
    painter.setBrush(background);
    painter.setPen(QPen(InstrumentVisuals::tone(this, QColor(67,82,100)),1.0));
    painter.drawRoundedRect(outer,9,9);

    const bool alarm=alarmActive();
    const QColor main=alarm?InstrumentVisuals::tone(this, QColor(255,94,91)):accentColor(m_accent);
    const qreal margin=qMax(6.0,qMin(13.0,width()*0.04));
    const QRectF inner=outer.adjusted(margin,margin,-margin,-margin);
    if(inner.width()<10 || inner.height()<12)return;
    // Narrow accent line; unlike a QSS decoration this scales with the QWidget.
    painter.setPen(Qt::NoPen);
    painter.setBrush(main);
    painter.drawRoundedRect(QRectF(inner.left(),inner.top(),3.0,inner.height()),1.5,1.5);
    const qreal contentLeft=inner.left()+11.0;
    const qreal contentRight=inner.right()-3.0;
    const qreal availableW=qMax(1.0,contentRight-contentLeft);

    // Compact title and status use text elision, not fixed-width label controls.
    const int headingPx=qBound(8,int(height()*0.115),15);
    QFont heading=font();heading.setPixelSize(headingPx);heading.setWeight(QFont::DemiBold);
    painter.setFont(heading);
    painter.setPen(InstrumentVisuals::tone(this, QColor(176,193,210)));
    const QFontMetrics headingMetrics(heading);
    const QString caption=headingMetrics.elidedText(m_title,Qt::ElideRight,int(availableW));
    const int headingY=int(inner.top()+headingMetrics.ascent()+1);
    painter.drawText(QPointF(contentLeft,headingY),caption);

    const QString reading=formattedValue();
    // The main readout and unit share a baseline and shrink to fit narrow widgets.
    int fontPx=qBound(12,int(height()*0.35),60);
    QFont digits=font();digits.setWeight(QFont::DemiBold);
    digits.setStyleHint(QFont::Monospace);
    digits.setFamily(QStringLiteral("Consolas"));
    const int unitPx=qBound(8,int(height()*0.13),18);
    QFont units=font();units.setPixelSize(unitPx);units.setWeight(QFont::DemiBold);
    const QFontMetrics unitsFm(units);
    const int unitWidth=m_unit.isEmpty()?0:qMin(int(availableW/3),unitsFm.horizontalAdvance(m_unit)+8);
    const qreal numberWidth=qMax(6.0,availableW-unitWidth-2);
    while(fontPx>11) {
        digits.setPixelSize(fontPx);
        if(QFontMetrics(digits).horizontalAdvance(reading)<=numberWidth)break;
        --fontPx;
    }
    digits.setPixelSize(fontPx);
    const QRectF numberRect(contentLeft,inner.top()+inner.height()*0.29,
                            numberWidth,inner.height()*0.52);
    painter.setPen(main);
    painter.setFont(digits);
    painter.drawText(numberRect,Qt::AlignVCenter|Qt::AlignRight,reading);
    if(unitWidth>0) {
        painter.setPen(InstrumentVisuals::tone(this, QColor(177,196,211)));
        painter.setFont(units);
        const QRectF unitRect(contentRight-unitWidth,numberRect.top(),unitWidth,numberRect.height());
        painter.drawText(unitRect,Qt::AlignRight|Qt::AlignVCenter,
                         unitsFm.elidedText(m_unit,Qt::ElideRight,unitWidth));
    }
    if(height()>=76) {
        QFont status=font();status.setPixelSize(qBound(7,int(height()*0.09),12));
        painter.setFont(status);
        painter.setPen(alarm?InstrumentVisuals::tone(this, QColor(255,114,108)):InstrumentVisuals::tone(this, QColor(111,141,159)));
        const QString s=alarm?QStringLiteral("HIGH ALARM") : QStringLiteral("DIGITAL MEASUREMENT");
        const QFontMetrics fm(status);
        painter.drawText(QPointF(contentLeft,inner.bottom()-2),
                         fm.elidedText(s,Qt::ElideRight,int(availableW)));
    }
}
