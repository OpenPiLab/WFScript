#pragma once

#include <QVector>
#include <limits>

struct ScopeMetrics {
    double minimum = 0.0;
    double maximum = 0.0;
    double vpp = 0.0;
    double average = 0.0;
    double rms = 0.0;
    double frequencyHz = std::numeric_limits<double>::quiet_NaN();
    int samples = 0;
};

namespace ScopeMath {
ScopeMetrics measure(const QVector<double>& samples, double sampleRateHz);
}
