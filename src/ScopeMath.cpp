#include "ScopeMath.h"

#include <cmath>
#include <algorithm>

ScopeMetrics ScopeMath::measure(const QVector<double>& samples, double sampleRateHz) {
    ScopeMetrics m;
    // Online mean and RMS with a long-double accumulator to reduce cancellation.
    long double sum = 0.0L;
    long double squared = 0.0L;
    bool first = true;
    for (const double v : samples) {
        if (!std::isfinite(v)) continue;
        if (first) { m.minimum = m.maximum = v; first = false; }
        m.minimum = std::min(m.minimum, v);
        m.maximum = std::max(m.maximum, v);
        sum += v;
        squared += static_cast<long double>(v)*v;
        ++m.samples;
    }
    if (!m.samples) return m;
    m.average = static_cast<double>(sum/m.samples);
    m.rms = std::sqrt(static_cast<double>(squared/m.samples));
    m.vpp = m.maximum - m.minimum;
    if (!std::isfinite(sampleRateHz) || sampleRateHz <= 0 || m.vpp < 0.03 || samples.size() < 8)
        return m;

    // Hysteretic rising-mean crossings. The hysteresis rejects small noise
    // near the threshold. Interpolation gives sub-sample crossing times.
    const double threshold = (m.maximum + m.minimum)/2.0;
    const double lower = threshold - 0.10*m.vpp;
    bool armed = false;
    QVector<double> crossings;
    crossings.reserve(32);
    for (int i = 1; i < samples.size(); ++i) {
        const double a = samples.at(i-1), b = samples.at(i);
        if (!std::isfinite(a) || !std::isfinite(b)) continue;
        if (a < lower || b < lower) armed = true;
        if (armed && a <= threshold && b > threshold) {
            const double fraction = (b != a) ? (threshold-a)/(b-a) : 0.0;
            crossings.append(double(i-1) + fraction);
            armed = false;
        }
    }
    if (crossings.size() >= 2) {
        const double samplePeriod = (crossings.last()-crossings.first()) / double(crossings.size()-1);
        if (samplePeriod > 0)
            m.frequencyHz = sampleRateHz/samplePeriod;
    }
    return m;
}
