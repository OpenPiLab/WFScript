#include "ScopeAcquisitionWorker.h"
#include "DwfRuntime.h"
#include "ScopeMath.h"

#include <QElapsedTimer>
#include <QThread>
#include <QtGlobal>
#include <cmath>
#include <algorithm>
#include <limits>

namespace {
constexpr double kTwoPi = 6.2831853071795864769;

// Diagnostics never change or rotate the samples returned by the SDK.
// Measure both slopes independently so a 180-degree misalignment is visible.
void checkTriggerEdge(ScopeFrame& frame) {
    if (frame.trigger.mode == ScopeTriggerMode::None) return;
    const QVector<double>& data = frame.trigger.channel == 0 ? frame.ch1 : frame.ch2;
    if (data.size() < 16 || frame.sampleRateHz <= 0) return;
    const double expected = frame.trigger.positionPercent / 100.0 * (data.size()-1);
    double bestMatching = std::numeric_limits<double>::infinity();
    double bestOpposite = std::numeric_limits<double>::infinity();
    for (qsizetype i = 1; i < data.size(); ++i) {
        const double a = data.at(i-1), b = data.at(i);
        if (!std::isfinite(a) || !std::isfinite(b) || a == b) continue;
        const double level = frame.trigger.levelVolts;
        const bool rising = a <= level && b > level;
        const bool falling = a >= level && b < level;
        if (!rising && !falling) continue;
        const double crossing = double(i-1) + (level-a)/(b-a);
        const double offset = crossing-expected;
        double& best = (rising == frame.trigger.rising) ? bestMatching : bestOpposite;
        if (std::abs(offset) < std::abs(best)) best = offset;
    }
    frame.edgeCheckAvailable = true;
    frame.hasMatchingEdge = std::isfinite(bestMatching);
    frame.hasOppositeEdge = std::isfinite(bestOpposite);
    frame.nearestEdgeOffsetSamples = bestMatching;
    frame.nearestOppositeEdgeOffsetSamples = bestOpposite;
    const ScopeMetrics metrics = ScopeMath::measure(data, frame.sampleRateHz);
    // The threshold is intentionally generous (22% of one period) to allow
    // trigger-jitter and ADC interpolation, but not a half-cycle phase error.
    const double period = std::isfinite(metrics.frequencyHz) && metrics.frequencyHz > 0
        ? frame.sampleRateHz / metrics.frequencyHz : 0.0;
    const double tolerance = period >= 6.0 ? std::max(4.0, 0.22*period) : 4.0;
    frame.edgeAtRequestedPosition = frame.hasMatchingEdge &&
                                    std::abs(bestMatching) <= tolerance;
}

}

ScopeAcquisitionWorker::ScopeAcquisitionWorker(QObject* parent) : QObject(parent) {}

void ScopeAcquisitionWorker::execute(ScopeCaptureConfig config) {
    DwfRuntime runtime; // constructed, used, destroyed exclusively on worker thread
    QString error;
    bool generatorRunning = false;
    auto finish = [&](const QString& message, bool failed) {
        if (generatorRunning) {
            QString stopError;
            if (!runtime.stopWavegen(0, &stopError))
                emit status(QStringLiteral("WaveGen stop warning: %1").arg(stopError));
        }
        runtime.close();
        emit finished(message, failed);
    };
    if (config.physicalDevice) {
        if (!runtime.open(config.deviceIndex, config.configIndex, &error)) {
            finish(QStringLiteral("Hardware open failed: %1").arg(error), true);
            return;
        }
        emit status(QStringLiteral("Scope connected: %1, config %2")
                        .arg(runtime.deviceName()).arg(config.configIndex+1));
        if (config.testWavegen) {
            if (!runtime.setupWavegenSine(0, config.testFrequencyHz,
                                          config.testAmplitudeV, 0, &error)
                || !runtime.startWavegen(0, &error)) {
                finish(QStringLiteral("Test WaveGen failed: %1").arg(error), true);
                return;
            }
            generatorRunning = true;
            emit status(QStringLiteral("W1 test generator running at %1 Hz, %2 V peak. Wire W1 to 1+ and GND to 1− for CH1 loopback.")
                            .arg(config.testFrequencyHz).arg(config.testAmplitudeV));
        }
        if (!runtime.setupScope(config.sampleRateHz, config.samples, config.inputRange, config.inputRange,
                                0, 0, 1, 1, &error) ||
            !runtime.configureScopeTrigger(config.trigger, &error)) {
            finish(QStringLiteral("Scope configuration failed: %1").arg(error), true);
            return;
        }
        emit status(QStringLiteral("Trigger: %1  CH%2  %3 edge  %4 V  hysteresis %5 V  at %6% of capture")
            .arg(config.trigger.mode == ScopeTriggerMode::None ? QStringLiteral("None") :
                 config.trigger.mode == ScopeTriggerMode::Normal ? QStringLiteral("Normal") : QStringLiteral("Auto"))
            .arg(config.trigger.channel + 1)
            .arg(config.trigger.rising ? QStringLiteral("rising") : QStringLiteral("falling"))
            .arg(config.trigger.levelVolts, 0, 'f', 2)
            .arg(config.trigger.hysteresisVolts, 0, 'f', 3)
            .arg(config.trigger.positionPercent, 0, 'f', 0));
        int detectorType = -1;
        double detectorLevel = 0.0, detectorHysteresis = 0.0;
        QString detectorError;
        if (runtime.scopeTriggerDetectorReadback(&detectorType, &detectorLevel, &detectorHysteresis, &detectorError)) {
            emit status(QStringLiteral("Detector readback: type=%1 (edge=0), level=%2 V, hysteresis=%3 V")
                .arg(detectorType).arg(detectorLevel,0,'f',4).arg(detectorHysteresis,0,'f',4));
            // This is diagnostic only. Low-level quantization should not prevent acquisition.
            if (detectorType != 0 || std::abs(detectorLevel-config.trigger.levelVolts)>0.02 ||
                std::abs(detectorHysteresis-config.trigger.hysteresisVolts)>0.025)
                emit status(QStringLiteral("Detector readback differs from requested settings; check SDK configuration support"));
        } else {
            emit status(QStringLiteral("Detector readback unavailable: %1").arg(detectorError));
        }
    } else {
        emit status(QStringLiteral("Scope simulation: no hardware access"));
    }
    quint64 sequence = 0;
    quint64 selectedEdgeFrames = 0, oppositeEdgeFrames = 0, otherFrames = 0;
    QElapsedTimer timer;
    timer.start();
    while (!m_stop.load()) {
        ScopeFrame frame;
        frame.sampleRateHz = config.physicalDevice ? runtime.scopeRateHz() : config.sampleRateHz;
        frame.simulated = !config.physicalDevice;
        frame.sequence = ++sequence;
        frame.trigger = config.trigger;
        frame.triggerEnabled = config.trigger.mode != ScopeTriggerMode::None;
        if (config.physicalDevice) {
            if (!runtime.scopeStart(&error, &m_stop)) {
                finish(QStringLiteral("Scope start failed: %1").arg(error), true);
                return;
            }
            bool completed = false;
            QElapsedTimer waiting;
            waiting.start();
            bool reportedWaiting = false;
            while (!m_stop.load()) {
                if (!runtime.scopePoll(&completed, &error)) {
                    finish(QStringLiteral("Scope poll failed: %1").arg(error), true);
                    return;
                }
                if (completed) break;
                if (config.trigger.mode == ScopeTriggerMode::Normal) {
                    if (!reportedWaiting && waiting.elapsed() >= 500) {
                        emit status(QStringLiteral("Normal trigger: waiting for CH%1 %2 edge at %3 V (Stop is responsive)")
                            .arg(config.trigger.channel + 1)
                            .arg(config.trigger.rising ? QStringLiteral("rising") : QStringLiteral("falling"))
                            .arg(config.trigger.levelVolts, 0, 'f', 2));
                        reportedWaiting = true;
                    }
                    // Normal deliberately waits indefinitely for an edge; Stop remains responsive.
                } else if (waiting.elapsed() > qMax<qint64>(5000, qint64(config.trigger.autoTimeoutSeconds * 1000) + 2500)) {
                    finish(QStringLiteral("Scope acquisition timed out: %1")
                        .arg(config.trigger.mode == ScopeTriggerMode::Auto
                            ? QStringLiteral("auto trigger did not complete") : QStringLiteral("untriggered capture did not complete")), true);
                    return;
                }
                QThread::msleep(5);
            }
            if (m_stop.load()) break;
            if (config.trigger.mode != ScopeTriggerMode::None) {
                // Read even in Normal mode. Previously we assumed that any
                // completed Normal acquisition was necessarily edge-triggered.
                if (!runtime.scopeAutoTriggered(&frame.autoTriggered, &error)) {
                    finish(QStringLiteral("Reading trigger status failed: %1").arg(error), true);
                    return;
                }
                frame.triggerStatusAvailable = true;
                // PositionStatus is a device-reported per-acquisition value
                // (including sub-sample interpolation on analog trigger inputs).
                // A failure to get these diagnostics must not discard the data.
                QString posError;
                frame.triggerPositionStatusAvailable = runtime.scopeTriggerPosition(
                    &frame.configuredTriggerPositionSeconds,
                    &frame.measuredTriggerPositionSeconds, &posError);
                if (!frame.triggerPositionStatusAvailable && (sequence == 1))
                    emit status(QStringLiteral("Trigger position readback unavailable: %1").arg(posError));
                if (frame.autoTriggered && (sequence <= 3 || sequence % 60 == 0))
                    emit status(QStringLiteral("Auto capture #%1: SDK reports automatic timeout, "
                                               "not a detector edge").arg(sequence));
            }
            if (!runtime.readScope(0, runtime.scopeSamples(), frame.ch1, &error)
                || !runtime.readScope(1, runtime.scopeSamples(), frame.ch2, &error)) {
                finish(QStringLiteral("Scope read failed: %1").arg(error), true);
                return;
            }
        } else {
            frame.ch1.resize(config.samples);
            frame.ch2.resize(config.samples);
            const double amplitude = config.trigger.channel == 0 ? 1.0 : 0.45;
            const bool reachable = std::abs(config.trigger.levelVolts) < amplitude;
            if (config.trigger.mode == ScopeTriggerMode::Normal && !reachable) {
                emit status(QStringLiteral("Simulation: waiting for a trigger level the waveform never crosses. Press Stop or lower the level."));
                while (!m_stop.load()) QThread::msleep(15);
                break;
            }
            const bool aligned = config.trigger.mode != ScopeTriggerMode::None && reachable;
            const double angleAtTrigger = aligned
                ? (config.trigger.rising ? std::asin(config.trigger.levelVolts / amplitude)
                                        : 3.141592653589793 - std::asin(config.trigger.levelVolts / amplitude))
                : double(sequence) * 0.81;
            const double phase = angleAtTrigger - (config.trigger.channel == 1 ? 0.75 : 0.0);
            const double at = config.trigger.positionPercent / 100.0 * (config.samples - 1);
            for (int i = 0; i < config.samples; ++i) {
                const double t = (double(i) - (aligned ? at : 0.0)) / config.sampleRateHz;
                frame.ch1[i] = std::sin(kTwoPi*1000*t + phase) + 0.006*std::sin(kTwoPi*43017*t);
                frame.ch2[i] = 0.45*std::sin(kTwoPi*1000*t + phase + 0.75);
            }
        }
        if (config.physicalDevice) {
            checkTriggerEdge(frame);
            if (frame.edgeCheckAvailable && frame.trigger.mode != ScopeTriggerMode::None && !frame.autoTriggered) {
                if (frame.edgeAtRequestedPosition) ++selectedEdgeFrames;
                else if (frame.hasOppositeEdge && std::isfinite(frame.nearestOppositeEdgeOffsetSamples) &&
                         std::abs(frame.nearestOppositeEdgeOffsetSamples) <= 20.0) ++oppositeEdgeFrames;
                else ++otherFrames;
            }
            if (frame.edgeCheckAvailable && (sequence <= 3 || sequence % 60 == 0)) {
                const QString match = frame.hasMatchingEdge
                    ? QStringLiteral("%1 samples (%2 us)")
                          .arg(frame.nearestEdgeOffsetSamples,0,'f',1)
                          .arg(frame.nearestEdgeOffsetSamples/frame.sampleRateHz*1e6,0,'f',1)
                    : QStringLiteral("none");
                const QString opposite = frame.hasOppositeEdge
                    ? QStringLiteral("%1 samples").arg(frame.nearestOppositeEdgeOffsetSamples,0,'f',1)
                    : QStringLiteral("none");
                const QString sdkPos = frame.triggerPositionStatusAvailable
                    ? QStringLiteral("configured=%1 us, reported=%2 us")
                        .arg(frame.configuredTriggerPositionSeconds*1e6,0,'f',2)
                        .arg(frame.measuredTriggerPositionSeconds*1e6,0,'f',2)
                    : QStringLiteral("not available");
                const QString detection = frame.triggerStatusAvailable
                    ? (frame.autoTriggered ? QStringLiteral("AUTO TIMEOUT")
                                           : QStringLiteral("detector reported"))
                    : QStringLiteral("unknown");
                emit status(QStringLiteral("Trigger frame %1 | %2 | selected %3 edge offset: %4 "
                                           "| opposite edge: %5 | SDK position: %6")
                    .arg(sequence).arg(detection)
                    .arg(config.trigger.rising ? QStringLiteral("rising") : QStringLiteral("falling"))
                    .arg(match).arg(opposite).arg(sdkPos));
                if (sequence % 60 == 0) {
                    emit status(QStringLiteral("Trigger polarity totals (since Start): matching=%1, opposite=%2, other=%3")
                        .arg(selectedEdgeFrames).arg(oppositeEdgeFrames).arg(otherFrames));
                }
            }
        }
        emit frameReady(frame);
        if (!config.continuous) break;
        // Throttle both simulated and real acquisition to roughly 30 displayed frames/s;
        // responsive stopping is maintained with small sleeps.
        while (!m_stop.load() && timer.elapsed() < 33) QThread::msleep(2);
        timer.restart();
    }
    finish(m_stop.load() ? QStringLiteral("Scope stopped") : QStringLiteral("Single capture completed"), false);
}
