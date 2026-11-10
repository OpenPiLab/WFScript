#pragma once

// Visual helpers local to native, persistent, custom-painted instrument widgets.
// No widget owns external font resources, timers, or a duplicate Page-state cache.
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QRectF>
#include <QWidget>
#include <QString>
#include <QtGlobal>
#include <QVariant>

namespace InstrumentVisuals {
inline bool light(const QWidget* w) {
    // Walk parent hierarchy: all controls on a Page inherit its appearance
    // without storing a duplicate theme or being reconstructed.
    for(const QWidget* current=w;current;current=current->parentWidget()) {
        const QVariant v=current->property("wfPanelLight");
        if(v.isValid())return v.toBool();
    }
    return false;
}
inline QColor tone(const QWidget* w, const QColor& c) {
    if(!light(w))return c;
    // Remap neutral tones so existing dark instrument designs have legible
    // ink, tracks and surfaces in light mode; keep semantic LED/accent hues.
    if(c.hsvSaturationF()>0.48)return c;
    const int v=c.value();
    QColor out;
    if(v<42)out=QColor(237,244,250);           // deep surfaces -> paper
    else if(v<96)out=QColor(190,206,220);     // recessed/raised edges
    else if(v<155)out=QColor(115,143,166);    // secondary labels/ticks
    else out=QColor(34,57,77);                 // bright dark-mode ink -> ink
    out.setAlpha(c.alpha());
    return out;
}

inline QColor accent(const QString& name) {
    if(name==QLatin1String("green")) return QColor(67,221,157);
    if(name==QLatin1String("amber")) return QColor(255,193,91);
    if(name==QLatin1String("blue")) return QColor(108,162,255);
    if(name==QLatin1String("magenta")) return QColor(215,136,251);
    if(name==QLatin1String("red")) return QColor(255,105,108);
    if(name==QLatin1String("white")) return QColor(227,238,247);
    return QColor(82,219,234);
}
inline void background(QPainter& p, const QRectF& r, qreal corner=13, const QWidget* w=nullptr) {
    QLinearGradient g(r.topLeft(), r.bottomRight());
    if(light(w)) {
        g.setColorAt(0,QColor(255,255,255));
        g.setColorAt(.42,QColor(246,250,253));
        g.setColorAt(1,QColor(227,237,245));
    } else {
        g.setColorAt(0,QColor(34,48,65));
        g.setColorAt(.42,QColor(20,33,48));
        g.setColorAt(1,QColor(10,18,29));
    }
    p.setPen(QPen(light(w)?QColor(170,190,207):QColor(65,85,103),1));
    p.setBrush(g);
    p.drawRoundedRect(r.adjusted(.5,.5,-.5,-.5),corner,corner);
}
inline QFont labelFont(const QFont& base, int px, bool bold=false) {
    QFont f(base);
    f.setPixelSize(qMax(5,px));
    f.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    return f;
}
inline QFont numericFont(const QFont& base, int maxPixels, const QString& value, qreal width) {
    QFont f(base);
    f.setFamily(QStringLiteral("Consolas"));
    f.setStyleHint(QFont::Monospace);
    f.setWeight(QFont::DemiBold);
    for(int px=maxPixels; px>=9; --px) {
        f.setPixelSize(px);
        if(QFontMetricsF(f).horizontalAdvance(value)<=width)break;
    }
    return f;
}
inline QString elide(const QFont& f,const QString& s, qreal width) {
    return QFontMetricsF(f).elidedText(s,Qt::ElideRight,qMax(0,int(width)));
}
inline qreal fraction(double value,double min,double max) {
    if(!(max>min))return 0;
    return qBound(0.0,(value-min)/(max-min),1.0);
}
inline QString number(double v,int decimals) {
    if(qAbs(v)>=1.0e9)return QString::number(v,'e',qBound(0,decimals,4));
    return QString::number(v,'f',qBound(0,decimals,5));
}
inline void startPaint(QPainter& p, QWidget* w, qreal logicalWidth, qreal logicalHeight) {
    // Isotropic scaling keeps circles circular and makes all typography scale
    // with the actual widget dimensions. Centered when aspect ratios differ.
    // Transparent corners: Qt composites the unchanged Page background.
    const qreal s=qMin(w->width()/logicalWidth, w->height()/logicalHeight);
    p.translate((w->width()-logicalWidth*s)/2.0,(w->height()-logicalHeight*s)/2.0);
    p.scale(s,s);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setRenderHint(QPainter::TextAntialiasing,true);
}
}
