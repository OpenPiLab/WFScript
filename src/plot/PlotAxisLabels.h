#pragma once
// RC1.5.4.1: Viewport-aware linear X-axis formatting. Tick spacing, not the
// absolute coordinate magnitude, determines displayed precision. Avoids
// visually repeated labels (e.g. 24.99m / 25m / 25m / 25.01m) at close zoom.
// Only the label strings change: ticks, transforms and samples are untouched.
#include <QString>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PlotAxisLabels {
inline QVector<QString> linearX(const QVector<double>& values) {
    QVector<QString> result;
    result.reserve(values.size());
    if(values.isEmpty()) return result;
    double maximum=0.0, smallest=std::numeric_limits<double>::infinity();
    for(int i=0;i<values.size();++i) {
        if(!std::isfinite(values[i])) continue;
        maximum=std::max(maximum,std::abs(values[i]));
        if(i>0 && std::isfinite(values[i-1])) {
            const double difference=std::abs(values[i]-values[i-1]);
            if(difference>0 && std::isfinite(difference))
                smallest=std::min(smallest,difference);
        }
    }
    double multiplier=1.0;
    QString suffix;
    // Keep one unit for all ticks within a viewport rather than mixing m/µ.
    if(maximum>=1e9) {multiplier=1e-9;suffix=QStringLiteral("G");}
    else if(maximum>=1e6) {multiplier=1e-6;suffix=QStringLiteral("M");}
    else if(maximum>=1e3) {multiplier=1e-3;suffix=QStringLiteral("k");}
    else if(maximum>0 && maximum<1e-6) {multiplier=1e9;suffix=QStringLiteral("n");}
    else if(maximum>0 && maximum<1e-3) {multiplier=1e6;suffix=QStringLiteral("µ");}
    else if(maximum>0 && maximum<0.1) {multiplier=1e3;suffix=QStringLiteral("m");}

    int precision=0;
    if(std::isfinite(smallest)) {
        const double interval=smallest*multiplier;
        if(interval>0 && interval<1.0)
            precision=std::clamp(int(std::ceil(-std::log10(interval)-1e-10)),0,14);
    }
    // An interval near a decimal rounding boundary may need another digit.
    // Check adjacent formatted labels rather than relying on log10 alone.
    for(;precision<14;++precision) {
        bool distinct=true;
        QString previous;
        for(int i=0;i<values.size();++i) {
            const QString label=std::isfinite(values[i]) ?
                QString::number(values[i]*multiplier,'f',precision)+suffix : QStringLiteral("—");
            if(i>0 && label==previous && values[i]!=values[i-1]) {distinct=false;break;}
            previous=label;
        }
        if(distinct) break;
    }
    for(double v:values) {
        if(!std::isfinite(v)) result.append(QStringLiteral("—"));
        else result.append(QString::number(v*multiplier,'f',precision)+suffix);
    }
    return result;
}
} // namespace PlotAxisLabels
