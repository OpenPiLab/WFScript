#include "ScopeMetricsFormatter.h"
#include "../ScopeMath.h"
#include <cmath>

QString formatScopeMetrics(const ScopeFrame& frame) {
    const auto display=[&](const QString& label,const QVector<double>& values)->QString{
        const auto m=ScopeMath::measure(values,frame.sampleRateHz);
        if(m.samples==0)return label+QStringLiteral(": no data");
        const QString freq=std::isfinite(m.frequencyHz)
            ?QStringLiteral("%1 Hz").arg(m.frequencyHz,0,'f',1)
            :QStringLiteral("N/A (insufficient edges)");
        return QStringLiteral("%1  Min %2 V    Max %3 V    Vpp %4 V    RMS %5 V    DC %6 V    Freq %7")
            .arg(label).arg(m.minimum,0,'f',4).arg(m.maximum,0,'f',4)
            .arg(m.vpp,0,'f',4).arg(m.rms,0,'f',4).arg(m.average,0,'f',4).arg(freq);
    };
    QString triggerState;
    if(frame.triggerStatusAvailable)
        triggerState+=frame.autoTriggered
            ?QStringLiteral("  |  AUTO TIMEOUT")
            :QStringLiteral("  |  SDK: not Auto timeout");
    if(frame.edgeCheckAvailable) {
        if(frame.hasMatchingEdge)
            triggerState+=QStringLiteral("  |  %1 edge offset %2 samples (%3 us)")
                .arg(frame.trigger.rising?QStringLiteral("rising"):QStringLiteral("falling"))
                .arg(frame.nearestEdgeOffsetSamples,0,'f',1)
                .arg(frame.nearestEdgeOffsetSamples/frame.sampleRateHz*1e6,0,'f',1);
        else triggerState+=QStringLiteral("  |  selected edge not found");
    }
    if(frame.triggerPositionStatusAvailable)
        triggerState+=QStringLiteral("  |  HW position %1 us")
            .arg(frame.measuredTriggerPositionSeconds*1e6,0,'f',2);
    return display(QStringLiteral("CH1"),frame.ch1)+QStringLiteral("\n")
        +display(QStringLiteral("CH2"),frame.ch2)+triggerState;
}
