#pragma once

#include "ScopeTrigger.h"
#include <QLibrary>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <atomic>

// Independently written, small, header-free wrapper for the *documented* WaveForms SDK ABI.
// Only the script worker thread may access an instance. No vendor binaries are included.
class DwfRuntime {
public:
    struct DeviceInfo { int index = 0; QString name; QString serial; int configurations = 0; };
    DwfRuntime();
    ~DwfRuntime();
    bool load(QString* error);
    QVector<DeviceInfo> enumerate(QString* error);
    bool open(int deviceIndex, int configIndex, QString* error);
    void close();
    bool connected() const { return m_handle != 0; }
    QString deviceName() const { return m_name; }
    QString serial() const { return m_serial; }
    QString lastError() const;
    bool setupScope(double sampleRate, int samples, double range1, double range2,
                    double offset1, double offset2, double attenuation1, double attenuation2, QString* error);
    // v5.4: mode, filter, streaming and SDK capability discovery.
    bool configureScopeAdvanced(int mode, double recordSeconds, int filter1, int filter2, QString* error);
    QVariantMap scopeCapabilities(QString* error);
    // Discover SDK-advertised Vpp steps, not distinct physical input gain stages.
    // Non-mutating; missing optional exports return available=false, steps=[].
    QVariantMap scopeRangeCapabilities(QString* error);
    QVariantMap scopeHardwareStatus(QString* error);
    QVariantMap scopeTriggerTime() const;
    int scopeAcquisitionMode() const { return m_scopeAcqMode; }
    bool scopeReadScan(QVector<double>& a, QVector<double>& b, double timeoutSeconds,
                       const std::atomic_bool& stop, QString* error);
    bool scopeReadRecord(int totalSamples, QVector<double>& a, QVector<double>& b,
                         double timeoutSeconds, const std::atomic_bool& stop, QString* error);
    int recordLostSamples() const { return m_recordLost; }
    int recordCorruptSamples() const { return m_recordCorrupt; }
    bool configureScopeTrigger(const ScopeTriggerSettings& trigger, QString* error);
    int scopeSamples() const { return m_scopeSamples; }
    double scopeRateHz() const { return m_scopeRateHz; }
    bool scopeChannelReadback(int channelIndex, double* range, double* offset, double* attenuation, QString* error);
    // Rearm without reprogramming unchanged input/trigger settings. Waiting for
    // analog settling happens here only after actual front-end configuration.
    bool scopeStart(QString* error, const std::atomic_bool* stop = nullptr);
    int scopeSetupCount() const { return m_scopeSetupCount; }
    int scopeTriggerSetupCount() const { return m_scopeTriggerSetupCount; }
    int scopeArmCount() const { return m_scopeArmCount; }
    int scopeLastSettleMs() const { return m_scopeLastSettleMs; }
    bool scopeStop(QString* error);
    bool scopeWait(double seconds, const std::atomic_bool& stop, QString* error);
    bool scopePoll(bool* completed, QString* error);
    bool scopeAutoTriggered(bool* autoTriggered, QString* error);
    bool scopeTriggerPosition(double* configuredSeconds, double* measuredSeconds, QString* error);
    bool verifyScopeTrigger(const ScopeTriggerSettings& trigger, QString* error);
    bool scopeTriggerDetectorReadback(int* type, double* level, double* hysteresis, QString* error);
    bool readScope(int channelIndex, int samples, QVector<double>& out, QString* error);
    bool setupWavegen(int channelIndex, const QString& type, double hz, double amplitude, double offset,
                      double symmetry, double phase, const QVector<double>& custom,
                      double runSeconds, double waitSeconds, int repeats, int master, QString* error);
    struct WaveAdvanced {
        bool fm=false, am=false;
        QString fmType="Sine", amType="Sine";
        double fmHz=100, amHz=100, fmDepth=0, amDepth=0;
        int triggerSource=0, idle=1;
        bool repeatTrigger=false;
    };
    bool setupWavegenAdvanced(int channelIndex, const WaveAdvanced& options, QString* error);
    QVariantMap wavegenCapabilities(int channelIndex, QString* error);
    QVariantMap wavegenStatus(int channelIndex, QString* error);
    bool playWavegen(int channelIndex, double sampleRate, const QVector<double>& data,
                     double timeoutSeconds, const std::atomic_bool* stop, QString* error);
    bool setupWavegenSine(int channelIndex, double hz, double amplitude, double offset, QString* error);
    // A slave channel must be configured (fStart=0) before starting its master.
    // This is distinct from starting a standalone waveform generator.
    bool prepareWavegenSlave(int channelIndex, QString* error);
    bool startWavegen(int channelIndex, QString* error);
    bool stopWavegen(int channelIndex, QString* error);
    // AD3 programmable V+/V- supplies (AnalogIO channels 0/1, nodes Enable=0, Voltage=1).
    // Setpoints are committed only by explicit script action. Never touch supplies
    // until the bridge requests a configuration or stop.
    bool configureSupplies(bool master, bool positiveEnable, double positiveVolts,
                           bool negativeEnable, double negativeVolts, QString* error);
    bool stopSupplies(QString* error);
    bool suppliesReadback(double* positiveVolts, double* negativeVolts, QString* error);
    bool suppliesAvailable() const;
    // Static digital I/O, 16-bit AD3 masks. The same pins are shared with the
    // Pattern Generator, so StaticIO owns the output mask once configured.
    // No SDK state is changed by querying capabilities or inputs.
    bool staticIOCapabilities(quint32* outputMask, quint32* inputMask, QString* error);
    bool staticIOApply(quint32 enableMask, quint32 outputMask, QString* error);
    bool staticIORead(quint32* inputMask, QString* error);
    bool staticIOStop(QString* error);
    // v5.6.48 single-channel DigitalOut pattern generator: one clock/PWM
    // output, selected DIO pin. start=false stages configuration with Idle=Z.
    // No pattern outputs are modified until an explicit config()/run().
    bool patternClockConfigure(int pin, double hz, double dutyPercent, bool start,
                               double* actualHz, double* actualDuty, double* clockHz, QString* error);
    // Three phase-coherent, hardware-timed custom DigitalOut sequences:
    // A/B quadrature + one-index-pulse-per-revolution. The three DIO channels
    // share a divider and are started with ONE DigitalOutConfigure call.
    bool patternEncoderConfigure(int aPin, int bPin, int indexPin, int ppr,
                                 double rpm, bool reverse, bool start,
                                 double* actualRpm, double* clockHz, QString* error);
    // WFScript extension: 1..3 synchronized looping 0/1 bit strings.
    // Each string is one period with MSB/LSB explicitly ordered as written:
    // the first character is the first output bit in time. All channels
    // share the same native divider and one DigitalOutConfigure start.
    bool patternSequenceConfigure(const QVector<int>& pins, const QStringList& bitStrings,
                                  double bitRateHz, bool start, double* actualRateHz,
                                  double* clockHz, int* repeatFactor, QString* error);
    bool patternStop(QString* error);
    bool patternRunning() const { return m_patternRunning; }
    // SDK-read hardware state, not the PatternsBridge cached running flag.
    QVariantMap patternHardwareStatus() const;
    int logicLastValidSamples() const { return m_logicLastValidSamples; }
    int logicLastStateCode() const { return m_logicLastStateCode; }
    int logicLastBytesRequested() const { return m_logicLastBytesRequested; }
    bool setupLogic(int samples, double requestedRateHz, double* effectiveRateHz, QString* error);
    bool logicStart(QString* error);
    bool logicStop(QString* error);
    bool logicWait(double seconds, const std::atomic_bool& stop, QString* error);
    bool readLogic(int samples, QVector<quint16>& out, QString* error);
private:
    using H = int;
    QLibrary m_lib;
    H m_handle = 0;
    QString m_name;
    QString m_serial;
    int m_scopeSamples = 0;
    double m_scopeRateHz = 0;

    struct AppliedScope {
        bool valid = false;
        double sampleRate = 0;
        int samples = 0;
        double range1 = 0, range2 = 0;
        double offset1 = 0, offset2 = 0;
        double attenuation1 = 1, attenuation2 = 1;
    } m_appliedScope;
    ScopeTriggerSettings m_appliedTrigger;
    bool m_appliedTriggerValid = false;
    QElapsedTimer m_scopeSettleTimer;
    bool m_scopeSettlePending = false;
    // Conservative Digilent SDK initial input/offset settling recommendation.
    // Applied only when the analog front-end is first configured or changes.
    static constexpr qint64 kAnalogSettleMs = 2000;
    int m_scopeSetupCount = 0;
    int m_scopeTriggerSetupCount = 0;
    int m_scopeArmCount = 0;
    int m_scopeLastSettleMs = 0;
    int m_scopeAcqMode = 0;
    double m_scopeRecordSeconds = 1.0;
    int m_scopeFilters[2] = {0,0};
    bool m_scopeAdvancedValid = false;
    int m_recordLost = 0, m_recordCorrupt = 0;
    int m_scanLastIndex = -1;

    // BOOL is a 32-bit int; DwfState/TRIGSRC/FUNC are BYTE in dwf.h.
    using EnumFn = int (*)(int, int*);
    using EnumNameFn = int (*)(int, char*);
    using EnumConfigFn = int (*)(int, int*);
    using OpenConfigFn = int (*)(int, int, H*);
    using CloseFn = int (*)(H);
    using ErrorFn = int (*)(char*);
    using ScopeSetIntFn = int (*)(H, int);
    using ScopeSetDoubleFn = int (*)(H, double);
    using ChannelSetIntFn = int (*)(H, int, int);
    using ChannelSetDoubleFn = int (*)(H, int, double);
    using ScopeConfigureFn = int (*)(H, int, int);
    using ScopeStatusFn = int (*)(H, int, unsigned char*);
    using ScopeDataFn = int (*)(H, int, double*, int);
    using ScopeTriggerFn = int (*)(H, unsigned char);
    using ScopeGetByteFn = int (*)(H, unsigned char*);
    using DeviceAutoConfigureFn = int (*)(H, int);
    using WaveNodeIntFn = int (*)(H, int, int, int);
    using WaveNodeFuncFn = int (*)(H, int, int, unsigned char);
    using WaveNodeDoubleFn = int (*)(H, int, int, double);
    using WaveConfigureFn = int (*)(H, int, int);
    using ScopeRecordStatusFn = int (*)(H, int*, int*, int*);
    using ScopeStatusTimeFn = int (*)(H, unsigned int*, unsigned int*, unsigned int*);
    using ScopeData2Fn = int (*)(H, int, double*, int, int);
    using ScopeInfoIntFn = int (*)(H, int*);
    using ScopeInfoDoubleFn = int (*)(H, double*, double*);
    using ScopeInfoSizeFn = int (*)(H, int*, int*);
    using ScopeFilterFn = int (*)(H, int, int);
    using LogicSetIntFn = int (*)(H, int);
    using LogicConfigureFn = int (*)(H, int, int);
    using LogicStatusFn = int (*)(H, int, unsigned char*);
    using LogicDataFn = int (*)(H, void*, int);

    using AnalogEnableFn = int (*)(H, int);
    using AnalogConfigureFn = int (*)(H);
    using AnalogStatusFn = int (*)(H);
    using AnalogNodeSetFn = int (*)(H, int, int, double);
    using AnalogNodeStatusFn = int (*)(H, int, int, double*);
    AnalogEnableFn fSupplyMasterEnable=nullptr;
    AnalogConfigureFn fSupplyConfigure=nullptr;
    AnalogStatusFn fSupplyStatus=nullptr;
    AnalogNodeSetFn fSupplyNodeSet=nullptr;
    AnalogNodeStatusFn fSupplyNodeStatus=nullptr;
    EnumFn fEnum = nullptr;
    EnumNameFn fEnumName = nullptr, fEnumSN = nullptr;
    EnumConfigFn fEnumConfig = nullptr;
    OpenConfigFn fOpen = nullptr;
    CloseFn fClose = nullptr;
    ErrorFn fError = nullptr;
    ChannelSetIntFn fScopeEnable = nullptr;
    ChannelSetDoubleFn fScopeRange = nullptr, fScopeOffset = nullptr, fScopeAttenuation = nullptr;
    using ChannelGetDoubleFn = int (*)(H, int, double*);
    ChannelGetDoubleFn fScopeRangeGet = nullptr, fScopeOffsetGet = nullptr, fScopeAttenuationGet = nullptr;
    using ScopeRangeInfoFn = int (*)(H, double*, double*, double*);
    using ScopeRangeStepsFn = int (*)(H, double*, int*);
    ScopeRangeInfoFn fScopeRangeInfo = nullptr;
    ScopeRangeStepsFn fScopeRangeSteps = nullptr;
    ScopeSetDoubleFn fScopeHz = nullptr;
    ScopeSetIntFn fScopeSize = nullptr;
    using ScopeGetIntFn = int (*)(H, int*);
    using ScopeGetDoubleFn = int (*)(H, double*);
    ScopeGetIntFn fScopeSizeGet = nullptr;
    ScopeGetDoubleFn fScopeHzGet = nullptr;
    ScopeConfigureFn fScopeConfigure = nullptr;
    ScopeStatusFn fScopeStatus = nullptr;
    ScopeDataFn fScopeData = nullptr;
    ScopeTriggerFn fScopeTrigger = nullptr;
    DeviceAutoConfigureFn fDeviceAutoConfigure = nullptr;
    ScopeSetIntFn fScopeMode = nullptr;
    ScopeInfoIntFn fScopeModeInfo = nullptr, fScopeSamplesValid = nullptr, fScopeIndexWrite = nullptr;
    ScopeInfoIntFn fScopeTriggerSourceInfo = nullptr, fScopeTriggerTypeInfo = nullptr;
    ScopeInfoIntFn fScopeFilterInfo = nullptr;
    ScopeInfoDoubleFn fScopeRateInfo = nullptr;
    ScopeInfoSizeFn fScopeBufferInfo = nullptr;
    ScopeSetDoubleFn fScopeRecordLength = nullptr;
    ScopeRecordStatusFn fScopeRecordStatus = nullptr;
    ScopeStatusTimeFn fScopeStatusTime = nullptr;
    ScopeData2Fn fScopeData2 = nullptr;
    ScopeFilterFn fScopeChannelFilter = nullptr;
    ScopeSetIntFn fTriggerLengthCondition = nullptr, fTriggerFilter = nullptr;
    ScopeSetDoubleFn fTriggerLength = nullptr, fTriggerHoldOff = nullptr;
    ScopeGetIntFn fScopeAutoTriggered = nullptr;
    ScopeGetByteFn fTriggerSourceGet = nullptr;
    ScopeGetIntFn fTriggerConditionGet = nullptr, fTriggerChannelGet = nullptr;
    ScopeGetDoubleFn fTriggerTimeoutGet = nullptr;
    ScopeGetDoubleFn fTriggerLevelGet = nullptr, fTriggerHysteresisGet = nullptr;
    ScopeGetIntFn fTriggerTypeGet = nullptr;
    ScopeSetDoubleFn fTriggerAutoTimeout = nullptr;
    ScopeSetDoubleFn fTriggerPosition = nullptr;
    ScopeGetDoubleFn fTriggerPositionGet = nullptr, fTriggerPositionStatus = nullptr;
    ScopeSetDoubleFn fTriggerLevel = nullptr, fTriggerHysteresis = nullptr;
    ScopeSetIntFn fTriggerChannel = nullptr;
    ScopeSetIntFn fTriggerType = nullptr;
    ScopeSetIntFn fTriggerCondition = nullptr;
    WaveNodeIntFn fWaveEnable = nullptr;
    WaveNodeFuncFn fWaveFunction = nullptr;
    using WaveNodeFuncGetFn = int (*)(H,int,int,unsigned char*);
    WaveNodeFuncGetFn fWaveFunctionGet = nullptr;
    WaveNodeDoubleFn fWaveHz = nullptr, fWaveAmplitude = nullptr, fWaveOffset = nullptr;
    WaveNodeDoubleFn fWaveSymmetry = nullptr, fWavePhase = nullptr;
    using WaveDataFn = int (*)(H, int, int, double*, int);
    using WaveDataInfoFn = int (*)(H, int, int, int*, int*);
    WaveDataFn fWaveData = nullptr;
    WaveDataInfoFn fWaveDataInfo = nullptr;
    using WaveChannelDoubleFn = int (*)(H, int, double);
    using WaveChannelIntFn = int (*)(H, int, int);
    WaveChannelDoubleFn fWaveRun = nullptr, fWaveWait = nullptr;
    WaveChannelIntFn fWaveRepeat = nullptr, fWaveMaster = nullptr;
    WaveConfigureFn fWaveConfigure = nullptr;
    using WaveNodeInfoFn = int (*)(H,int,int*);
    using WaveNodeFuncInfoFn = int (*)(H,int,int,int*);
    using WaveStatusFn = int (*)(H,int,unsigned char*);
    using WaveGetDoubleFn = int (*)(H,int,double*);
    using WavePlayStatusFn = int (*)(H,int,int,int*,int*,int*);
    WaveNodeInfoFn fWaveNodeInfo=nullptr, fWaveIdleInfo=nullptr, fWaveTriggerSourceInfo=nullptr;
    WaveNodeFuncInfoFn fWaveFunctionInfo=nullptr;
    using WaveTriggerSourceFn = int (*)(H,int,unsigned char); // TRIGSRC is BYTE in dwf.h
    WaveTriggerSourceFn fWaveTriggerSourceSet=nullptr;
    WaveChannelIntFn fWaveIdleSet=nullptr, fWaveRepeatTriggerSet=nullptr;
    WaveStatusFn fWaveStatus=nullptr;
    WaveGetDoubleFn fWaveRunStatus=nullptr;
    WavePlayStatusFn fWavePlayStatus=nullptr;
    WaveDataFn fWavePlayData=nullptr;
    int m_playLost[2]={0,0}, m_playCorrupt[2]={0};
    using DigitalIOConfigureFn = int (*)(H);
    using DigitalIOInfoFn = int (*)(H, unsigned int*);
    using DigitalIOSetFn = int (*)(H, unsigned int);
    DigitalIOConfigureFn fDigitalIOConfigure=nullptr, fDigitalIOStatus=nullptr;
    DigitalIOInfoFn fDigitalIOOutputInfo=nullptr, fDigitalIOInputInfo=nullptr;
    DigitalIOInfoFn fDigitalIOOutputEnableGet=nullptr, fDigitalIOOutputGet=nullptr;
    DigitalIOInfoFn fDigitalIOInputStatus=nullptr;
    DigitalIOSetFn fDigitalIOOutputEnableSet=nullptr, fDigitalIOOutputSet=nullptr;
    // Optional DigitalOut SDK exports, resolved independently of DigitalIn.
    using PatternConfigureFn=int (*)(H,int);
    using PatternClockFn=int (*)(H,double*);
    using PatternIntFn=int (*)(H,int*);
    using PatternSetIntFn=int (*)(H,int,int);
    using PatternSetUIntFn=int (*)(H,int,unsigned int);
    using PatternSetCounterFn=int (*)(H,int,unsigned int,unsigned int);
    using PatternCounterInitFn=int (*)(H,int,int,unsigned int);
    using PatternRangeFn=int (*)(H,int,unsigned int*,unsigned int*);
    using PatternDataInfoFn=int (*)(H,int,unsigned int*);
    using PatternDataSetFn=int (*)(H,int,const void*,unsigned int);
    using PatternTimeFn=int (*)(H,double);
    using PatternTriggerFn=int (*)(H,unsigned char);
    using PatternStatusFn=int (*)(H,unsigned char*); // DwfState is BYTE, not int
    using PatternRepeatSetFn=int (*)(H,unsigned int);
    using PatternRepeatGetFn=int (*)(H,unsigned int*);
    PatternConfigureFn fPatternConfigure=nullptr;
    PatternStatusFn fPatternStatus=nullptr;
    PatternRepeatSetFn fPatternRepeatSet=nullptr;
    PatternRepeatGetFn fPatternRepeatGet=nullptr;
    PatternClockFn fPatternClockInfo=nullptr;
    PatternIntFn fPatternCount=nullptr;
    PatternSetIntFn fPatternEnable=nullptr,fPatternOutput=nullptr,fPatternIdle=nullptr,fPatternType=nullptr;
    PatternSetUIntFn fPatternDivider=nullptr,fPatternDividerInit=nullptr;
    PatternSetCounterFn fPatternCounter=nullptr;
    PatternCounterInitFn fPatternCounterInit=nullptr;
    PatternRangeFn fPatternCounterInfo=nullptr,fPatternDividerInfo=nullptr;
    PatternDataInfoFn fPatternDataInfo=nullptr;
    PatternDataSetFn fPatternDataSet=nullptr;
    PatternTimeFn fPatternRunTime=nullptr;
    PatternTriggerFn fPatternTrigger=nullptr;
    bool m_patternRunning=false;
    bool m_patternTouched=false;
    int m_patternPin=-1;
    quint32 m_patternPinMask=0; // only channels actually owned by WFScript Patterns
    LogicSetIntFn fLogicFormat = nullptr, fLogicSize = nullptr, fLogicDivider = nullptr;
    LogicConfigureFn fLogicConfigure = nullptr;
    LogicStatusFn fLogicStatus = nullptr;
    LogicDataFn fLogicData = nullptr;
    using LogicClockInfoFn = int (*)(H, double*);
    using LogicSizeInfoFn = int (*)(H, int*);
    using LogicTriggerSourceFn = int (*)(H, unsigned char);
    LogicClockInfoFn fLogicClockInfo = nullptr;
    LogicSizeInfoFn fLogicSizeInfo = nullptr;
    LogicTriggerSourceFn fLogicTriggerSource = nullptr;
    using LogicSamplesValidFn = int (*)(H,int*);
    LogicSamplesValidFn fLogicSamplesValid=nullptr;
    int m_logicLastValidSamples=-1; // -1 means query unavailable/failed
    int m_logicLastStateCode=-1;
    int m_logicLastBytesRequested=0;

    bool fail(QString* error, const QString& action) const;
};
