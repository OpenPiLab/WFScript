#pragma once

#include "ScopeFrame.h"
#include "ScopeTrigger.h"
#include <QObject>
#include <atomic>

struct ScopeCaptureConfig {
    bool physicalDevice = false;
    int deviceIndex = 0;
    int configIndex = 0;             // zero-based DWF configuration
    int samples = 4096;
    double sampleRateHz = 1'000'000;
    double inputRange = 5;
    bool continuous = true;
    ScopeTriggerSettings trigger;
    bool testWavegen = false;        // explicit opt-in; never generates by default
    double testFrequencyHz = 1000;
    double testAmplitudeV = 1;
};

class ScopeAcquisitionWorker final : public QObject {
    Q_OBJECT
public:
    explicit ScopeAcquisitionWorker(QObject* parent = nullptr);
    void requestStop() { m_stop.store(true); }
    void execute(ScopeCaptureConfig config);    // called only on worker thread
signals:
    void frameReady(const ScopeFrame& frame);
    void status(const QString& text);
    void finished(const QString& reason, bool error);
private:
    std::atomic_bool m_stop{false};
};
