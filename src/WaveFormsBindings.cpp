#include "WaveFormsBindings.h"
#include "ScopeMath.h"
#include "ScopeAnalysis.h"
#include <QElapsedTimer>
#include <QThread>
#include <QtGlobal>
#include <cmath>
#include <algorithm>
#include <QLocale>
#include <QDateTime>
#include <limits>

static constexpr double kPi = 3.1415926535897932384626433832795;

QString ValueSetting::text() const {
    if (m_value == -1.0 && m_unit == QStringLiteral("V")) return QStringLiteral("Auto");
    const double a = std::abs(m_value);
    if (m_unit == QStringLiteral("Hz") && a >= 1e6)
        return QString::number(m_value/1e6, 'g', 5) + QStringLiteral(" MHz");
    if (m_unit == QStringLiteral("Hz") && a >= 1e3)
        return QString::number(m_value/1e3, 'g', 5) + QStringLiteral(" kHz");
    if (m_unit == QStringLiteral("s") && a < 0.01 && a > 0)
        return QString::number(m_value*1000, 'g', 5) + QStringLiteral(" ms");
    return QString::number(m_value, 'g', 8) + (m_unit.isEmpty() ? QString() : QStringLiteral(" ") + m_unit);
}
void ValueSetting::setValue(double v) {
    if (!std::isfinite(v) || m_value == v) return;
    m_value = v;
    emit valueChanged();
}
void ValueSetting::setReal(double v) {
    if (!std::isfinite(v) || m_real == v) return;
    m_real = v;
    emit realChanged();
}
void TextSetting::setText(const QString& v) {
    QString canonical = v;
    if (!m_choices.isEmpty()) {
        canonical.clear();
        for (const QString& choice : m_choices)
            if (choice.compare(v,Qt::CaseInsensitive)==0) { canonical=choice; break; }
        if (canonical.isEmpty()) return;
    }
    if (canonical == m_text) return;
    m_text = canonical;
    emit textChanged();
}
void TextSetting::setValue(int index) {
    if (index >= 0 && index < m_choices.size()) setText(m_choices[index]);
}
ScopeTriggerSettings ScopeTrigger::settings() const {
    ScopeTriggerSettings t;
    const QString mode = m_trigger.text();
    t.mode = mode == QStringLiteral("None") ? ScopeTriggerMode::None
        : mode == QStringLiteral("Normal") ? ScopeTriggerMode::Normal
        : ScopeTriggerMode::Auto;
    const int srcIndex=m_source.value();
    t.channel = srcIndex == 1 ? 1 : 0;
    const unsigned char sources[] = {2,2,7,8,11,12,1};
    t.source=sources[qBound(0,srcIndex,6)];
    t.type=m_type.value();
    t.lengthCondition=int(m_lcondition.value());
    t.lengthSeconds=m_length.value();
    t.holdoffSeconds=m_holdoff.value();
    t.filter=m_filter.value();
    t.rising = m_condition.text() != QStringLiteral("Falling");
    t.levelVolts = m_level.value();
    // Digilent's Auto hysteresis (-1) is not a valid DWF hysteresis voltage.
    t.hysteresisVolts = m_hysteresis.value() == -1 ? 0.05 : m_hysteresis.value();
    return t;
}
WavegenSimple::WavegenSimple() {
    QObject::connect(&m_frequency, &ValueSetting::valueChanged, this, [this]() {
        if (!m_syncPeriod && m_frequency.value() > 0) {
            m_syncPeriod=true; m_period.setValue(1.0 / m_frequency.value()); m_syncPeriod=false;
        }
    });
    QObject::connect(&m_period, &ValueSetting::valueChanged, this, [this]() {
        if (!m_syncPeriod && m_period.value() > 0) {
            m_syncPeriod=true; m_frequency.setValue(1.0 / m_period.value()); m_syncPeriod=false;
        }
    });
}

bool WavegenCustom::set(const QVariantList& samples) {
    if (samples.size() < 2 || samples.size() > 65536) return false;
    QVector<double> buffer; buffer.reserve(samples.size());
    for (const QVariant& item : samples) {
        bool ok = false;
        const double v = item.toDouble(&ok);
        if (!ok || !std::isfinite(v) || std::abs(v) > 1.0) return false;
        buffer.append(v);
    }
    m_samples = std::move(buffer);
    return true;
}
bool WavegenPlay::set(const QVariantList& values) {
    if(values.size()<16 || values.size()>500000) return false;
    QVector<double> tmp;tmp.reserve(values.size());
    for(const QVariant& v:values) {
        bool ok=false;const double d=v.toDouble(&ok);
        if(!ok || !std::isfinite(d) || d < -1.0 || d > 1.0) return false;
        tmp.append(d);
    }
    m_samples=std::move(tmp);return true;
}
double ScopeChannel::sample(int index) const {
    return index >= 0 && index < m_data.size() ? m_data[index] : std::numeric_limits<double>::quiet_NaN();
}
double ScopeChannel::timeOfIndex(int index) const {
    return index >= 0 && index < m_data.size() ? double(index)/m_sampleRateHz : std::numeric_limits<double>::quiet_NaN();
}
QVector<double> ScopeChannel::getData() const {
    // QList<double> / QVector<double> sequence; avoids QVariantList boxing.
    return m_data;
}
QVariantMap ScopeChannel::statistics() const {
    const ScopeMetrics s = ScopeMath::measure(m_data, m_sampleRateHz);
    QVariantMap out;
    out.insert(QStringLiteral("count"), s.samples);
    out.insert(QStringLiteral("min"), s.minimum);
    out.insert(QStringLiteral("max"), s.maximum);
    out.insert(QStringLiteral("vpp"), s.vpp);
    out.insert(QStringLiteral("mean"), s.average);
    out.insert(QStringLiteral("rms"), s.rms);
    out.insert(QStringLiteral("frequencyHz"), std::isfinite(s.frequencyHz)
               ? QVariant(s.frequencyHz) : QVariant());
    return out;
}

namespace {
bool scopeWindowIndices(int size,double rate,double start,double end,int& a,int& b) {
    if(size<=0 || !std::isfinite(rate) || rate<=0 || !std::isfinite(start) ||
       !std::isfinite(end) || start<0 || end<=start || end>double(size)/rate+1e-10)
        return false;
    a=std::clamp(int(std::floor(start*rate+1e-8)),0,size);
    b=std::clamp(int(std::ceil(end*rate-1e-8)),0,size);
    return b>a;
}
QVariantList analysisList(const std::vector<double>& values) {
    QVariantList result;result.reserve(qsizetype(values.size()));
    for(double x:values) result.append(x);
    return result;
}
}

double ScopeChannel::measure(const QString& name,double start,double end) const {
    return measureWindow(name,start,end);
}
double ScopeChannel::measureWindow(const QString& name,double start,double end) const {
    int a=0,b=0;
    if(!scopeWindowIndices(int(m_data.size()),m_sampleRateHz,start,end,a,b))
        return std::numeric_limits<double>::quiet_NaN();
    ScopeChannel temporary(m_index);
    temporary.setSamples(m_data.mid(a,b-a),m_sampleRateHz);
    return temporary.measure(name);
}
QVariantMap ScopeChannel::statistics(double start,double end) const {
    int a=0,b=0;
    if(!scopeWindowIndices(int(m_data.size()),m_sampleRateHz,start,end,a,b)) return {};
    ScopeChannel temporary(m_index);
    temporary.setSamples(m_data.mid(a,b-a),m_sampleRateHz);
    return temporary.statistics();
}
QVariantMap ScopeChannel::spectrum(int fftSize,const QString& window) const {
    try {
        const auto result=ScopeAnalysis::fft(m_data.constData(),size_t(m_data.size()),
                    m_sampleRateHz,fftSize>=0?size_t(fftSize):0,window.toStdString());
        return {{QStringLiteral("fftSize"),int(result.fftSize)},
                {QStringLiteral("sampleRateHz"),m_sampleRateHz},
                {QStringLiteral("window"),window},
                {QStringLiteral("binHz"),result.binHz},
                {QStringLiteral("peakFrequencyHz"),result.peakFrequencyHz},
                {QStringLiteral("peakAmplitude"),result.peakAmplitude},
                {QStringLiteral("frequencyHz"),analysisList(result.frequencyHz)},
                {QStringLiteral("amplitudePeak"),analysisList(result.amplitudePeak)},
                {QStringLiteral("amplitudeRms"),analysisList(result.amplitudeRms)},
                {QStringLiteral("powerV2"),analysisList(result.powerV2)},
                {QStringLiteral("decibels"),analysisList(result.decibels)}};
    } catch (const std::exception&) {
        // An empty result indicates invalid size, unsupported window or bad input.
        return {};
    }
}

double ScopeChannel::measure(const QString& type) const {
    const ScopeMetrics s = ScopeMath::measure(m_data,m_sampleRateHz);
    if (s.samples == 0) return std::numeric_limits<double>::quiet_NaN();
    const QString t = type.trimmed().toLower();
    if (t == QStringLiteral("average") || t == QStringLiteral("mean")) return s.average;
    if (t == QStringLiteral("rms") || t == QStringLiteral("true rms")) return s.rms;
    if (t == QStringLiteral("minimum") || t == QStringLiteral("min")) return s.minimum;
    if (t == QStringLiteral("maximum") || t == QStringLiteral("max")) return s.maximum;
    if (t == QStringLiteral("peak2peak") || t == QStringLiteral("peak to peak") ||
        t == QStringLiteral("peak-to-peak") || t == QStringLiteral("pk-pk") ||
        t == QStringLiteral("vpp")) return s.vpp;
    if (t == QStringLiteral("acrms") || t == QStringLiteral("ac rms"))
        return std::sqrt(std::max(0.0,s.rms*s.rms-s.average*s.average));
    if (t == QStringLiteral("crestfactor") || t == QStringLiteral("crest factor")) {
        const double ac=std::sqrt(std::max(0.0,s.rms*s.rms-s.average*s.average));
        return ac>0 ? std::max(std::abs(s.maximum-s.average),std::abs(s.minimum-s.average))/ac
                    : std::numeric_limits<double>::quiet_NaN();
    }
    if (t == QStringLiteral("variance"))
        return std::max(0.0,s.rms*s.rms-s.average*s.average);
    if (t == QStringLiteral("middle") || t == QStringLiteral("mid")) return 0.5*(s.minimum+s.maximum);
    if (t == QStringLiteral("amplitude")) return 0.5*s.vpp; // peak-to-peak/2 approximation
    if (t == QStringLiteral("frequency") || t == QStringLiteral("freq")) return s.frequencyHz;
    if (t == QStringLiteral("period")) return s.frequencyHz > 0 ? 1.0/s.frequencyHz
                                                             : std::numeric_limits<double>::quiet_NaN();
    if (t == QStringLiteral("median") || t == QStringLiteral("standard deviation") ||
        t == QStringLiteral("stddev") || t == QStringLiteral("low") || t == QStringLiteral("high")) {
        QVector<double> sorted=m_data;
        std::sort(sorted.begin(),sorted.end());
        if (t == QStringLiteral("median")) return sorted[sorted.size()/2];
        if (t == QStringLiteral("low")) return sorted[qBound(0,int(sorted.size()*0.10),int(sorted.size()-1))];
        if (t == QStringLiteral("high")) return sorted[qBound(0,int(sorted.size()*0.90),int(sorted.size()-1))];
        double variance=0.0;
        for (double x : m_data) variance+=(x-s.average)*(x-s.average);
        return std::sqrt(variance/m_data.size());
    }
    if (t == QStringLiteral("duty") || t == QStringLiteral("dutycycle") ||
        t == QStringLiteral("duty cycle")) {
        if (s.vpp <= 1e-9) return std::numeric_limits<double>::quiet_NaN();
        const double middle=(s.minimum+s.maximum)/2.0;
        int above=0;
        for (double x : m_data) if (x > middle) ++above;
        return 100.0*above/m_data.size();
    }
    if (t == QStringLiteral("risetime") || t == QStringLiteral("rise time") ||
        t == QStringLiteral("falltime") || t == QStringLiteral("fall time") ||
        t == QStringLiteral("positivewidth") || t == QStringLiteral("negativewidth") ||
        t == QStringLiteral("positive width") || t == QStringLiteral("negative width")) {
        if (m_data.size()<4 || s.vpp <= 1e-9) return std::numeric_limits<double>::quiet_NaN();
        const bool rising=(t.contains(QStringLiteral("rise")) || t.contains(QStringLiteral("positive")));
        const bool width=t.contains(QStringLiteral("width"));
        const double lo=s.minimum+(width?0.5:0.1)*s.vpp;
        const double hi=s.minimum+(width?0.5:0.9)*s.vpp;
        // Linear interpolation locates the first complete edge/width in the capture.
        // This is deliberately a first-edge measurement, not averaged over pulses.
        auto crossing=[this](double level, bool up, double after) {
            for (int i=1; i<m_data.size(); ++i) {
                const double x0=m_data[i-1],x1=m_data[i];
                if ((up && x0<level && x1>=level) || (!up && x0>level && x1<=level)) {
                    const double time=(i-1+(level-x0)/(x1-x0))/m_sampleRateHz;
                    if (time>after) return time;
                }
            }
            return std::numeric_limits<double>::quiet_NaN();
        };
        const double beginning=crossing(width?lo:(rising?lo:hi),rising,-1);
        if (!std::isfinite(beginning)) return beginning;
        const double end=crossing(width?hi:(rising?hi:lo), width?!rising:rising,beginning);
        return std::isfinite(end) ? end-beginning : std::numeric_limits<double>::quiet_NaN();
    }
    return std::numeric_limits<double>::quiet_NaN(); // Unsupported name, not silently zero.
}
QVariantMap ScopeBridge::captureInfo() const {
    return {{QStringLiteral("timestampUTC"),m_time.taken()},
            {QStringLiteral("timestampEpochMs"),m_time.takenEpochMs()},
            {QStringLiteral("timestampSource"),m_time.timestampSource()},
            {QStringLiteral("sampleCount"),m_ch1.getData().size()},
            {QStringLiteral("sampleRateHz"),m_ch1.sampleRateHz()},
            {QStringLiteral("captureDurationSeconds"),m_ch1.getData().size()/m_ch1.sampleRateHz()},
            {QStringLiteral("recordLostSamples"),recordLostSamples()},
            {QStringLiteral("recordCorruptSamples"),recordCorruptSamples()},
            {QStringLiteral("triggerTime"),m_lastTriggerTime}};
}
bool ScopeBridge::saveReference(int channelNumber) {
    if(channelNumber<1 || channelNumber>2) return false;
    const auto data=channelNumber==1 ? m_ch1.getData() : m_ch2.getData();
    if(data.isEmpty()) return false;
    m_reference[channelNumber-1]=data;
    m_referenceRateHz[channelNumber-1]=channelNumber==1 ? m_ch1.sampleRateHz():m_ch2.sampleRateHz();
    return true;
}
QVector<double> ScopeBridge::referenceData(int channelNumber) const {
    return channelNumber>=1 && channelNumber<=2 ? m_reference[channelNumber-1] : QVector<double>{};
}
QVector<double> ScopeBridge::mathData(const QString& operation) const {
    const QString op=operation.trimmed().toUpper().remove(' ');
    const QVector<double> a=m_ch1.getData(), b=m_ch2.getData();
    const bool ref1=op==QStringLiteral("CH1-REF1");
    const bool ref2=op==QStringLiteral("CH2-REF2");
    const bool subtract=op==QStringLiteral("CH1-CH2")||ref1||ref2;
    const bool sum=op==QStringLiteral("CH1+CH2");
    const bool multiply=op==QStringLiteral("CH1*CH2");
    if(!subtract&&!sum&&!multiply) return {};
    const QVector<double>& lhs=ref2 ? b : a;
    const QVector<double>& rhs=ref1 ? m_reference[0] : (ref2?m_reference[1]:b);
    if(lhs.isEmpty() || lhs.size()!=rhs.size()) return {};
    if(ref1 && m_referenceRateHz[0]!=m_ch1.sampleRateHz()) return {};
    if(ref2 && m_referenceRateHz[1]!=m_ch2.sampleRateHz()) return {};
    QVector<double> out(lhs.size());
    for(qsizetype i=0;i<lhs.size();++i)
        out[i]=sum?lhs[i]+rhs[i]:multiply?lhs[i]*rhs[i]:lhs[i]-rhs[i];
    return out;
}

QVariantMap ScopeBridge::hardwareStatus() {
    if (m_mock) return {{QStringLiteral("stateCode"),m_started?3:2},
        {QStringLiteral("rateHz"),m_time.sampleHz()},
        {QStringLiteral("deviceBufferSamples"),m_time.sampleCount()},
        {QStringLiteral("acquisitionMode"),
            m_time.acquisitionMode()==1?2:(m_time.acquisitionMode()==2?1:m_time.acquisitionMode())},
        {QStringLiteral("recordLostSamples"),0},
        {QStringLiteral("recordCorruptSamples"),0},
        {QStringLiteral("autoTriggered"),false}};
    return m_runtime ? m_runtime->scopeHardwareStatus(&m_error) : QVariantMap{};
}

QVariantMap ScopeBridge::capabilities() {
    if (m_mock) {
        QVariantMap mockCapabilities{{QStringLiteral("acquisitionModeMask"), 15},
                {QStringLiteral("channelFilterMask"), 7},
                {QStringLiteral("triggerTypeMask"), 7},
                {QStringLiteral("triggerSourceMask"), (1<<2)|(1<<7)|(1<<8)|(1<<11)|(1<<12)},
                {QStringLiteral("recordSupported"), true},
                {QStringLiteral("screenSupported"), true},
                {QStringLiteral("shiftSupported"), true},
                {QStringLiteral("minRateHz"), 1},
                {QStringLiteral("maxRateHz"), 100000000},
                {QStringLiteral("minBufferSamples"), 16},
                {QStringLiteral("maxBufferSamples"), 65536}};
        for (auto it=m_rangeCapabilities.constBegin();it!=m_rangeCapabilities.constEnd();++it)
            mockCapabilities.insert(it.key(),it.value());
        return mockCapabilities;
    }
    QVariantMap result=m_runtime ? m_runtime->scopeCapabilities(&m_error) : QVariantMap{};
    for (auto it=m_rangeCapabilities.constBegin();it!=m_rangeCapabilities.constEnd();++it)
        result.insert(it.key(),it.value());
    return result;
}
ScopeBridge::ScopeBridge(DwfRuntime* runtime, MockInstrumentState* mock, const std::atomic_bool* stop)
    : m_runtime(runtime), m_mock(mock), m_stop(stop) {
    // Query once after the hardware device is opened (both runtime creation
    // paths open before constructing ScopeBridge). No DWF on each JS read.
    m_rangeCapabilities = {{QStringLiteral("rangeStepsAvailable"),false},
                           {QStringLiteral("rangeSteps"),QVariantList{}}};
    if (m_runtime && m_runtime->connected()) {
        QString diagnostic;
        m_rangeCapabilities=m_runtime->scopeRangeCapabilities(&diagnostic);
        if (!diagnostic.isEmpty())
            m_rangeCapabilities.insert(QStringLiteral("rangeStepsError"),diagnostic);
    } else {
        m_rangeCapabilities.insert(QStringLiteral("rangeStepsError"),
                                   QStringLiteral("Simulation: no hardware range steps"));
    }
    const QVariantList steps=m_rangeCapabilities.value(QStringLiteral("rangeSteps")).toList();
    static_cast<ScopeRangeSetting*>(m_ch1.range())->setSteps(steps);
    static_cast<ScopeRangeSetting*>(m_ch2.range())->setSteps(steps);
}
bool ScopeBridge::config() {
    m_error.clear();
    const double requestedHz=m_time.sampleHz();
    const int effectiveSamples=m_time.sampleCount();
    const int modeIndex=m_time.acquisitionMode();
    // WaveForms order: Repeated, Screen, Shift, Record.
    const int sdkMode[] = {0,2,1,3};
    const int mode = sdkMode[qBound(0,modeIndex,3)];
    const double recordSeconds=m_time.recordSeconds();
    if (!std::isfinite(recordSeconds) || recordSeconds<=0 || recordSeconds>60 ||
        (mode==3 && (requestedHz*recordSeconds > 2'000'000.0 ||
                      requestedHz*recordSeconds < 16.0))) {
        m_error=QStringLiteral("Record length must produce 16..2,000,000 samples (maximum 60 s)"); return false;
    }
    if (m_ch1.filterIndex()<0 || m_ch1.filterIndex()>2 ||
        m_ch2.filterIndex()<0 || m_ch2.filterIndex()>2) {
        m_error=QStringLiteral("Unsupported Scope sampling filter"); return false;
    }
    if (!std::isfinite(requestedHz) || requestedHz < 1 ||
        !std::isfinite(static_cast<ValueSetting*>(m_time.position())->value()) ||
        static_cast<ValueSetting*>(m_time.position())->value() < 0 ||
        static_cast<ValueSetting*>(m_time.position())->value() > 100) {
        m_error=QStringLiteral("Invalid Scope sample rate or trigger position (0..100%)"); return false;
    }
    ScopeChannel* ch[] = {&m_ch1,&m_ch2};
    for (ScopeChannel* c : ch) {
        if (!std::isfinite(c->voltsRange()) || c->voltsRange() <= 0 ||
            !std::isfinite(c->voltsOffset()) || !std::isfinite(c->xAttenuation()) ||
            c->xAttenuation() < 1 || c->xAttenuation() > 1000) {
            m_error=QStringLiteral("Invalid Scope channel range, offset or attenuation"); return false;
        }
    }
    ScopeTriggerSettings trigger = m_trigger.settings();
    // Time.Position is the script-facing display position; Trigger.Position is
    // also accepted for convenience. Prefer Time.Position when both differ.
    trigger.positionPercent = static_cast<ValueSetting*>(m_time.position())->value();
    if (trigger.lengthCondition<0 || trigger.lengthCondition>2 ||
        !std::isfinite(trigger.lengthSeconds) || trigger.lengthSeconds<=0 ||
        !std::isfinite(trigger.holdoffSeconds) || trigger.holdoffSeconds<0) {
        m_error=QStringLiteral("Invalid trigger length, length condition or holdoff"); return false;
    }
    if (m_mock) {
        static_cast<ValueSetting*>(m_time.rate())->setReal(requestedHz);
        static_cast<ValueSetting*>(m_time.samples())->setReal(
            mode==3 ? int(std::round(requestedHz*recordSeconds)) : effectiveSamples);
        static_cast<ValueSetting*>(m_time.position())->setReal(trigger.positionPercent);
        for (ScopeChannel* c : ch) {
            static_cast<ValueSetting*>(c->range())->setReal(c->voltsRange());
            static_cast<ValueSetting*>(c->offset())->setReal(c->voltsOffset());
            static_cast<ValueSetting*>(c->attenuation())->setReal(c->xAttenuation());
        }
        return true;
    }
    if (!m_runtime) { m_error=QStringLiteral("No runtime"); return false; }
    if (!m_runtime->setupScope(requestedHz,effectiveSamples,
                               m_ch1.voltsRange(),m_ch2.voltsRange(),
                               m_ch1.voltsOffset(),m_ch2.voltsOffset(),
                               m_ch1.xAttenuation(),m_ch2.xAttenuation(),&m_error)) return false;
    if (!m_runtime->configureScopeAdvanced(mode,recordSeconds,
                m_ch1.filterIndex(),m_ch2.filterIndex(),&m_error)) return false;
    static_cast<ValueSetting*>(m_time.rate())->setReal(m_runtime->scopeRateHz());
    static_cast<ValueSetting*>(m_time.samples())->setReal(m_runtime->scopeSamples());
    if (!m_runtime->configureScopeTrigger(trigger,&m_error)) return false;
    static_cast<ValueSetting*>(m_time.position())->setReal(trigger.positionPercent);
    for (int i=0; i<2; ++i) {
        double r=0,o=0,a=0;
        if (!m_runtime->scopeChannelReadback(i,&r,&o,&a,&m_error)) return false;
        static_cast<ValueSetting*>(ch[i]->range())->setReal(r);
        static_cast<ValueSetting*>(ch[i]->offset())->setReal(o);
        static_cast<ValueSetting*>(ch[i]->attenuation())->setReal(a);
    }
    return true;
}
bool ScopeBridge::single() {
    m_repeat = false;
    m_scan = false;
    if (m_time.acquisitionMode()==1 || m_time.acquisitionMode()==2) {
        m_error=QStringLiteral("Screen and Shift are continuous modes: use Scope.run()");
        return false;
    }
    if (!config()) return false;
    m_started = true;
    if (m_mock) return true;
    if (!m_runtime->scopeStart(&m_error, m_stop)) { m_started = false; return false; }
    return true;
}
bool ScopeBridge::run() {
    if (m_time.acquisitionMode()==1 || m_time.acquisitionMode()==2) {
        if (!config()) return false;
        m_repeat=false; m_scan=true; m_started=true;
        if (!m_mock && !m_runtime->scopeStart(&m_error,m_stop)) {
            m_started=false; return false;
        }
        return true;
    }
    if (!single()) return false;
    m_repeat=true;
    return true;
}
bool ScopeBridge::wait() { return wait(5.0); }
bool ScopeBridge::wait(double seconds) {
    if (!m_started) { m_error = QStringLiteral("Call Scope1.single() first"); return false; }
    if (m_stop->load()) { m_error = QStringLiteral("Stopped"); return false; }
    const int count = m_time.acquisitionMode()==3
        ? int(std::round(m_time.sampleHz()*m_time.recordSeconds())) : m_time.sampleCount();
    if (m_mock) {
        QVector<double> a(count), b(count);
        const double sr = m_time.sampleHz();
        // Explicit independent W1 -> CH1 and W2 -> CH2 simulation routing.
        // Simulation does not model analog settling or hardware trigger jitter.
        auto valueAt = [this](int ch, double t) {
            if (!m_mock->waveRunning[ch]) return 0.0;
            const double hz=m_mock->frequency[ch];
            // Simulation approximates modulation mathematically. It does not
            // reproduce device-specific DAC quantization or trigger sequencing.
            auto modValue=[](int shape,double hzMod,double t) {
                const double p=std::fmod(hzMod*t,1.0);
                switch(shape) {
                case 2:return p<0.5?1.0:-1.0;
                case 3:return 1.0-4.0*std::abs(p-0.5);
                case 4:return 2*p-1;
                case 5:return 1-2*p;
                default:return std::sin(2*kPi*p);
                }
            };
            double cycles=hz*t+m_mock->phase[ch]/360.0;
            if(m_mock->fm[ch] && m_mock->fmHz[ch]>0) {
                const double rate=m_mock->fmHz[ch];
                const double depth=m_mock->fmDepth[ch]/100.0;
                if(m_mock->fmShape[ch]==1) // integrated sine modulation
                    cycles+=hz*depth*(1-std::cos(2*kPi*rate*t))/(2*kPi*rate);
                else if(m_mock->fmShape[ch]==4) { // repeated ramp sweep
                    const double fraction=std::fmod(rate*t,1.0);
                    cycles+=hz*depth*(fraction*fraction-fraction)/rate;
                } else cycles+=hz*depth*modValue(m_mock->fmShape[ch],rate,t)*t;
            }
            const double phase=std::fmod(cycles,1.0);
            const double duty=m_mock->symmetry[ch]/100.0;
            double carrier=0;
            switch(m_mock->shape[ch]) {
            case 0: carrier=0; break;
            case 1: carrier=std::sin(2*kPi*phase); break;
            case 2: carrier=(phase<duty?1.0:-1.0); break;
            case 3: carrier= phase<duty ? (-1.0+2.0*phase/duty) : (1.0-2.0*(phase-duty)/(1.0-duty)); break;
            case 4: carrier=2.0*phase-1.0; break;
            case 5: carrier=1.0-2.0*phase; break;
            case 6: carrier=std::sin(2*kPi*47*phase)*std::sin(2*kPi*113*phase); break;
            case 7: {
                const auto& custom=m_mock->custom[ch];
                if (!custom.isEmpty()) {
                    const double index=phase*custom.size();
                    const int n=int(index)%custom.size();
                    const int next=(n+1)%custom.size();
                    const double fraction=index-std::floor(index);
                    carrier=custom[n]*(1-fraction)+custom[next]*fraction;
                }
                break;
            }
            }
            double amp=m_mock->amplitude[ch];
            if(m_mock->am[ch] && m_mock->amHz[ch]>0)
                amp*=1.0+(m_mock->amDepth[ch]/100.0)*
                    modValue(m_mock->amShape[ch],m_mock->amHz[ch],t);
            return m_mock->offset[ch]+amp*carrier;
        };
        for (int i=0; i<count; ++i) {
            const double t=double(i)/sr;
            a[i]=valueAt(0,t)*m_ch1.xAttenuation();
            b[i]=valueAt(1,t)*m_ch2.xAttenuation();
        }
        m_lastTriggerTime={};
        m_ch1.setSamples(a, sr); m_ch2.setSamples(b, sr);
        ScopeFrame frame; frame.ch1=a;frame.ch2=b;frame.sampleRateHz=m_time.sampleHz();frame.simulated=true;
        m_time.setTaken(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        emit captureReady(frame);
        if (!m_repeat && !m_scan) m_started = false;
        return true;
    }
    QVector<double> a,b;
    if (m_time.acquisitionMode()==3) {
        const int target=int(std::round(m_runtime->scopeRateHz()*m_time.recordSeconds()));
        if (!m_runtime->scopeReadRecord(target,a,b,seconds,*m_stop,&m_error)) return false;
        static_cast<ValueSetting*>(m_time.samples())->setReal(a.size());
    } else if (m_scan) {
        if (!m_runtime->scopeReadScan(a,b,seconds,*m_stop,&m_error)) return false;
    } else {
        if (!m_runtime->scopeWait(seconds,*m_stop,&m_error)) return false;
        const int actualCount = m_runtime->scopeSamples();
        if (!m_runtime->readScope(0,actualCount,a,&m_error) ||
            !m_runtime->readScope(1,actualCount,b,&m_error)) return false;
    }
    m_lastTriggerTime=m_runtime->scopeTriggerTime();
    m_ch1.setSamples(a, m_runtime->scopeRateHz()); m_ch2.setSamples(b, m_runtime->scopeRateHz());
    ScopeFrame frame; frame.ch1=a;frame.ch2=b;frame.sampleRateHz=m_runtime->scopeRateHz();frame.simulated=false;
    m_time.setTaken(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    emit captureReady(frame);
    if (m_repeat && !m_scan && !m_stop->load()) {
        if (!m_runtime->scopeStart(&m_error, m_stop)) { m_started=false; return false; }
    } else if (!m_scan) m_started = false;
    return true;
}
void ScopeBridge::stop() {
    m_repeat=false;
    m_scan=false;
    if (m_started && !m_mock && m_runtime && m_runtime->connected())
        m_runtime->scopeStop(&m_error);
    m_started=false;
}

WavegenBridge::WavegenBridge(DwfRuntime* runtime,MockInstrumentState* mock,const std::atomic_bool* stop)
    : m_runtime(runtime), m_mock(mock),m_stop(stop) { m_ch1.setEnabled(true); }
bool WavegenBridge::config() {
    m_error.clear();
    WavegenChannel* channels[] = {&m_ch1,&m_ch2};
    for (int i=0;i<2;++i) {
        WavegenChannel* channel=channels[i];
        if (!channel->enabled()) {
            if (m_running[i]) {
                if (m_mock) m_mock->waveRunning[i]=false;
                else if (!m_runtime || !m_runtime->stopWavegen(i,&m_error)) return false;
                m_running[i]=false;
            }
            continue;
        }
        auto& settings=channel->settings();
        const auto& burst=channel->burstSettings();
        const QString mode=channel->mode()->property("text").toString();
        if (mode==QStringLiteral("Play")) {
            m_error=QStringLiteral("Use Wavegen.play(channel,timeout) for bounded Play streaming"); return false;
        }
        double hz=settings.hz(), amplitude=settings.amplitudeVolts();
        const double offset=settings.offsetVolts();
        DwfRuntime::WaveAdvanced advanced;
        auto& modulation=channel->modulationSettings();
        auto& fm=modulation.fmNode(); auto& am=modulation.amNode();
        advanced.fm=fm.enabled(); advanced.am=am.enabled();
        advanced.fmType=fm.waveform(); advanced.amType=am.waveform();
        advanced.fmHz=fm.hz(); advanced.amHz=am.hz();
        advanced.fmDepth=fm.depthPercent(); advanced.amDepth=am.depthPercent();
        const QStringList sources={"None","PC","Detector 1","Detector 2","Wavegen 1","Wavegen 2","External 1","External 2"};
        const int sourceIds[]={0,1,2,3,7,8,11,12};
        const int src=sources.indexOf(channel->triggerSettings().sourceText());
        const QStringList idleNames={"Disable","Offset","Initial"};
        advanced.triggerSource=src>=0?sourceIds[src]:-1;
        advanced.idle=idleNames.indexOf(channel->triggerSettings().idleText());
        advanced.repeatTrigger=channel->triggerSettings().repeatTrigger();
        if(mode==QStringLiteral("Sweep")) {
            auto& sweep=channel->sweepSettings();
            auto& sf=sweep.frequencyAxis(); auto& sa=sweep.amplitudeAxis();
            if ((!sf.enabled()&&!sa.enabled()) || advanced.fm || advanced.am) {
                m_error=QStringLiteral("Sweep needs an enabled axis; disable separate AM/FM modulation"); return false;
            }
            if(sf.enabled()) {
                const double lo=sf.from(),hi=sf.to();
                if(!std::isfinite(lo)||!std::isfinite(hi)||lo<=0||hi<=0||
                   lo>25e6||hi>25e6||sf.seconds()<0.001||sf.seconds()>3600) {
                    m_error=QStringLiteral("Invalid frequency sweep endpoints/time"); return false;
                }
                hz=(lo+hi)/2.0; advanced.fm=true; advanced.fmType="Ramp Up";
                advanced.fmHz=1.0/sf.seconds();
                advanced.fmType=hi>=lo?"Ramp Up":"Ramp Down";
                advanced.fmDepth=100*std::abs(hi-lo)/(hi+lo);
            }
            if(sa.enabled()) {
                const double lo=sa.from(),hi=sa.to();
                if(!std::isfinite(lo)||!std::isfinite(hi)||lo<0||hi<0||
                   lo>5||hi>5||lo+hi<=0||sa.seconds()<0.001||sa.seconds()>3600) {
                    m_error=QStringLiteral("Invalid amplitude sweep endpoints/time"); return false;
                }
                amplitude=(lo+hi)/2.0; advanced.am=true;advanced.amType="Ramp Up";
                advanced.amHz=1.0/sa.seconds();
                advanced.amType=hi>=lo?"Ramp Up":"Ramp Down";
                advanced.amDepth=100*std::abs(hi-lo)/(hi+lo);
            }
        }
        auto validNode=[](bool enabled,double rate,double depth) {
            return !enabled || (std::isfinite(rate)&&rate>0&&rate<=1e6&&
                std::isfinite(depth)&&std::abs(depth)<=100.0);
        };
        if(!validNode(advanced.fm,advanced.fmHz,advanced.fmDepth)||
           !validNode(advanced.am,advanced.amHz,advanced.amDepth)||
           advanced.triggerSource<0||advanced.idle<0) {
            m_error=QStringLiteral("Invalid modulation, trigger source or idle setting");return false;
        }
        if(std::abs(offset)+amplitude*(1+(advanced.am?std::abs(advanced.amDepth)/100.0:0))>5.0) {
            m_error="AM peak voltage exceeds validated ±5 V output envelope";return false;
        }
        const double symmetry=settings.symmetryPct(), phase=settings.phaseDegrees();
        const double runSeconds=burst.runSeconds(), waitSeconds=burst.waitSeconds();
        const double repeats=burst.repetitionValue();
        const double master=burst.masterValue();
        if (!std::isfinite(hz) || !std::isfinite(amplitude) || !std::isfinite(offset) ||
            !std::isfinite(symmetry) || !std::isfinite(phase) ||
            !std::isfinite(runSeconds) || !std::isfinite(waitSeconds) ||
            !std::isfinite(repeats) || std::floor(repeats)!=repeats ||
            !std::isfinite(master) || std::floor(master)!=master ||
            hz <= 0 || hz > 25e6 || amplitude < 0 || amplitude > 5.0 ||
            std::abs(offset)+amplitude > 5.0 || symmetry <= 0 || symmetry >= 100 ||
            phase < 0 || phase > 360 || runSeconds < 0 || runSeconds > 3600 ||
            waitSeconds < 0 || waitSeconds > 3600 || repeats < 0 || repeats > 100000 ||
            (master != -1 && !(i == 1 && master == 0))) {
            m_error=QStringLiteral("Wavegen setting outside validated limits (W2 Master: -1 or 0)");
            return false;
        }
        const QString type=settings.typeText();
        const QVector<double>& custom=channel->customSettings().data();
        if (type==QStringLiteral("Custom") && custom.size()<2) {
            m_error=QStringLiteral("Call Wavegen.ChannelN.Custom.set([...]) before using Type Custom"); return false;
        }
        if (m_mock) {
            m_mock->frequency[i]=hz; m_mock->amplitude[i]=amplitude;
            m_mock->offset[i]=offset; m_mock->symmetry[i]=symmetry;
            m_mock->phase[i]=phase; m_mock->custom[i]=custom;
            m_mock->fm[i]=advanced.fm; m_mock->am[i]=advanced.am;
            m_mock->fmHz[i]=advanced.fmHz;m_mock->amHz[i]=advanced.amHz;
            m_mock->fmDepth[i]=advanced.fmDepth;m_mock->amDepth[i]=advanced.amDepth;
            const QStringList modShapes={"DC","Sine","Square","Triangle","Ramp Up","Ramp Down","Noise"};
            m_mock->fmShape[i]=modShapes.indexOf(advanced.fmType);
            m_mock->amShape[i]=modShapes.indexOf(advanced.amType);
            m_mock->shape[i]=QStringList({"DC","Sine","Square","Triangle","Ramp Up","Ramp Down","Noise","Custom"}).indexOf(type);
            if (m_mock->shape[i]<0) {m_error=QStringLiteral("Unsupported waveform type"); return false;}
        } else {
            if (!m_runtime || !m_runtime->setupWavegen(i,type,hz,amplitude,offset,
                      symmetry,phase,custom,runSeconds,waitSeconds,int(repeats),int(master),&m_error)) return false;
            if(!m_runtime->setupWavegenAdvanced(i,advanced,&m_error)) return false;
            // For independent generators a config update while running is committed
            // by re-starting that output. A slave is started by its master.
            if (m_running[i] && master == -1 && !m_runtime->startWavegen(i,&m_error)) return false;
        }
        // .real reflects accepted settings (not calibrated measured analog voltage).
        // For sweep mode carrier settings reflect the effective midpoint.
        static_cast<ValueSetting*>(settings.frequency())->setReal(hz);
        static_cast<ValueSetting*>(settings.period())->setReal(1.0/hz);
        static_cast<ValueSetting*>(settings.amplitude())->setReal(amplitude);
        static_cast<ValueSetting*>(settings.offset())->setReal(offset);
        static_cast<ValueSetting*>(settings.symmetry())->setReal(symmetry);
        static_cast<ValueSetting*>(settings.phase())->setReal(phase);
    }
    return true;
}
bool WavegenBridge::run() {
    if (!config()) return false;
    WavegenChannel* channels[] = {&m_ch1,&m_ch2};
    // For W2 slaved to W1, commit the stopped slave's settings BEFORE the
    // master is started. The SDK otherwise may start W2 from an older
    // configuration, giving an arbitrary apparent relative phase.
    // Leave independent-channel startup behavior unchanged.
    if (!m_mock && channels[1]->enabled() &&
        channels[1]->burstSettings().masterIndex() == 0) {
        if (!m_runtime || !m_runtime->prepareWavegenSlave(1, &m_error)) return false;
    }
    // Start the master first; the DWF hardware starts its prepared slave.
    for (int i=0;i<2;++i) {
        if (!channels[i]->enabled()) continue;
        if (m_mock) m_mock->waveRunning[i]=true;
        else if (!(i==1 && channels[i]->burstSettings().masterIndex()==0) &&
                 !m_runtime->startWavegen(i,&m_error)) {
            stop(); // Don't leave partially configured independent channels running.
            return false;
        }
        m_running[i]=true;
    }
    return true;
}
void WavegenBridge::stop() {
    for (int i=0;i<2;++i) {
        if (m_mock) m_mock->waveRunning[i]=false;
        else if (m_runtime && m_runtime->connected()) m_runtime->stopWavegen(i,&m_error);
        m_running[i]=false;
    }
}
QVariantMap WavegenBridge::capabilities(int channelNumber) {
    m_error.clear();
    if(channelNumber<1||channelNumber>2) {m_error="Expected channel 1 or 2";return {};}
    if(m_mock) return {{"channel",channelNumber},{"nodeMask",7},{"simulation",true},
                       {"playSupported",true}};
    return m_runtime?m_runtime->wavegenCapabilities(channelNumber-1,&m_error):QVariantMap{};
}
QVariantMap WavegenBridge::status(int channelNumber) {
    m_error.clear();
    if(channelNumber<1||channelNumber>2) {m_error="Expected channel 1 or 2";return {};}
    if(m_mock) return {{"channel",channelNumber},{"simulation",true},
                       {"running",m_mock->waveRunning[channelNumber-1]}};
    return m_runtime?m_runtime->wavegenStatus(channelNumber-1,&m_error):QVariantMap{};
}
bool WavegenBridge::play(int channelNumber,double seconds) {
    m_error.clear();
    if(channelNumber<1||channelNumber>2||!std::isfinite(seconds)||seconds<=0||seconds>120) {
        m_error="Invalid Play channel/timeout";return false;
    }
    WavegenChannel& ch=channelNumber==1?m_ch1:m_ch2;
    if(ch.mode()->property("text").toString()!="Play" || !ch.enabled() ||
       !std::isfinite(ch.playData().hz()) || ch.playData().hz()<1 ||ch.playData().hz()>1000000 ||
       ch.playData().data().size()<16) {
        m_error="Use enabled channel Mode=Play, Play.Rate and Play.set([...])"; return false;
    }
    // Play has a dedicated finite streaming API; never leave a replenishment
    // stream running on a waveform-generator .run() command.
    if(m_mock) return true; // Validated dry-run; physical scheduling requires the SDK.
    if(!m_runtime) {m_error="Device disconnected";return false;}
    return m_runtime->playWavegen(channelNumber-1,ch.playData().hz(),
                                ch.playData().data(),seconds,m_stop,&m_error);
}
LogicBridge::LogicBridge(DwfRuntime* runtime, MockInstrumentState* mock, const std::atomic_bool* stop)
    : m_runtime(runtime),m_mock(mock),m_stop(stop) {}
bool LogicBridge::single() {
    // Explicitly stop an earlier arm rather than discarding its state.
    if(m_started)stop();
    m_error.clear(); m_data.clear(); m_started=false; m_effectiveRateHz=0;
    const double count=m_time.sampleCount();
    const double rate=m_time.sampleRate();
    if (!std::isfinite(rate) || rate<1 || rate>125e6 ||
        count<16 || count>32768 || count!=std::floor(count)) {
        m_error=QStringLiteral("Logic1.Time: Samples must be 16..32768 and Rate 1..125e6 Hz");
        return false;
    }
    m_count=int(count);
    if (m_stop && m_stop->load()) { m_error=QStringLiteral("Script stopped"); return false; }
    if (m_mock) {
        m_effectiveRateHz=rate;
    } else if (!m_runtime || !m_runtime->setupLogic(m_count,rate,&m_effectiveRateHz,&m_error) ||
               !m_runtime->logicStart(&m_error)) return false;
    m_time.effectiveRate(m_effectiveRateHz);
    m_time.effectiveSamples(m_count);
    m_started=true;
    return true;
}
bool LogicBridge::wait() { return wait(5.0); }
bool LogicBridge::wait(double seconds) {
    if (!m_started) { m_error=QStringLiteral("Call Logic1.single() first"); return false; }
    if (!std::isfinite(seconds) || seconds<=0) { m_error=QStringLiteral("Invalid Logic1.wait timeout"); return false; }
    if (m_stop && m_stop->load()) {stop();m_error=QStringLiteral("Stopped");return false;}
    if (m_mock) {
        m_data.resize(m_count);
        // Distinct, unmistakably simulated digital toggles on DIO0 and DIO1.
        for (int i=0;i<m_count;++i) {
            const int quarter=(4*i)/m_count;
            const quint16 state=(quarter==1 || quarter==3)?quint16(3):quint16(0);
            m_data[i]=state;
        }
    } else {
        if (!m_runtime->logicWait(seconds,*m_stop,&m_error) ||
            !m_runtime->readLogic(m_count,m_data,&m_error)) {
            QString stopError;
            m_runtime->logicStop(&stopError);
            m_started=false;
            return false;
        }
    }
    m_started=false;
    return true;
}
QVariantList LogicBridge::samples() const {
    QVariantList result;result.reserve(m_data.size());
    for (quint16 value:m_data) result.append(int(value));
    return result;
}
QVariantMap LogicBridge::captureDiagnostics() const {
    int firstChanges=0,secondChanges=0,firstHighWords=0,secondHighWords=0;
    // Track DIO0..DIO5 relevant to the encoder; all-low is a valid static
    // input, so zero transitions by itself is NOT an incomplete-data error.
    const int mid=m_data.size()/2;
    for(int i=0;i<m_data.size();++i) {
        const int word=int(m_data[i])&0x3f;
        if(word) {if(i<mid)++firstHighWords;else ++secondHighWords;}
        if(i>0 && ((word^int(m_data[i-1]))&0x3f)) {
            if(i<mid)++firstChanges;else ++secondChanges;
        }
    }
    return {{QStringLiteral("requestedSamples"),m_count},
            {QStringLiteral("samplesReturned"),m_data.size()},
            {QStringLiteral("bytesRequested"),m_runtime?m_runtime->logicLastBytesRequested():int(m_data.size()*sizeof(quint16))},
            {QStringLiteral("sampleFormatBits"),16},
            {QStringLiteral("sdkValidSamples"),m_runtime?m_runtime->logicLastValidSamples():-1},
            {QStringLiteral("sdkStateCode"),m_runtime?m_runtime->logicLastStateCode():-1},
            {QStringLiteral("firstHalfChanges"),firstChanges},
            {QStringLiteral("secondHalfChanges"),secondChanges},
            {QStringLiteral("firstHalfHighWords"),firstHighWords},
            {QStringLiteral("secondHalfHighWords"),secondHighWords},
            {QStringLiteral("simulation"),bool(m_mock)}};
}
void LogicBridge::stop() {
    if (m_started && m_runtime && !m_mock) {
        QString stopError;
        if (!m_runtime->logicStop(&stopError)) m_error=stopError;
    }
    m_started=false;
}
bool ScriptUtilities::wait(double seconds) const {
    if (!std::isfinite(seconds)) return false;
    // WaveForms semantics: wait(negative) remains blocked until the script is
    // cancelled and then returns false. v5.1 incorrectly ended after one hour.
    if (seconds < 0) {
        while (!m_stop->load()) QThread::msleep(10);
        return false;
    }
    QElapsedTimer timer; timer.start();
    const qint64 totalMs = qint64(qBound(0.0, seconds, 3600.0)*1000);
    while (timer.elapsed() < totalMs) {
        if (m_stop->load()) return false;
        QThread::msleep(10);
    }
    return !m_stop->load();
}
