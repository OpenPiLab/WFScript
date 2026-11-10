#pragma once

#include "DwfRuntime.h"
#include "ScopeFrame.h"
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <utility>
#include <QVariantMap>
#include <QDateTime>
#include <QVector>
#include <atomic>

// Independent subset of the documented WaveForms JavaScript object model.
// Deliberately not a representation of Digilent's proprietary GUI classes.
struct MockInstrumentState { bool waveRunning[2] = {false,false}; double frequency[2] = {1000,1000}; double amplitude[2] = {1,1}; double offset[2] = {0,0}; int shape[2]={1,1};
    double symmetry[2]={50,50}, phase[2]={0,0}; QVector<double> custom[2];
    bool fm[2]={false,false}, am[2]={false,false};
    double fmHz[2]={0,0},fmDepth[2]={0,0},amHz[2]={0,0},amDepth[2]={0,0};
    int fmShape[2]={1,1},amShape[2]={1,1}; };

// Deliberately small, UI-independent Qt meta-objects. The documented
// WaveForms parameter surface is value / real / text / preset, not the
// underlying WaveForms QWidget or internal Qt signal/slot tree.
class ValueSetting : public QObject {
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double real READ real NOTIFY realChanged)
    Q_PROPERTY(QString text READ text NOTIFY valueChanged)
    Q_PROPERTY(QVariantList preset READ preset CONSTANT)
public:
    explicit ValueSetting(double initial=0, QObject* parent=nullptr, QString unit={})
        : QObject(parent), m_value(initial), m_real(initial), m_unit(std::move(unit)) {}
    double value() const { return m_value; }
    double real() const { return m_real; }
    QString text() const;
    QVariantList preset() const { return {}; } // Numeric presets not yet modeled.
    void setValue(double v);
    void setReal(double v);
signals:
    void valueChanged();
    void realChanged();
private:
    double m_value=0, m_real=0;
    QString m_unit;
};
// A read-only SDK enumerated range list; values are volts peak-to-peak.
// In simulation or if the optional SDK query is unavailable, steps is [].
// This is not a list of distinct physical amplifier gain stages.
class ScopeRangeSetting final : public ValueSetting {
    Q_OBJECT
    Q_PROPERTY(QVariantList steps READ steps NOTIFY stepsChanged)
public:
    explicit ScopeRangeSetting(double initial=5.0)
        : ValueSetting(initial,nullptr,QStringLiteral("V")) {}
    QVariantList steps() const { return m_steps; }
    void setSteps(const QVariantList& steps) {
        if (m_steps == steps) return;
        m_steps = steps;
        emit stepsChanged();
    }
signals:
    void stepsChanged();
private:
    QVariantList m_steps;
};
class TextSetting final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY textChanged)
    Q_PROPERTY(int real READ value NOTIFY textChanged)
    Q_PROPERTY(QStringList preset READ preset CONSTANT)
public:
    explicit TextSetting(const QString& initial=QString(), QObject* parent=nullptr,
                         QStringList choices={})
        : QObject(parent), m_text(initial), m_choices(std::move(choices)) {}
    QString text() const { return m_text; }
    int value() const { return m_choices.indexOf(m_text); }
    QStringList preset() const { return m_choices; }
    void setText(const QString& v);
    void setValue(int index);
signals:
    void textChanged();
private:
    QString m_text;
    QStringList m_choices;
};
class ScopeTrigger final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Trigger READ trigger CONSTANT)
    Q_PROPERTY(QObject* Type READ type CONSTANT)
    Q_PROPERTY(QObject* Source READ source CONSTANT)
    Q_PROPERTY(QObject* Condition READ condition CONSTANT)
    Q_PROPERTY(QObject* Level READ level CONSTANT)
    Q_PROPERTY(QObject* Hysteresis READ hysteresis CONSTANT)
    Q_PROPERTY(QObject* LCondition READ lengthCondition CONSTANT)
    Q_PROPERTY(QObject* Length READ length CONSTANT)
    Q_PROPERTY(QObject* HoldOff READ holdOff CONSTANT)
    Q_PROPERTY(QObject* Filter READ filter CONSTANT)
public:
    using QObject::QObject;
    QObject* trigger() { return &m_trigger; }
    QObject* type() { return &m_type; }
    QObject* source() { return &m_source; }
    QObject* condition() { return &m_condition; }
    QObject* level() { return &m_level; }
    QObject* hysteresis() { return &m_hysteresis; }
    QObject* lengthCondition() { return &m_lcondition; }
    QObject* length() { return &m_length; }
    QObject* holdOff() { return &m_holdoff; }
    QObject* filter() { return &m_filter; }
    ScopeTriggerSettings settings() const;
private:
    // Disabled by default to preserve v5.0 script behavior.
    TextSetting m_trigger{QStringLiteral("None"),nullptr,{"None","Auto","Normal","Repeated"}};
    TextSetting m_type{QStringLiteral("Edge"),nullptr,{"Edge","Pulse","Transition"}};
    TextSetting m_source{QStringLiteral("Channel 1"),nullptr,{"Channel 1","Channel 2","Wavegen 1","Wavegen 2","External 1","External 2","PC"}};
    TextSetting m_condition{QStringLiteral("Rising"),nullptr,{"Rising","Falling"}};
    ValueSetting m_level{0.0,nullptr,QStringLiteral("V")};
    ValueSetting m_hysteresis{0.05,nullptr,QStringLiteral("V")};
    ValueSetting m_lcondition{1};
    ValueSetting m_length{0.0005,nullptr,QStringLiteral("s")};
    ValueSetting m_holdoff{0,nullptr,QStringLiteral("s")};
    TextSetting m_filter{QStringLiteral("Decimate"),nullptr,{"Decimate","Average"}};
};
class ScopeTime final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Samples READ samples CONSTANT)
    Q_PROPERTY(QObject* Rate READ rate CONSTANT)
    Q_PROPERTY(QObject* TriggerPositionPercent READ position CONSTANT)
    Q_PROPERTY(QString taken READ taken)
    Q_PROPERTY(qint64 takenEpochMs READ takenEpochMs)
    Q_PROPERTY(QString timestampSource READ timestampSource)
    Q_PROPERTY(QObject* Mode READ mode CONSTANT)
    Q_PROPERTY(QObject* RecordLength READ recordLength CONSTANT)
public:
    using QObject::QObject;
    QObject* samples() { return &m_samples; }
    QObject* rate() { return &m_rate; }
    QObject* position() { return &m_position; }
    QObject* mode() { return &m_mode; }
    QObject* recordLength() { return &m_recordLength; }
    int acquisitionMode() const { return m_mode.value(); }
    double recordSeconds() const { return m_recordLength.value(); }
    QString taken() const { return m_taken; }
    void setTaken(const QString& value) { m_taken=value; m_takenEpochMs=QDateTime::currentMSecsSinceEpoch(); }
    qint64 takenEpochMs() const { return m_takenEpochMs; }
    QString timestampSource() const { return QStringLiteral("HostAfterTransfer"); }
    int sampleCount() const { return int(qBound(16.0, m_samples.value(), 65536.0)); }
    double sampleHz() const { return m_rate.value(); }
private:
    ValueSetting m_samples{4096};
    ValueSetting m_rate{1e6,nullptr,QStringLiteral("Hz")};
    ValueSetting m_position{50.0,nullptr,QStringLiteral("%")};
    TextSetting m_mode{QStringLiteral("Repeated"),nullptr,{"Repeated","Screen","Shift","Record"}};
    ValueSetting m_recordLength{0.2,nullptr,QStringLiteral("s")};
    QString m_taken;
    qint64 m_takenEpochMs=0;
};
class ScopeChannel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Range READ range CONSTANT)
    Q_PROPERTY(QObject* Offset READ offset CONSTANT)
    Q_PROPERTY(QObject* Attenuation READ attenuation CONSTANT)
    Q_PROPERTY(QObject* SampleMode READ sampleMode CONSTANT)
    // A Qt 6 native numeric sequence avoids one QVariant allocation per sample.
    Q_PROPERTY(QVector<double> data READ getData)
public:
    explicit ScopeChannel(int index, QObject* parent=nullptr) : QObject(parent), m_index(index) {}
    QObject* range() { return &m_range; }
    QObject* offset() { return &m_offset; }
    QObject* attenuation() { return &m_attenuation; }
    QObject* sampleMode() { return &m_sampleMode; }
    int filterIndex() const { return m_sampleMode.value(); }
    Q_INVOKABLE QVector<double> getData() const;
    Q_INVOKABLE double sample(int index) const;
    Q_INVOKABLE double timeOfIndex(int index) const;
    Q_INVOKABLE QVariantMap statistics() const;
    Q_INVOKABLE QVariantMap statistics(double startSeconds,double endSeconds) const;
    Q_INVOKABLE double measure(const QString& type) const;
    Q_INVOKABLE double measure(const QString& type,double startSeconds,double endSeconds) const;
    Q_INVOKABLE double measureWindow(const QString& type,double startSeconds,double endSeconds) const;
    Q_INVOKABLE QVariantMap spectrum(int fftSize,const QString& window=QStringLiteral("Hann")) const;
    Q_INVOKABLE double measureCapture(const QString& type) const { return measure(type); }
    Q_INVOKABLE double measureCapture(const QString& type,double start,double end) const { return measureWindow(type,start,end); }
    Q_INVOKABLE QVector<double> getVisibleData() const { return getData(); }
    int index() const { return m_index; }
    double sampleRateHz() const { return m_sampleRateHz; }
    double voltsRange() const { return m_range.value(); }
    double voltsOffset() const { return m_offset.value(); }
    double xAttenuation() const { return m_attenuation.value(); }
    void setSamples(const QVector<double>& data, double sampleRateHz) {
        m_data = data; m_sampleRateHz = sampleRateHz;
    }
private:
    int m_index=0;
    ScopeRangeSetting m_range{5.0};
    ValueSetting m_offset{0.0,nullptr,QStringLiteral("V")};
    ValueSetting m_attenuation{1.0};
    TextSetting m_sampleMode{QStringLiteral("Decimate"),nullptr,{"Decimate","Average","Min/Max"}};
    QVector<double> m_data;
    double m_sampleRateHz = 1e6;
};
class ScopeBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Time READ time CONSTANT)
    Q_PROPERTY(QObject* Trigger READ trigger CONSTANT)
    Q_PROPERTY(QObject* Channel1 READ channel1 CONSTANT)
    Q_PROPERTY(QObject* Channel2 READ channel2 CONSTANT)
    Q_PROPERTY(QString lastError READ lastError)
    // WFScript diagnostics; counts reflect committed DWF operations, not merely JS calls.
    Q_PROPERTY(int configurationCount READ configurationCount)
    Q_PROPERTY(int triggerConfigurationCount READ triggerConfigurationCount)
    Q_PROPERTY(int acquisitionCount READ acquisitionCount)
    Q_PROPERTY(int lastSettleMs READ lastSettleMs)
    Q_PROPERTY(int recordLostSamples READ recordLostSamples)
    Q_PROPERTY(int recordCorruptSamples READ recordCorruptSamples)
public:
    ScopeBridge(DwfRuntime* runtime, MockInstrumentState* mock, const std::atomic_bool* stop);
    QObject* time() { return &m_time; }
    QObject* trigger() { return &m_trigger; }
    QObject* channel1() { return &m_ch1; }
    QObject* channel2() { return &m_ch2; }
    QString lastError() const { return m_error; }
    int configurationCount() const { return m_runtime ? m_runtime->scopeSetupCount() : 0; }
    int triggerConfigurationCount() const { return m_runtime ? m_runtime->scopeTriggerSetupCount() : 0; }
    int acquisitionCount() const { return m_runtime ? m_runtime->scopeArmCount() : 0; }
    int lastSettleMs() const { return m_runtime ? m_runtime->scopeLastSettleMs() : 0; }
    int recordLostSamples() const { return m_runtime ? m_runtime->recordLostSamples() : 0; }
    int recordCorruptSamples() const { return m_runtime ? m_runtime->recordCorruptSamples() : 0; }
    Q_INVOKABLE QVariantMap capabilities();
    Q_INVOKABLE QVariantMap hardwareStatus();
    Q_INVOKABLE QVariantMap captureInfo() const;
    Q_INVOKABLE bool saveReference(int channelNumber);
    Q_INVOKABLE QVector<double> referenceData(int channelNumber) const;
    Q_INVOKABLE QVector<double> mathData(const QString& operation) const;
    Q_INVOKABLE bool config();
    Q_INVOKABLE bool single();
    Q_INVOKABLE bool run(); // rearm on every completed wait() until stop()
    Q_INVOKABLE bool wait();
    Q_INVOKABLE bool wait(double timeoutSeconds);
    Q_INVOKABLE void stop();
signals:
    void captureReady(const ScopeFrame& frame);
private:
    DwfRuntime* m_runtime;
    MockInstrumentState* m_mock;
    const std::atomic_bool* m_stop;
    ScopeTime m_time;
    ScopeTrigger m_trigger;
    ScopeChannel m_ch1{0}, m_ch2{1};
    QVariantMap m_rangeCapabilities; // cached once per device/engine, no DWF calls on property reads
    QString m_error;
    bool m_started=false;
    bool m_repeat=false;
    bool m_scan=false;
    QVector<double> m_reference[2];
    double m_referenceRateHz[2] = {0,0};
    QVariantMap m_lastTriggerTime;
};

class WavegenSimple final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Type READ type CONSTANT)
    Q_PROPERTY(QObject* Frequency READ frequency CONSTANT)
    Q_PROPERTY(QObject* Amplitude READ amplitude CONSTANT)
    Q_PROPERTY(QObject* Offset READ offset CONSTANT)
    Q_PROPERTY(QObject* Period READ period CONSTANT)
    Q_PROPERTY(QObject* Symmetry READ symmetry CONSTANT)
    Q_PROPERTY(QObject* Phase READ phase CONSTANT)
public:
    using QObject::QObject;
    QObject* type() { return &m_type; }
    QObject* frequency() { return &m_frequency; }
    QObject* amplitude() { return &m_amplitude; }
    QObject* offset() { return &m_offset; }
    QObject* period() { return &m_period; }
    QObject* symmetry() { return &m_symmetry; }
    QObject* phase() { return &m_phase; }
    WavegenSimple();
    QString typeText() const { return m_type.text(); }
    double hz() const { return m_frequency.value(); }
    double amplitudeVolts() const { return m_amplitude.value(); }
    double offsetVolts() const { return m_offset.value(); }
    double symmetryPct() const { return m_symmetry.value(); }
    double phaseDegrees() const { return m_phase.value(); }
private:
    TextSetting m_type{QStringLiteral("Sine"),nullptr,{"DC","Sine","Square","Triangle","Ramp Up","Ramp Down","Noise","Custom"}};
    ValueSetting m_frequency{1000,nullptr,QStringLiteral("Hz")};
    ValueSetting m_amplitude{1.0,nullptr,QStringLiteral("V")};
    ValueSetting m_offset{0.0,nullptr,QStringLiteral("V")};
    ValueSetting m_period{0.001,nullptr,QStringLiteral("s")};
    ValueSetting m_symmetry{50.0,nullptr,QStringLiteral("%")};
    ValueSetting m_phase{0.0,nullptr,QStringLiteral("deg")};
    bool m_syncPeriod=false;
};
// Custom samples are normalized [-1, 1], with amplitude/offset applied by the DWF generator.
class WavegenCustom final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int length READ length)
public:
    using QObject::QObject;
    Q_INVOKABLE bool set(const QVariantList& samples);
    int length() const { return m_samples.size(); }
    const QVector<double>& data() const { return m_samples; }
private:
    QVector<double> m_samples;
};
class WavegenBurst final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Run READ run CONSTANT)
    Q_PROPERTY(QObject* Wait READ waitTime CONSTANT)
    Q_PROPERTY(QObject* Repeat READ repeat CONSTANT)
    Q_PROPERTY(QObject* Master READ master CONSTANT)
public:
    using QObject::QObject;
    QObject* run() { return &m_run; }
    QObject* waitTime() { return &m_wait; }
    QObject* repeat() { return &m_repeat; }
    QObject* master() { return &m_master; }
    double runSeconds() const { return m_run.value(); }
    double waitSeconds() const { return m_wait.value(); }
    int repetitions() const { return int(m_repeat.value()); }
    double repetitionValue() const { return m_repeat.value(); }
    double masterValue() const { return m_master.value(); }
    int masterIndex() const { return int(m_master.value()); }
private:
    ValueSetting m_run{0,nullptr,QStringLiteral("s")}; // 0: continuous
    ValueSetting m_wait{0,nullptr,QStringLiteral("s")};
    ValueSetting m_repeat{0}; // 0: infinite (when run is finite)
    ValueSetting m_master{-1}; // -1 independent, 0 W1 master for W2
};
// Hardware modulation nodes are controlled independently of the carrier.
// Depth is an SDK modulation index in percent; capabilities determine support.
class WavegenModulationNode final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enable READ enabled WRITE setEnabled)
    Q_PROPERTY(QObject* Type READ type CONSTANT)
    Q_PROPERTY(QObject* Frequency READ frequency CONSTANT)
    Q_PROPERTY(QObject* Depth READ depth CONSTANT)
public:
    using QObject::QObject;
    bool enabled() const { return m_enabled; }
    void setEnabled(bool v) { m_enabled=v; }
    QObject* type() { return &m_type; }
    QObject* frequency() { return &m_frequency; }
    QObject* depth() { return &m_depth; }
    QString waveform() const { return m_type.text(); }
    double hz() const { return m_frequency.value(); }
    double depthPercent() const { return m_depth.value(); }
private:
    bool m_enabled=false;
    TextSetting m_type{QStringLiteral("Sine"),nullptr,{"Sine","Triangle","Square","Ramp Up","Ramp Down"}};
    ValueSetting m_frequency{100,nullptr,QStringLiteral("Hz")};
    ValueSetting m_depth{25,nullptr,QStringLiteral("%")};
};
class WavegenModulation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* FM READ fm CONSTANT)
    Q_PROPERTY(QObject* AM READ am CONSTANT)
public:
    using QObject::QObject;
    QObject* fm() { return &m_fm; }
    QObject* am() { return &m_am; }
    WavegenModulationNode& fmNode() { return m_fm; }
    WavegenModulationNode& amNode() { return m_am; }
private:
    WavegenModulationNode m_fm, m_am;
};
class WavegenSweepAxis final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enable READ enabled WRITE setEnabled)
    Q_PROPERTY(QObject* Start READ start CONSTANT)
    Q_PROPERTY(QObject* Stop READ stop CONSTANT)
    Q_PROPERTY(QObject* Time READ time CONSTANT)
public:
    using QObject::QObject;
    bool enabled() const { return m_enabled; }
    void setEnabled(bool v) { m_enabled=v; }
    QObject* start() { return &m_start; }
    QObject* stop() { return &m_stop; }
    QObject* time() { return &m_time; }
    double from() const { return m_start.value(); }
    double to() const { return m_stop.value(); }
    double seconds() const { return m_time.value(); }
private:
    bool m_enabled=false;
    ValueSetting m_start{500};
    ValueSetting m_stop{1500};
    ValueSetting m_time{1,nullptr,QStringLiteral("s")};
};
class WavegenSweep final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Frequency READ frequency CONSTANT)
    Q_PROPERTY(QObject* Amplitude READ amplitude CONSTANT)
public:
    using QObject::QObject;
    QObject* frequency() { return &m_frequency; }
    QObject* amplitude() { return &m_amplitude; }
    WavegenSweepAxis& frequencyAxis() { return m_frequency; }
    WavegenSweepAxis& amplitudeAxis() { return m_amplitude; }
private:
    WavegenSweepAxis m_frequency, m_amplitude;
};
class WavegenTrigger final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Source READ source CONSTANT)
    Q_PROPERTY(QObject* Idle READ idle CONSTANT)
    Q_PROPERTY(bool RepeatTrigger READ repeatTrigger WRITE setRepeatTrigger)
public:
    using QObject::QObject;
    QObject* source() { return &m_source; }
    QObject* idle() { return &m_idle; }
    bool repeatTrigger() const { return m_repeatTrigger; }
    void setRepeatTrigger(bool v) { m_repeatTrigger=v; }
    QString sourceText() const { return m_source.text(); }
    QString idleText() const { return m_idle.text(); }
private:
    TextSetting m_source{QStringLiteral("None"),nullptr,{"None","PC","Detector 1","Detector 2","Wavegen 1","Wavegen 2","External 1","External 2"}};
    TextSetting m_idle{QStringLiteral("Offset"),nullptr,{"Disable","Offset","Initial"}};
    bool m_repeatTrigger=false;
};
class WavegenPlay final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Rate READ rate CONSTANT)
    Q_PROPERTY(int length READ length)
public:
    using QObject::QObject;
    QObject* rate() { return &m_rate; }
    int length() const { return m_samples.size(); }
    double hz() const { return m_rate.value(); }
    const QVector<double>& data() const { return m_samples; }
    Q_INVOKABLE bool set(const QVariantList& values);
private:
    ValueSetting m_rate{10000,nullptr,QStringLiteral("Sa/s")};
    QVector<double> m_samples;
};
class WavegenChannel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Simple READ simple CONSTANT)
    Q_PROPERTY(bool enable READ enabled WRITE setEnabled)
    Q_PROPERTY(QObject* Mode READ mode CONSTANT)
    Q_PROPERTY(QObject* Custom READ custom CONSTANT)
    Q_PROPERTY(QObject* Burst READ burst CONSTANT)
    Q_PROPERTY(QObject* Modulation READ modulation CONSTANT)
    Q_PROPERTY(QObject* Sweep READ sweep CONSTANT)
    Q_PROPERTY(QObject* Trigger READ trigger CONSTANT)
    Q_PROPERTY(QObject* Play READ playSettings CONSTANT)
public:
    using QObject::QObject;
    bool enabled() const { return m_enabled; }
    void setEnabled(bool value) { m_enabled = value; }
    QObject* simple() { return &m_simple; }
    QObject* mode() { return &m_mode; }
    QObject* custom() { return &m_custom; }
    QObject* burst() { return &m_burst; }
    QObject* modulation() { return &m_modulation; }
    QObject* sweep() { return &m_sweep; }
    QObject* trigger() { return &m_trigger; }
    QObject* playSettings() { return &m_play; }
    const WavegenCustom& customSettings() const { return m_custom; }
    const WavegenBurst& burstSettings() const { return m_burst; }
    WavegenSimple& settings() { return m_simple; }
    WavegenModulation& modulationSettings() { return m_modulation; }
    WavegenSweep& sweepSettings() { return m_sweep; }
    WavegenTrigger& triggerSettings() { return m_trigger; }
    WavegenPlay& playData() { return m_play; }
private:
    WavegenSimple m_simple;
    WavegenCustom m_custom;
    WavegenBurst m_burst;
    WavegenModulation m_modulation;
    WavegenSweep m_sweep;
    WavegenTrigger m_trigger;
    WavegenPlay m_play;
    TextSetting m_mode{QStringLiteral("Simple"),nullptr,{"Simple","Sweep","Play"}};
    bool m_enabled=false;
};
class WavegenBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Channel1 READ channel1 CONSTANT)
    Q_PROPERTY(QObject* Channel2 READ channel2 CONSTANT)
    Q_PROPERTY(QString lastError READ lastError)
public:
    WavegenBridge(DwfRuntime* runtime, MockInstrumentState* mock, const std::atomic_bool* stop=nullptr);
    QObject* channel1() { return &m_ch1; }
    QObject* channel2() { return &m_ch2; }
    QString lastError() const { return m_error; }
    Q_INVOKABLE bool config();
    Q_INVOKABLE bool run();
    Q_INVOKABLE bool start() { return run(); }
    Q_INVOKABLE void stop();
    Q_INVOKABLE QVariantMap capabilities(int channelNumber=1);
    Q_INVOKABLE QVariantMap status(int channelNumber=1);
    // Blocking streaming playback, runs on the script worker thread.
    Q_INVOKABLE bool play(int channelNumber, double timeoutSeconds);
private:
    DwfRuntime* m_runtime;
    MockInstrumentState* m_mock;
    const std::atomic_bool* m_stop=nullptr;
    WavegenChannel m_ch1, m_ch2;
    bool m_running[2] = {false,false};
    QString m_error;
};
class LogicTime final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Rate READ rate CONSTANT)
    Q_PROPERTY(QObject* Samples READ samples CONSTANT)
public:
    QObject* rate() { return &m_rate; }
    QObject* samples() { return &m_samples; }
    double sampleCount() const { return m_samples.value(); }
    double sampleRate() const { return m_rate.value(); }
    void effectiveRate(double hz) { m_rate.setReal(hz); }
    void effectiveSamples(int count) { m_samples.setReal(count); }
private:
    ValueSetting m_rate{20000.0,nullptr,QStringLiteral("Hz")};
    ValueSetting m_samples{4096.0};
};
class LogicBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
    Q_PROPERTY(QObject* Time READ time CONSTANT)
    Q_PROPERTY(double sampleRateHz READ sampleRateHz)
public:
    QObject* time() { return &m_time; }
    double sampleRateHz() const { return m_effectiveRateHz; }
    LogicBridge(DwfRuntime* runtime, MockInstrumentState* mock, const std::atomic_bool* stop);
    QString lastError() const { return m_error; }
    Q_INVOKABLE bool single();
    Q_INVOKABLE bool wait();
    Q_INVOKABLE bool wait(double timeoutSeconds);
    Q_INVOKABLE QVariantList samples() const; // extension: raw 16-bit digital sample words
    Q_INVOKABLE QVariantMap captureDiagnostics() const; // WFScript: byte count and half-buffer activity
    Q_INVOKABLE void stop();
private:
    DwfRuntime* m_runtime;
    MockInstrumentState* m_mock;
    const std::atomic_bool* m_stop;
    QVector<quint16> m_data;
    QString m_error;
    bool m_started = false;
    int m_count = 1024;
    LogicTime m_time;
    double m_effectiveRateHz=0;
};
class DeviceBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString SN READ serial CONSTANT)
public:
    DeviceBridge(DwfRuntime* runtime, bool demo) : m_runtime(runtime), m_demo(demo) {}
    Q_INVOKABLE bool isConnected() const { return m_demo || (m_runtime && m_runtime->connected()); }
    QString name() const { return m_demo ? QStringLiteral("Simulated Analog Discovery") : m_runtime->deviceName(); }
    QString serial() const { return m_demo ? QStringLiteral("DEMO") : m_runtime->serial(); }
private:
    DwfRuntime* m_runtime;
    bool m_demo;
};
class ScriptUtilities final : public QObject {
    Q_OBJECT
public:
    explicit ScriptUtilities(const std::atomic_bool* stop) : m_stop(stop) {}
    Q_INVOKABLE bool wait(double seconds) const;
    Q_INVOKABLE bool wait() const { return wait(0.05); }
private: const std::atomic_bool* m_stop;
};
