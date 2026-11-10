#pragma once

#include "ScopeTrigger.h"
#include <QMetaType>
#include <QVector>
#include <QtGlobal>

// Immutable-by-convention data passed from acquisition/script workers to the UI.
// Frame channels contain calibrated Volts from the public DWF SDK.
struct ScopeFrame {
    QVector<double> ch1;
    QVector<double> ch2;
    double sampleRateHz = 1'000'000.0;
    quint64 sequence = 0;
    bool simulated = false;
    bool triggerEnabled = false;
    bool autoTriggered = false; // true if AD3 forced an Auto acquisition without a matching edge
    bool triggerStatusAvailable = false;
    bool edgeCheckAvailable = false;
    bool edgeAtRequestedPosition = false;
    double nearestEdgeOffsetSamples = 0.0;
    double nearestOppositeEdgeOffsetSamples = 0.0;
    bool hasMatchingEdge = false;
    bool hasOppositeEdge = false;
    bool triggerPositionStatusAvailable = false;
    double configuredTriggerPositionSeconds = 0.0;
    double measuredTriggerPositionSeconds = 0.0;
    ScopeTriggerSettings trigger;
};
Q_DECLARE_METATYPE(ScopeFrame)
