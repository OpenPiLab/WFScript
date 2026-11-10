#include "DwfRuntime.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QThread>
#include <QtGlobal>
#include <type_traits>
#include <utility>
#include <cmath>

DwfRuntime::DwfRuntime() = default;
DwfRuntime::~DwfRuntime() { close(); }

bool DwfRuntime::load(QString* error) {
    if (m_lib.isLoaded()) return true;
    const QByteArray overridePath = qgetenv("WAVEFORMS_DWF_PATH");
    m_lib.setFileName(overridePath.isEmpty() ? QStringLiteral("dwf") : QString::fromLocal8Bit(overridePath));
    if (!m_lib.load()) {
        if (error) *error = QStringLiteral("Cannot load WaveForms SDK: %1. Set WAVEFORMS_DWF_PATH to the full dwf.dll path if needed.").arg(m_lib.errorString());
        return false;
    }
#define RESOLVE(member, symbol) \
    member = reinterpret_cast<decltype(member)>(m_lib.resolve(symbol)); \
    if (!member) { \
        if (error) *error = QStringLiteral("SDK missing required export: %1").arg(QStringLiteral(symbol)); \
        m_lib.unload(); return false; \
    }
    RESOLVE(fEnum, "FDwfEnum");
    RESOLVE(fEnumName, "FDwfEnumDeviceName");
    RESOLVE(fEnumSN, "FDwfEnumSN");
    RESOLVE(fEnumConfig, "FDwfEnumConfig");
    RESOLVE(fOpen, "FDwfDeviceConfigOpen");
    RESOLVE(fClose, "FDwfDeviceClose");
    RESOLVE(fError, "FDwfGetLastErrorMsg");
    RESOLVE(fScopeEnable, "FDwfAnalogInChannelEnableSet");
    RESOLVE(fScopeRange, "FDwfAnalogInChannelRangeSet");
    RESOLVE(fScopeOffset, "FDwfAnalogInChannelOffsetSet");
    RESOLVE(fScopeAttenuation, "FDwfAnalogInChannelAttenuationSet");
    RESOLVE(fScopeRangeGet, "FDwfAnalogInChannelRangeGet");
    RESOLVE(fScopeOffsetGet, "FDwfAnalogInChannelOffsetGet");
    RESOLVE(fScopeAttenuationGet, "FDwfAnalogInChannelAttenuationGet");
    RESOLVE(fScopeHz, "FDwfAnalogInFrequencySet");
    RESOLVE(fScopeSize, "FDwfAnalogInBufferSizeSet");
    RESOLVE(fScopeSizeGet, "FDwfAnalogInBufferSizeGet");
    RESOLVE(fScopeHzGet, "FDwfAnalogInFrequencyGet");
    RESOLVE(fScopeConfigure, "FDwfAnalogInConfigure");
    RESOLVE(fScopeStatus, "FDwfAnalogInStatus");
    RESOLVE(fScopeData, "FDwfAnalogInStatusData");
    RESOLVE(fScopeTrigger, "FDwfAnalogInTriggerSourceSet");
    RESOLVE(fDeviceAutoConfigure, "FDwfDeviceAutoConfigureSet");
    RESOLVE(fScopeMode, "FDwfAnalogInAcquisitionModeSet");
    RESOLVE(fScopeAutoTriggered, "FDwfAnalogInStatusAutoTriggered");
    // Advanced Scope features are resolved lazily as optional exports: an older
    // SDK still supports our hardware-proven single acquisition path.
#define RESOLVE_OPTIONAL(member, symbol) member = reinterpret_cast<decltype(member)>(m_lib.resolve(symbol));
    RESOLVE_OPTIONAL(fScopeModeInfo, "FDwfAnalogInAcquisitionModeInfo");
    RESOLVE_OPTIONAL(fScopeSamplesValid, "FDwfAnalogInStatusSamplesValid");
    RESOLVE_OPTIONAL(fScopeIndexWrite, "FDwfAnalogInStatusIndexWrite");
    RESOLVE_OPTIONAL(fScopeData2, "FDwfAnalogInStatusData2");
    RESOLVE_OPTIONAL(fScopeRecordLength, "FDwfAnalogInRecordLengthSet");
    RESOLVE_OPTIONAL(fScopeRecordStatus, "FDwfAnalogInStatusRecord");
    RESOLVE_OPTIONAL(fScopeStatusTime, "FDwfAnalogInStatusTime");
    RESOLVE_OPTIONAL(fScopeChannelFilter, "FDwfAnalogInChannelFilterSet");
    RESOLVE_OPTIONAL(fScopeFilterInfo, "FDwfAnalogInChannelFilterInfo");
    RESOLVE_OPTIONAL(fScopeRangeInfo, "FDwfAnalogInChannelRangeInfo");
    RESOLVE_OPTIONAL(fScopeRangeSteps, "FDwfAnalogInChannelRangeSteps");
    RESOLVE_OPTIONAL(fScopeRateInfo, "FDwfAnalogInFrequencyInfo");
    RESOLVE_OPTIONAL(fScopeBufferInfo, "FDwfAnalogInBufferSizeInfo");
    RESOLVE_OPTIONAL(fScopeTriggerSourceInfo, "FDwfAnalogInTriggerSourceInfo");
    RESOLVE_OPTIONAL(fScopeTriggerTypeInfo, "FDwfAnalogInTriggerTypeInfo");
    RESOLVE_OPTIONAL(fTriggerLengthCondition, "FDwfAnalogInTriggerLengthConditionSet");
    RESOLVE_OPTIONAL(fTriggerLength, "FDwfAnalogInTriggerLengthSet");
    RESOLVE_OPTIONAL(fTriggerHoldOff, "FDwfAnalogInTriggerHoldOffSet");
    RESOLVE_OPTIONAL(fTriggerFilter, "FDwfAnalogInTriggerFilterSet");
#undef RESOLVE_OPTIONAL

    RESOLVE(fTriggerSourceGet, "FDwfAnalogInTriggerSourceGet");
    RESOLVE(fTriggerConditionGet, "FDwfAnalogInTriggerConditionGet");
    RESOLVE(fTriggerChannelGet, "FDwfAnalogInTriggerChannelGet");
    RESOLVE(fTriggerTimeoutGet, "FDwfAnalogInTriggerAutoTimeoutGet");
    RESOLVE(fTriggerAutoTimeout, "FDwfAnalogInTriggerAutoTimeoutSet");
    RESOLVE(fTriggerPosition, "FDwfAnalogInTriggerPositionSet");
    RESOLVE(fTriggerPositionGet, "FDwfAnalogInTriggerPositionGet");
    RESOLVE(fTriggerPositionStatus, "FDwfAnalogInTriggerPositionStatus");
    RESOLVE(fTriggerChannel, "FDwfAnalogInTriggerChannelSet");
    RESOLVE(fTriggerType, "FDwfAnalogInTriggerTypeSet");
    RESOLVE(fTriggerCondition, "FDwfAnalogInTriggerConditionSet");
    RESOLVE(fTriggerLevel, "FDwfAnalogInTriggerLevelSet");
    RESOLVE(fTriggerLevelGet, "FDwfAnalogInTriggerLevelGet");
    RESOLVE(fTriggerTypeGet, "FDwfAnalogInTriggerTypeGet");
    RESOLVE(fTriggerHysteresis, "FDwfAnalogInTriggerHysteresisSet");
    RESOLVE(fTriggerHysteresisGet, "FDwfAnalogInTriggerHysteresisGet");
    RESOLVE(fWaveEnable, "FDwfAnalogOutNodeEnableSet");
    RESOLVE(fWaveFunction, "FDwfAnalogOutNodeFunctionSet");
    RESOLVE(fWaveHz, "FDwfAnalogOutNodeFrequencySet");
    RESOLVE(fWaveAmplitude, "FDwfAnalogOutNodeAmplitudeSet");
    RESOLVE(fWaveOffset, "FDwfAnalogOutNodeOffsetSet");
    RESOLVE(fWaveSymmetry, "FDwfAnalogOutNodeSymmetrySet");
    RESOLVE(fWavePhase, "FDwfAnalogOutNodePhaseSet");
    RESOLVE(fWaveData, "FDwfAnalogOutNodeDataSet");
    RESOLVE(fWaveDataInfo, "FDwfAnalogOutNodeDataInfo");
    RESOLVE(fWaveRun, "FDwfAnalogOutRunSet");
    RESOLVE(fWaveWait, "FDwfAnalogOutWaitSet");
    RESOLVE(fWaveRepeat, "FDwfAnalogOutRepeatSet");
    RESOLVE(fWaveMaster, "FDwfAnalogOutMasterSet");
    RESOLVE(fWaveConfigure, "FDwfAnalogOutConfigure");
    // Release B: optional because older SDKs/devices may lack some features.
#define RESOLVE_OPTIONAL(member, symbol) member = reinterpret_cast<decltype(member)>(m_lib.resolve(symbol));
    RESOLVE_OPTIONAL(fWaveNodeInfo, "FDwfAnalogOutNodeInfo");
    RESOLVE_OPTIONAL(fWaveFunctionInfo, "FDwfAnalogOutNodeFunctionInfo");
    RESOLVE_OPTIONAL(fWaveFunctionGet, "FDwfAnalogOutNodeFunctionGet");
    RESOLVE_OPTIONAL(fWaveTriggerSourceInfo, "FDwfAnalogOutTriggerSourceInfo");
    RESOLVE_OPTIONAL(fWaveIdleInfo, "FDwfAnalogOutIdleInfo");
    RESOLVE_OPTIONAL(fWaveTriggerSourceSet, "FDwfAnalogOutTriggerSourceSet");
    RESOLVE_OPTIONAL(fWaveIdleSet, "FDwfAnalogOutIdleSet");
    RESOLVE_OPTIONAL(fWaveRepeatTriggerSet, "FDwfAnalogOutRepeatTriggerSet");
    RESOLVE_OPTIONAL(fWaveStatus, "FDwfAnalogOutStatus");
    RESOLVE_OPTIONAL(fWaveRunStatus, "FDwfAnalogOutRunStatus");
    RESOLVE_OPTIONAL(fWavePlayStatus, "FDwfAnalogOutNodePlayStatus");
    RESOLVE_OPTIONAL(fWavePlayData, "FDwfAnalogOutNodePlayData");
#undef RESOLVE_OPTIONAL
    // Optional so older runtime configurations remain usable without these exports.
#define RESOLVE_OPTIONAL(member, symbol) member = reinterpret_cast<decltype(member)>(m_lib.resolve(symbol));
    RESOLVE_OPTIONAL(fSupplyMasterEnable, "FDwfAnalogIOEnableSet");
    RESOLVE_OPTIONAL(fSupplyConfigure, "FDwfAnalogIOConfigure");
    RESOLVE_OPTIONAL(fSupplyStatus, "FDwfAnalogIOStatus");
    RESOLVE_OPTIONAL(fSupplyNodeSet, "FDwfAnalogIOChannelNodeSet");
    RESOLVE_OPTIONAL(fSupplyNodeStatus, "FDwfAnalogIOChannelNodeStatus");
#undef RESOLVE_OPTIONAL
    // Optional Static I/O support: keep existing Scope/Wavegen/Logic usable
    // with older versions of dwf.dll that lack these exports.
#define RESOLVE_OPTIONAL(member, symbol) member = reinterpret_cast<decltype(member)>(m_lib.resolve(symbol));
    RESOLVE_OPTIONAL(fDigitalIOConfigure, "FDwfDigitalIOConfigure");
    RESOLVE_OPTIONAL(fDigitalIOStatus, "FDwfDigitalIOStatus");
    RESOLVE_OPTIONAL(fDigitalIOOutputInfo, "FDwfDigitalIOOutputEnableInfo");
    RESOLVE_OPTIONAL(fDigitalIOInputInfo, "FDwfDigitalIOInputInfo");
    RESOLVE_OPTIONAL(fDigitalIOOutputEnableGet, "FDwfDigitalIOOutputEnableGet");
    RESOLVE_OPTIONAL(fDigitalIOOutputGet, "FDwfDigitalIOOutputGet");
    RESOLVE_OPTIONAL(fDigitalIOInputStatus, "FDwfDigitalIOInputStatus");
    RESOLVE_OPTIONAL(fDigitalIOOutputEnableSet, "FDwfDigitalIOOutputEnableSet");
    RESOLVE_OPTIONAL(fDigitalIOOutputSet, "FDwfDigitalIOOutputSet");
#undef RESOLVE_OPTIONAL
    // Pattern Generator support is optional; simulation and older dwf.dll
    // installations continue to load even if DigitalOut exports are missing.
#define RESOLVE_OPTIONAL(member, symbol) member = reinterpret_cast<decltype(member)>(m_lib.resolve(symbol));
    RESOLVE_OPTIONAL(fPatternConfigure, "FDwfDigitalOutConfigure");
    RESOLVE_OPTIONAL(fPatternClockInfo, "FDwfDigitalOutInternalClockInfo");
    RESOLVE_OPTIONAL(fPatternCount, "FDwfDigitalOutCount");
    RESOLVE_OPTIONAL(fPatternEnable, "FDwfDigitalOutEnableSet");
    RESOLVE_OPTIONAL(fPatternOutput, "FDwfDigitalOutOutputSet");
    RESOLVE_OPTIONAL(fPatternIdle, "FDwfDigitalOutIdleSet");
    RESOLVE_OPTIONAL(fPatternType, "FDwfDigitalOutTypeSet");
    RESOLVE_OPTIONAL(fPatternDivider, "FDwfDigitalOutDividerSet");
    RESOLVE_OPTIONAL(fPatternDividerInit, "FDwfDigitalOutDividerInitSet");
    RESOLVE_OPTIONAL(fPatternCounter, "FDwfDigitalOutCounterSet");
    RESOLVE_OPTIONAL(fPatternCounterInit, "FDwfDigitalOutCounterInitSet");
    RESOLVE_OPTIONAL(fPatternCounterInfo, "FDwfDigitalOutCounterInfo");
    RESOLVE_OPTIONAL(fPatternDividerInfo, "FDwfDigitalOutDividerInfo");
    RESOLVE_OPTIONAL(fPatternDataInfo, "FDwfDigitalOutDataInfo");
    RESOLVE_OPTIONAL(fPatternDataSet, "FDwfDigitalOutDataSet");
    RESOLVE_OPTIONAL(fPatternRunTime, "FDwfDigitalOutRunSet");
    RESOLVE_OPTIONAL(fPatternTrigger, "FDwfDigitalOutTriggerSourceSet");
    RESOLVE_OPTIONAL(fPatternStatus, "FDwfDigitalOutStatus");
    RESOLVE_OPTIONAL(fPatternRepeatSet, "FDwfDigitalOutRepeatSet");
    RESOLVE_OPTIONAL(fPatternRepeatGet, "FDwfDigitalOutRepeatGet");
#undef RESOLVE_OPTIONAL
    RESOLVE(fLogicFormat, "FDwfDigitalInSampleFormatSet");
    RESOLVE(fLogicSize, "FDwfDigitalInBufferSizeSet");
    RESOLVE(fLogicDivider, "FDwfDigitalInDividerSet");
    RESOLVE(fLogicConfigure, "FDwfDigitalInConfigure");
    RESOLVE(fLogicStatus, "FDwfDigitalInStatus");
    RESOLVE(fLogicData, "FDwfDigitalInStatusData");
    // SDK standard DigitalIn discovery, optional for older vendor runtimes.
    fLogicClockInfo = reinterpret_cast<decltype(fLogicClockInfo)>(m_lib.resolve("FDwfDigitalInInternalClockInfo"));
    fLogicSizeInfo = reinterpret_cast<decltype(fLogicSizeInfo)>(m_lib.resolve("FDwfDigitalInBufferSizeInfo"));
    fLogicTriggerSource = reinterpret_cast<decltype(fLogicTriggerSource)>(m_lib.resolve("FDwfDigitalInTriggerSourceSet"));
    fLogicSamplesValid = reinterpret_cast<decltype(fLogicSamplesValid)>(m_lib.resolve("FDwfDigitalInStatusSamplesValid"));
#undef RESOLVE
    return true;
}

QString DwfRuntime::lastError() const {
    char msg[512] = {};
    if (fError) fError(msg);
    return QString::fromLocal8Bit(msg);
}
bool DwfRuntime::fail(QString* error, const QString& action) const {
    if (error) *error = action + QStringLiteral(": ") + lastError();
    return false;
}
QVector<DwfRuntime::DeviceInfo> DwfRuntime::enumerate(QString* error) {
    QVector<DeviceInfo> result;
    if (!load(error)) return result;
    int count = 0;
    if (!fEnum(0, &count)) { fail(error, QStringLiteral("Device enumeration")); return result; }
    for (int i=0; i<count; ++i) {
        char name[64] = {}, sn[64] = {};
        int configs = 0;
        if (!fEnumName(i, name) || !fEnumSN(i, sn) || !fEnumConfig(i, &configs)) {
            fail(error, QStringLiteral("Reading device information")); return {};
        }
        result.push_back({i, QString::fromLocal8Bit(name), QString::fromLocal8Bit(sn), configs});
    }
    return result;
}
bool DwfRuntime::open(int deviceIndex, int configIndex, QString* error) {
    close();
    const auto devices = enumerate(error);
    if (deviceIndex < 0 || deviceIndex >= devices.size()) {
        if (error) *error = QStringLiteral("Invalid device index or no connected device.");
        return false;
    }
    const auto info = devices.at(deviceIndex);
    if (configIndex < 0 || configIndex >= info.configurations) {
        if (error) *error = QStringLiteral("Configuration %1 is not available; device offers %2.").arg(configIndex).arg(info.configurations);
        return false;
    }
    H handle = 0;
    if (!fOpen(deviceIndex, configIndex, &handle) || !handle) return fail(error, QStringLiteral("Opening device/configuration"));
    m_handle = handle;
    // Stage DWF parameter changes until an explicit *Configure call.
    // Otherwise every trigger Set can implicitly stop/reconfigure the instrument.
    if (!fDeviceAutoConfigure(m_handle, 0)) {
        const QString message = QStringLiteral("Disabling SDK auto-configuration: ") + lastError();
        close();
        if (error) *error = message;
        return false;
    }
    m_name = info.name;
    m_serial = info.serial;
    return true;
}
void DwfRuntime::close() {
    // Final best-effort DigitalOut shutdown if a caller bypassed PatternsBridge cleanup.
    // Bridge cleanup reports stop failures to the user before this fallback.
    if (m_handle && m_patternTouched) { QString ignored; patternStop(&ignored); }
    if (m_handle && fClose) fClose(m_handle);
    m_handle = 0;
    m_patternTouched = false; m_patternRunning = false; m_patternPin = -1; m_patternPinMask=0;
    m_scopeSamples = 0; m_scopeRateHz = 0;
    m_appliedScope = AppliedScope{};
    m_appliedTriggerValid = false;
    m_scopeSettlePending = false;
    m_scopeSettleTimer.invalidate();
    m_scopeSetupCount = m_scopeTriggerSetupCount = m_scopeArmCount = 0;
    m_scopeLastSettleMs = 0;
    m_scopeAdvancedValid = false;
    m_scopeAcqMode = 0;
    m_scopeRecordSeconds = 1.0;
    m_scopeFilters[0] = m_scopeFilters[1] = 0;
    m_scanLastIndex = -1;
    m_recordLost = m_recordCorrupt = 0;
    m_logicLastValidSamples=-1;
    m_logicLastStateCode=-1;
    m_logicLastBytesRequested=0;
    m_name.clear(); m_serial.clear();
}
bool DwfRuntime::setupScope(double sampleRate, int samples, double range1, double range2,
                            double offset1, double offset2, double attenuation1, double attenuation2,
                            QString* error) {
    if (!connected()) { if (error) *error = QStringLiteral("No instrument connected"); return false; }
    if (samples < 16 || samples > 1'048'576 || !qIsFinite(sampleRate) || sampleRate < 1 ||
        !qIsFinite(range1) || !qIsFinite(range2) || range1 <= 0 || range2 <= 0 ||
        !qIsFinite(offset1) || !qIsFinite(offset2) ||
        !qIsFinite(attenuation1) || !qIsFinite(attenuation2) ||
        attenuation1 < 1 || attenuation2 < 1 || attenuation1 > 1000 || attenuation2 > 1000) {
        if (error) *error = QStringLiteral("Invalid scope rate, count, range, offset or attenuation");
        return false;
    }
    // Compare requested values, never calibrated .real readbacks: the hardware
    // quantizes ranges, so requested 2 V and actual 5.16 V differ by design.
    const bool same = m_appliedScope.valid &&
        m_appliedScope.sampleRate == sampleRate && m_appliedScope.samples == samples &&
        m_appliedScope.range1 == range1 && m_appliedScope.range2 == range2 &&
        m_appliedScope.offset1 == offset1 && m_appliedScope.offset2 == offset2 &&
        m_appliedScope.attenuation1 == attenuation1 &&
        m_appliedScope.attenuation2 == attenuation2;
    if (same) return true; // No SDK *Set or *Configure, no relay activity requested.

    const AppliedScope previous = m_appliedScope;
    // Attenuation is an external-probe software correction; it is NOT a
    // physical gain relay change, so it does not need analog settling.
    const bool analogChanged = !previous.valid ||
        previous.range1 != range1 || previous.range2 != range2 ||
        previous.offset1 != offset1 || previous.offset2 != offset2;
    // Do not retain a cached state after a partially successful SDK sequence.
    m_appliedScope.valid = false;
    m_appliedTriggerValid = false; // setupScope resets the trigger source to None.
    m_scopeAdvancedValid = false; // commit may invalidate scan / record state
    // Write only modified settings. In particular, changing the sample rate
    // must NOT resend input ranges and provoke unnecessary relay activity.
    if ((!previous.valid && (!fScopeMode(m_handle, 0) || // proven v4.4 single mode
                             !fScopeEnable(m_handle, -1, 1))) ||
        ((!previous.valid || previous.sampleRate != sampleRate) && !fScopeHz(m_handle, sampleRate)) ||
        ((!previous.valid || previous.samples != samples) && !fScopeSize(m_handle, samples)) ||
        ((!previous.valid || previous.range1 != range1) && !fScopeRange(m_handle, 0, range1)) ||
        ((!previous.valid || previous.range2 != range2) && !fScopeRange(m_handle, 1, range2)) ||
        ((!previous.valid || previous.offset1 != offset1) && !fScopeOffset(m_handle, 0, offset1)) ||
        ((!previous.valid || previous.offset2 != offset2) && !fScopeOffset(m_handle, 1, offset2)) ||
        ((!previous.valid || previous.attenuation1 != attenuation1) &&
           !fScopeAttenuation(m_handle, 0, attenuation1)) ||
        ((!previous.valid || previous.attenuation2 != attenuation2) &&
           !fScopeAttenuation(m_handle, 1, attenuation2)) ||
        !fScopeTrigger(m_handle, 0) || !fScopeConfigure(m_handle, 1, 0))
        return fail(error, QStringLiteral("Configuring oscilloscope"));
    if (!fScopeSizeGet(m_handle, &m_scopeSamples) || !fScopeHzGet(m_handle, &m_scopeRateHz))
        return fail(error, QStringLiteral("Reading back actual oscilloscope settings"));
    if (m_scopeSamples < 16 || m_scopeSamples > 1'048'576 || m_scopeRateHz <= 0)
        return fail(error, QStringLiteral("Device returned invalid scope settings"));
    m_appliedScope = {true, sampleRate, samples, range1, range2, offset1, offset2,
                      attenuation1, attenuation2};
    ++m_scopeSetupCount;
    if (analogChanged) {
        m_scopeSettlePending = true;
        m_scopeSettleTimer.start(); // start after committing the front-end settings
    }
    return true;
}
bool DwfRuntime::scopeChannelReadback(int channelIndex, double* range, double* offset,
                                      double* attenuation, QString* error) {
    if (!connected() || channelIndex < 0 || channelIndex > 1 ||
        !fScopeRangeGet(m_handle, channelIndex, range) ||
        !fScopeOffsetGet(m_handle, channelIndex, offset) ||
        !fScopeAttenuationGet(m_handle, channelIndex, attenuation))
        return fail(error, QStringLiteral("Reading back Scope channel settings"));
    return true;
}
QVariantMap DwfRuntime::scopeTriggerTime() const {
    if(!connected() || !fScopeStatusTime) return {};
    unsigned int sec=0,tick=0,ticksPerSecond=0;
    if(!fScopeStatusTime(m_handle,&sec,&tick,&ticksPerSecond) || ticksPerSecond==0) return {};
    // Do not synthesize a synchronized UTC timestamp here. AD3's timestamp
    // counter is high resolution but not synchronized to other devices.
    return {{QStringLiteral("seconds"),quint64(sec)},
            {QStringLiteral("tick"),quint64(tick)},
            {QStringLiteral("ticksPerSecond"),quint64(ticksPerSecond)},
            {QStringLiteral("subsecondSeconds"),double(tick)/double(ticksPerSecond)},
            {QStringLiteral("source"),QStringLiteral("DWF AnalogInStatusTime")}};
}
QVariantMap DwfRuntime::scopeHardwareStatus(QString* error) {
    QVariantMap status;
    if (!connected()) { if(error) *error=QStringLiteral("No device"); return status; }
    unsigned char state=0;
    if (!fScopeStatus(m_handle,0,&state)) { fail(error,QStringLiteral("Scope state")); return {}; }
    status.insert(QStringLiteral("stateCode"),int(state));
    status.insert(QStringLiteral("rateHz"),m_scopeRateHz);
    status.insert(QStringLiteral("deviceBufferSamples"),m_scopeSamples);
    status.insert(QStringLiteral("acquisitionMode"),m_scopeAcqMode);
    status.insert(QStringLiteral("configurations"),m_scopeSetupCount);
    status.insert(QStringLiteral("triggerConfigurations"),m_scopeTriggerSetupCount);
    status.insert(QStringLiteral("arms"),m_scopeArmCount);
    status.insert(QStringLiteral("lastSettleMs"),m_scopeLastSettleMs);
    status.insert(QStringLiteral("recordLostSamples"),m_recordLost);
    status.insert(QStringLiteral("recordCorruptSamples"),m_recordCorrupt);
    bool triggered=false;
    if (m_scopeAcqMode==0 && fScopeAutoTriggered && !scopeAutoTriggered(&triggered,error)) return {};
    status.insert(QStringLiteral("autoTriggered"),triggered);
    if (fTriggerPositionGet && fTriggerPositionStatus && m_scopeAcqMode==0) {
        double configured=0, measured=0;
        if (!scopeTriggerPosition(&configured,&measured,error)) return {};
        status.insert(QStringLiteral("triggerPositionSeconds"),configured);
        status.insert(QStringLiteral("actualTriggerPositionSeconds"),measured);
    }
    return status;
}

QVariantMap DwfRuntime::scopeRangeCapabilities(QString* error) {
    QVariantMap result{{QStringLiteral("rangeStepsAvailable"),false},
                       {QStringLiteral("rangeSteps"),QVariantList{}}};
    if (!connected()) {
        if (error) *error=QStringLiteral("No instrument connected");
        return result;
    }
    // The SDK describes range in volts peak-to-peak. These are selectable
    // steps, not a claim that the AD3 has one physical gain stage per step.
    if (fScopeRangeInfo) {
        double minimum=0, maximum=0, count=0;
        if (fScopeRangeInfo(m_handle,&minimum,&maximum,&count) &&
            qIsFinite(minimum) && qIsFinite(maximum) && qIsFinite(count) &&
            minimum>0 && maximum>=minimum && count>=0) {
            result.insert(QStringLiteral("rangeMinVpp"),minimum);
            result.insert(QStringLiteral("rangeMaxVpp"),maximum);
            result.insert(QStringLiteral("rangeStepCount"),count);
        } else {
            result.insert(QStringLiteral("rangeInfoError"),QStringLiteral("SDK range info query failed or returned invalid values"));
        }
    }
    if (fScopeRangeSteps) {
        // Digilent documents a fixed 32-element output array. Never allocate
        // less than 32 elements, even when RangeInfo reports fewer steps.
        double raw[32] = {};
        int count=0;
        if (!fScopeRangeSteps(m_handle,raw,&count)) {
            result.insert(QStringLiteral("rangeStepsError"),QStringLiteral("SDK range steps query failed"));
        } else if (count<0 || count>32) {
            result.insert(QStringLiteral("rangeStepsError"),QStringLiteral("SDK returned an invalid range step count"));
        } else {
            QVariantList steps;
            steps.reserve(count);
            bool valid=true;
            for (int i=0;i<count;++i) {
                if (!qIsFinite(raw[i]) || raw[i]<=0) { valid=false; break; }
                steps.append(raw[i]);
            }
            if (valid) {
                result.insert(QStringLiteral("rangeSteps"),steps);
                result.insert(QStringLiteral("rangeStepsAvailable"),true);
            } else {
                result.insert(QStringLiteral("rangeStepsError"),QStringLiteral("SDK returned an invalid range voltage"));
            }
        }
    } else {
        result.insert(QStringLiteral("rangeStepsError"),QStringLiteral("Installed DWF SDK does not export RangeSteps"));
    }
    return result;
}

QVariantMap DwfRuntime::scopeCapabilities(QString* error) {
    QVariantMap c;
    if (!connected()) { if (error) *error = QStringLiteral("No instrument connected"); return c; }
    int modes=1, filters=1, types=1, sources=(1<<2), minBuffer=0, maxBuffer=0;
    double minHz=0, maxHz=0;
    if (fScopeModeInfo && !fScopeModeInfo(m_handle, &modes)) return (fail(error, QStringLiteral("Mode capabilities")), QVariantMap{});
    if (fScopeFilterInfo && !fScopeFilterInfo(m_handle, &filters)) return (fail(error, QStringLiteral("Filter capabilities")), QVariantMap{});
    if (fScopeTriggerTypeInfo && !fScopeTriggerTypeInfo(m_handle, &types)) return (fail(error, QStringLiteral("Trigger capabilities")), QVariantMap{});
    // Deprecated SDK SourceInfo may be absent or return an error on some DLLs.
    // In that case report only the universally supported analog detector.
    bool sourceQueryAvailable = fScopeTriggerSourceInfo != nullptr;
    if (fScopeTriggerSourceInfo && !fScopeTriggerSourceInfo(m_handle, &sources)) {
        sources=(1<<2);
        sourceQueryAvailable=false;
    }
    if (fScopeRateInfo && !fScopeRateInfo(m_handle, &minHz, &maxHz)) return (fail(error, QStringLiteral("Sample rate capabilities")), QVariantMap{});
    if (fScopeBufferInfo && !fScopeBufferInfo(m_handle, &minBuffer, &maxBuffer)) return (fail(error, QStringLiteral("Buffer capabilities")), QVariantMap{});
    c.insert(QStringLiteral("acquisitionModeMask"), modes);
    c.insert(QStringLiteral("channelFilterMask"), filters);
    c.insert(QStringLiteral("triggerTypeMask"), types);
    c.insert(QStringLiteral("triggerSourceMask"), sources);
    c.insert(QStringLiteral("triggerSourceQueryAvailable"),sourceQueryAvailable);
    c.insert(QStringLiteral("minRateHz"), minHz);
    c.insert(QStringLiteral("maxRateHz"), maxHz);
    c.insert(QStringLiteral("minBufferSamples"), minBuffer);
    c.insert(QStringLiteral("maxBufferSamples"), maxBuffer);
    c.insert(QStringLiteral("recordSupported"), bool((modes & (1<<3)) && fScopeRecordLength && fScopeRecordStatus));
    c.insert(QStringLiteral("screenSupported"), bool((modes & (1<<2)) && fScopeSamplesValid && fScopeIndexWrite && fScopeData2));
    c.insert(QStringLiteral("shiftSupported"), bool((modes & (1<<1)) && fScopeSamplesValid));
    return c;
}

bool DwfRuntime::configureScopeAdvanced(int mode, double recordSeconds, int filter1, int filter2, QString* error) {
    if (!connected() || !m_appliedScope.valid) { if(error) *error = QStringLiteral("Configure basic Scope first"); return false; }
    if (mode < 0 || mode > 3 || !qIsFinite(recordSeconds) || recordSeconds <= 0 || recordSeconds > 60 ||
        filter1 < 0 || filter1 > 2 || filter2 < 0 || filter2 > 2) {
        if (error) *error = QStringLiteral("Invalid Scope mode, record length, or sampling filter");
        return false;
    }
    if (m_scopeAdvancedValid && m_scopeAcqMode == mode && m_scopeFilters[0] == filter1 &&
        m_scopeFilters[1] == filter2 && (mode != 3 || m_scopeRecordSeconds == recordSeconds)) return true;
    // Preserve v5.3.3 initial single-shot configuration exactly. There is
    // no need for a second SDK configuration when mode and filters are default.
    if (!m_scopeAdvancedValid && mode==0 && filter1==0 && filter2==0 && m_scopeAcqMode==0) {
        m_scopeAdvancedValid=true;
        m_scopeRecordSeconds=recordSeconds;
        return true;
    }
    int modes=1, filters=1;
    if (fScopeModeInfo && !fScopeModeInfo(m_handle,&modes))
        return fail(error,QStringLiteral("Acquisition mode capabilities"));
    if (fScopeFilterInfo && !fScopeFilterInfo(m_handle,&filters))
        return fail(error,QStringLiteral("Sampling filter capabilities"));
    if (!(modes & (1<<mode))) { if (error) *error=QStringLiteral("Scope acquisition mode not supported by this device"); return false; }
    if (!(filters & (1<<filter1)) || !(filters & (1<<filter2))) {
        if (error) *error = QStringLiteral("Sampling filter not supported by this device");
        return false;
    }
    if ((mode==3 && (!fScopeRecordLength || !fScopeRecordStatus)) ||
        (mode==1 && !fScopeSamplesValid) || (mode==2 && (!fScopeSamplesValid || !fScopeIndexWrite || !fScopeData2)) ||
        !fScopeChannelFilter) {
        if (error) *error = QStringLiteral("Installed DWF SDK lacks this acquisition feature");
        return false;
    }
    const bool needsMode = !m_scopeAdvancedValid || mode != m_scopeAcqMode;
    const bool needsRecordLength = mode==3 && (!m_scopeAdvancedValid || m_scopeRecordSeconds != recordSeconds);
    const bool needsF1 = !m_scopeAdvancedValid || m_scopeFilters[0]!=filter1;
    const bool needsF2 = !m_scopeAdvancedValid || m_scopeFilters[1]!=filter2;
    m_scopeAdvancedValid = false;
    m_appliedTriggerValid = false;
    if ((needsMode && !fScopeMode(m_handle, mode)) ||
        (needsRecordLength && !fScopeRecordLength(m_handle,recordSeconds)) ||
        (needsF1 && !fScopeChannelFilter(m_handle,0,filter1)) ||
        (needsF2 && !fScopeChannelFilter(m_handle,1,filter2)) ||
        !fScopeConfigure(m_handle,1,0))
        return fail(error, QStringLiteral("Committing Scope acquisition mode/filter"));
    m_scopeAcqMode=mode; m_scopeRecordSeconds=recordSeconds;
    m_scopeFilters[0]=filter1; m_scopeFilters[1]=filter2;
    m_scanLastIndex = -1;
    m_scopeAdvancedValid=true;
    return true;
}

bool DwfRuntime::configureScopeTrigger(const ScopeTriggerSettings& trigger, QString* error) {
    if (!connected() || m_scopeSamples < 16 || m_scopeRateHz <= 0) {
        if (error) *error = QStringLiteral("Configure trigger requires an initialized scope");
        return false;
    }
    if (trigger.channel < 0 || trigger.channel > 1 ||
        !qIsFinite(trigger.levelVolts) || !qIsFinite(trigger.hysteresisVolts) ||
        trigger.hysteresisVolts < 0 || trigger.hysteresisVolts > 2.0 ||
        !qIsFinite(trigger.positionPercent) ||
        trigger.positionPercent < 0 || trigger.positionPercent > 100 ||
        !qIsFinite(trigger.autoTimeoutSeconds) ||
        trigger.autoTimeoutSeconds <= 0 || trigger.autoTimeoutSeconds > 10 ||
        trigger.type < 0 || trigger.type > 2 || trigger.lengthCondition < 0 ||
        trigger.lengthCondition > 2 || !qIsFinite(trigger.lengthSeconds) ||
        trigger.lengthSeconds <= 0 || !qIsFinite(trigger.holdoffSeconds) ||
        trigger.holdoffSeconds < 0 || trigger.filter < 0 || trigger.filter > 1) {
        if (error) *error = QStringLiteral("Invalid trigger channel, level, hysteresis, position or auto timeout");
        return false;
    }
    // Preserve the committed detector on consecutive Scope.single()/config()
    // calls: otherwise fReconfigure=1 restarts analog-in unnecessarily.
    const bool sameTrigger = m_appliedTriggerValid &&
        m_appliedTrigger.mode == trigger.mode &&
        m_appliedTrigger.channel == trigger.channel &&
        m_appliedTrigger.rising == trigger.rising &&
        m_appliedTrigger.levelVolts == trigger.levelVolts &&
        m_appliedTrigger.hysteresisVolts == trigger.hysteresisVolts &&
        m_appliedTrigger.positionPercent == trigger.positionPercent &&
        m_appliedTrigger.autoTimeoutSeconds == trigger.autoTimeoutSeconds &&
        m_appliedTrigger.type == trigger.type && m_appliedTrigger.source == trigger.source &&
        m_appliedTrigger.lengthCondition == trigger.lengthCondition &&
        m_appliedTrigger.lengthSeconds == trigger.lengthSeconds &&
        m_appliedTrigger.holdoffSeconds == trigger.holdoffSeconds &&
        m_appliedTrigger.filter == trigger.filter;
    if (sameTrigger) return true;
    m_appliedTriggerValid = false;
    // Digilent dwf.h: trigsrcNone=0, trigsrcDetectorAnalogIn=2,
    // trigtypeEdge=0, DwfTriggerSlopeRise=0, DwfTriggerSlopeFall=1.
    // Configure the detector BEFORE selecting it as trigger source.
    // DWF's positive position shifts the trigger LEFT of center.
    // A positive half-capture duration puts it at the first sample.
    const double seconds = (0.5 - trigger.positionPercent / 100.0)
        * (static_cast<double>(m_scopeSamples) / m_scopeRateHz);
    const double timeout = trigger.mode == ScopeTriggerMode::Auto
        ? trigger.autoTimeoutSeconds : 0.0;
    // For device-generated or external triggers, the analog detector is not used.
    // Validate the source against the SDK capabilities rather than silently
    // mapping an unsupported trigger to an analog edge.
    const unsigned char source = trigger.mode == ScopeTriggerMode::None ? 0 : trigger.source;
    // Detector analog in (2) is the v4.4/v5.3.3 proven default path.
    // Do not introduce an optional SourceInfo dependency for that path.
    if (source!=0 && source!=2) {
        if (!fScopeTriggerSourceInfo) {
            if(error) *error=QStringLiteral("Non-analog trigger capabilities unavailable in SDK");
            return false;
        }
        int mask=0;
        if (!fScopeTriggerSourceInfo(m_handle,&mask))
            return fail(error,QStringLiteral("Trigger source capability query"));
        if (!(mask & (1<<source))) {
            if (error) *error = QStringLiteral("Trigger source unsupported by device");
            return false;
        }
    }
    if (source==2 && trigger.type!=0) {
        if (!fScopeTriggerTypeInfo || !fTriggerLength || !fTriggerLengthCondition) {
            if (error) *error = QStringLiteral("Advanced trigger unavailable in SDK");
            return false;
        }
        int mask=0;
        if (!fScopeTriggerTypeInfo(m_handle,&mask)) return fail(error,QStringLiteral("Trigger type capabilities"));
        if (!(mask & (1<<trigger.type))) { if(error) *error=QStringLiteral("Trigger type unsupported by device"); return false; }
    }
    if (trigger.holdoffSeconds > 0 && !fTriggerHoldOff) {
        if (error) *error = QStringLiteral("Holdoff unavailable in SDK");
        return false;
    }
    if (trigger.filter != 0 && !fTriggerFilter) {
        if (error) *error = QStringLiteral("Trigger filter unavailable in SDK");
        return false;
    }
    if (!fTriggerChannel(m_handle, trigger.channel) ||
        !fTriggerType(m_handle, trigger.type) ||
        !fTriggerCondition(m_handle, trigger.rising ? 0 : 1) ||
        !fTriggerLevel(m_handle, trigger.levelVolts) ||
        !fTriggerHysteresis(m_handle, trigger.hysteresisVolts) ||
        !fTriggerPosition(m_handle, seconds) ||
        !fTriggerAutoTimeout(m_handle, timeout) ||
        (trigger.type!=0 && source==2 && (!fTriggerLengthCondition(m_handle,trigger.lengthCondition) ||
                                           !fTriggerLength(m_handle,trigger.lengthSeconds))) ||
        (fTriggerHoldOff && !fTriggerHoldOff(m_handle,trigger.holdoffSeconds)) ||
        (fTriggerFilter && !fTriggerFilter(m_handle,trigger.filter)) ||
        !fScopeTrigger(m_handle, source))
        return fail(error, QStringLiteral("Configuring hardware edge trigger"));

    // AutoConfigure is disabled in open(). Every *Set call above only stages
    // the configuration. Commit it BEFORE attempting the SDK *Get readback.
    // fStart=0 keeps acquisition stopped until scopeStart(). Without this,
    // the readback can still report source=trigsrcNone and timeout=0.
    if (!fScopeConfigure(m_handle, 1, 0))
        return fail(error, QStringLiteral("Applying hardware trigger configuration"));
    if (!verifyScopeTrigger(trigger, error)) return false;
    m_appliedTrigger = trigger;
    m_appliedTriggerValid = true;
    ++m_scopeTriggerSetupCount;
    return true;
}

bool DwfRuntime::verifyScopeTrigger(const ScopeTriggerSettings& trigger, QString* error) {
    unsigned char source = 0;
    int condition = -1, channel = -1;
    double timeout = -1;
    if (!fTriggerSourceGet(m_handle, &source) ||
        !fTriggerConditionGet(m_handle, &condition) ||
        !fTriggerChannelGet(m_handle, &channel) ||
        !fTriggerTimeoutGet(m_handle, &timeout))
        return fail(error, QStringLiteral("Reading back hardware trigger settings"));

    const unsigned char expectedSource = trigger.mode == ScopeTriggerMode::None ? 0 : trigger.source;
    const int expectedCondition = trigger.rising ? 0 : 1;
    if (source != expectedSource || (expectedSource == 2 && (channel != trigger.channel ||
        condition != expectedCondition)) ||
        (trigger.mode == ScopeTriggerMode::Normal && timeout > 0.001) ||
        (trigger.mode == ScopeTriggerMode::Auto && timeout <= 0)) {
        if (error) *error = QStringLiteral(
            "Trigger settings mismatch: requested source=%1 CH%2 slope=%3 timeout=%4s; "
            "read source=%5 CH%6 slope=%7 timeout=%8s")
            .arg(int(expectedSource)).arg(trigger.channel+1).arg(expectedCondition)
            .arg(trigger.mode == ScopeTriggerMode::Normal ? 0.0 : trigger.autoTimeoutSeconds)
            .arg(int(source)).arg(channel+1).arg(condition).arg(timeout);
        return false;
    }
    return true;
}

bool DwfRuntime::scopeTriggerDetectorReadback(int* type, double* level, double* hysteresis, QString* error) {
    if (type) *type = -1;
    if (level) *level = 0.0;
    if (hysteresis) *hysteresis = 0.0;
    int actualType = -1;
    double actualLevel = 0.0, actualHysteresis = 0.0;
    if (!connected() || !fTriggerTypeGet(m_handle, &actualType) ||
        !fTriggerLevelGet(m_handle, &actualLevel) ||
        !fTriggerHysteresisGet(m_handle, &actualHysteresis))
        return fail(error, QStringLiteral("Reading trigger detector settings"));
    if (type) *type = actualType;
    if (level) *level = actualLevel;
    if (hysteresis) *hysteresis = actualHysteresis;
    return true;
}

bool DwfRuntime::scopeAutoTriggered(bool* autoTriggered, QString* error) {
    if (autoTriggered) *autoTriggered = false;
    int wasAuto = 0;
    if (!connected() || !fScopeAutoTriggered(m_handle, &wasAuto))
        return fail(error, QStringLiteral("Checking whether acquisition was auto-triggered"));
    if (autoTriggered) *autoTriggered = wasAuto != 0;
    return true;
}

bool DwfRuntime::scopeTriggerPosition(double* configuredSeconds, double* measuredSeconds,
                                       QString* error) {
    if (configuredSeconds) *configuredSeconds = 0.0;
    if (measuredSeconds) *measuredSeconds = 0.0;
    double configured = 0.0, measured = 0.0;
    if (!connected() || !fTriggerPositionGet(m_handle, &configured) ||
        !fTriggerPositionStatus(m_handle, &measured))
        return fail(error, QStringLiteral("Reading analog-in trigger position"));
    if (configuredSeconds) *configuredSeconds = configured;
    if (measuredSeconds) *measuredSeconds = measured;
    return true;
}

bool DwfRuntime::scopeStart(QString* error, const std::atomic_bool* stop) {
    if (!connected() || !m_appliedScope.valid || !m_appliedTriggerValid) {
        if (error) *error = QStringLiteral("Starting oscilloscope requires committed Scope/trigger configuration");
        return false;
    }
    m_scopeLastSettleMs = 0;
    if (m_scopeSettlePending) {
        // The two-second conservative startup interval is measured from the
        // actual front-end commit, NOT from each acquisition. Preserve an
        // application-provided delay rather than waiting two extra seconds.
        QElapsedTimer waited; waited.start();
        while (m_scopeSettleTimer.elapsed() < kAnalogSettleMs) {
            if (stop && stop->load()) {
                if (error) *error = QStringLiteral("Scope settling cancelled");
                return false;
            }
            const qint64 remaining = kAnalogSettleMs - m_scopeSettleTimer.elapsed();
            QThread::msleep(static_cast<unsigned long>(qMin<qint64>(20, qMax<qint64>(1, remaining))));
        }
        if (stop && stop->load()) {
            if (error) *error = QStringLiteral("Scope settling cancelled");
            return false;
        }
        m_scopeLastSettleMs = static_cast<int>(waited.elapsed());
        m_scopeSettlePending = false;
    }
    // fReconfigure=0, fStart=1: arm using existing committed configuration.
    if (m_scopeAcqMode==1 || m_scopeAcqMode==2) m_scanLastIndex=-1;
    if (m_scopeAcqMode==3) m_recordLost=m_recordCorrupt=0;
    if (!fScopeConfigure(m_handle, 0, 1)) return fail(error, QStringLiteral("Starting oscilloscope"));
    ++m_scopeArmCount;
    return true;
}
bool DwfRuntime::scopeStop(QString* error) {
    if (!connected() || !fScopeConfigure(m_handle,0,0))
        return fail(error,QStringLiteral("Stopping oscilloscope"));
    return true;
}
bool DwfRuntime::scopeWait(double seconds, const std::atomic_bool& stop, QString* error) {
    if (!connected()) return fail(error, QStringLiteral("Scope wait (no device)"));
    QElapsedTimer timer; timer.start();
    const qint64 timeoutMs = qint64(qBound(0.01, seconds, 3600.0)*1000);
    while (!stop.load()) {
        unsigned char state = 0;
        if (!fScopeStatus(m_handle, 1, &state)) return fail(error, QStringLiteral("Scope acquisition status"));
        if (state == 2) return true; // DwfStateDone
        if (timer.elapsed() >= timeoutMs) {
            if (error) *error = QStringLiteral("Scope acquisition timed out (no trigger or incomplete capture)");
            return false;
        }
        QThread::msleep(5);
    }
    if (error) *error = QStringLiteral("Script was stopped");
    return false;
}
bool DwfRuntime::scopePoll(bool* completed, QString* error) {
    if (completed) *completed = false;
    unsigned char state = 0;
    if (!connected() || !fScopeStatus(m_handle, 1, &state))
        return fail(error, QStringLiteral("Polling scope acquisition"));
    if (completed) *completed = (state == 2); // DwfStateDone
    return true;
}
bool DwfRuntime::readScope(int channelIndex, int samples, QVector<double>& out, QString* error) {
    if (!connected() || samples < 1 || samples > 1'048'576) return fail(error, QStringLiteral("Reading scope data"));
    out.resize(samples);
    if (!fScopeData(m_handle, channelIndex, out.data(), samples)) return fail(error, QStringLiteral("Scope samples"));
    return true;
}
bool DwfRuntime::scopeReadRecord(int totalSamples, QVector<double>& a, QVector<double>& b,
                                 double timeoutSeconds, const std::atomic_bool& stop, QString* error) {
    m_recordLost=0; m_recordCorrupt=0;
    if (m_scopeAcqMode!=3 || !fScopeRecordStatus || totalSamples<1 || totalSamples>2'000'000) {
        if (error) *error = QStringLiteral("Record must be configured and within 1..2,000,000 samples");
        return false;
    }
    a.resize(totalSamples); b.resize(totalSamples);
    QElapsedTimer timer; timer.start();
    int copied=0;
    while (copied<totalSamples && !stop.load()) {
        unsigned char state=0;
        if (!fScopeStatus(m_handle,1,&state)) return fail(error,QStringLiteral("Record acquisition status"));
        if (state==4 || state==5 || state==1) {
            if (timer.elapsed()>=qint64(timeoutSeconds*1000.0)) {
                if(error) *error=QStringLiteral("Record timed out waiting for acquisition trigger");
                return false;
            }
            QThread::msleep(2); continue;
        }
        int available=0,lost=0,corrupt=0;
        if (!fScopeRecordStatus(m_handle,&available,&lost,&corrupt))
            return fail(error,QStringLiteral("Record FIFO status"));
        if (lost<0 || corrupt<0 || available<0) {
            if (error) *error = QStringLiteral("Invalid SDK record FIFO counters");
            return false;
        }
        m_recordLost+=lost; m_recordCorrupt+=corrupt;
        // Lost samples are explicitly represented by NaN, never fabricated as 0 V.
        const int pad=qMin(lost,totalSamples-copied);
        for (int i=0;i<pad;++i) { a[copied+i]=qQNaN(); b[copied+i]=qQNaN(); }
        copied+=pad;
        const int n=qMin(available,totalSamples-copied);
        if (n>0) {
            if (!fScopeData(m_handle,0,a.data()+copied,n) ||
                !fScopeData(m_handle,1,b.data()+copied,n))
                return fail(error,QStringLiteral("Reading two-channel Record FIFO"));
            copied+=n;
        }
        if (copied>=totalSamples) break;
        if (timer.elapsed() >= qint64(timeoutSeconds*1000.0)) {
            if(error) *error=QStringLiteral("Record timed out: copied %1 of %2 samples, lost %3, corrupt %4")
                .arg(copied).arg(totalSamples).arg(m_recordLost).arg(m_recordCorrupt);
            return false;
        }
        if (state==2 && available==0) {
            if(error) *error=QStringLiteral("Record stopped early: %1 of %2 samples")
                .arg(copied).arg(totalSamples);
            return false;
        }
        QThread::msleep(2);
    }
    if (stop.load()) { if(error) *error=QStringLiteral("Record cancelled"); return false; }
    if (m_recordLost || m_recordCorrupt) {
        if(error) *error=QStringLiteral("Record contains %1 lost and %2 corrupt samples")
            .arg(m_recordLost).arg(m_recordCorrupt);
        return false; // Data retained, but cannot be reported as a valid measurement.
    }
    return true;
}

bool DwfRuntime::scopeReadScan(QVector<double>& a, QVector<double>& b,
                               double timeoutSeconds, const std::atomic_bool& stop, QString* error) {
    if ((m_scopeAcqMode!=1 && m_scopeAcqMode!=2) || !fScopeSamplesValid) {
        if (error) *error = QStringLiteral("Scope is not in a supported scan mode");
        return false;
    }
    QElapsedTimer timer; timer.start();
    int available=0,index=0;
    while (!stop.load()) {
        unsigned char state=0;
        if (!fScopeStatus(m_handle,1,&state) || !fScopeSamplesValid(m_handle,&available))
            return fail(error,QStringLiteral("Scan acquisition status"));
        if (m_scopeAcqMode==2 && (!fScopeIndexWrite || !fScopeIndexWrite(m_handle,&index)))
            return fail(error,QStringLiteral("Screen write index"));
        if (available>=m_scopeSamples && (m_scopeAcqMode==1 || index!=m_scanLastIndex)) break;
        if (timer.elapsed()>=qint64(timeoutSeconds*1000.0)) {
            if (error) *error = QStringLiteral("Scan timeout waiting for full buffer/new write position");
            return false;
        }
        QThread::msleep(3);
    }
    if (stop.load()) { if(error) *error=QStringLiteral("Scan cancelled"); return false; }
    a.resize(m_scopeSamples); b.resize(m_scopeSamples);
    if (m_scopeAcqMode==2) {
        // IndexWrite is the next circular write address (oldest sample).
        // StatusData2 reads chronologically starting at this index.
        if (index<0 || index>=m_scopeSamples) {
            if (error) *error = QStringLiteral("Invalid SDK Screen write index");
            return false;
        }
        const int tail=m_scopeSamples-index;
        for (int ch=0;ch<2;++ch) {
            double* dest=ch==0?a.data():b.data();
            if (!fScopeData2(m_handle,ch,dest,index,tail) ||
                (index>0 && !fScopeData2(m_handle,ch,dest+tail,0,index)))
                return fail(error,QStringLiteral("Reading Screen circular buffer"));
        }
        m_scanLastIndex=index;
    } else if (!fScopeData(m_handle,0,a.data(),m_scopeSamples) ||
               !fScopeData(m_handle,1,b.data(),m_scopeSamples))
        return fail(error,QStringLiteral("Reading Shift FIFO buffer"));
    return true;
}

bool DwfRuntime::setupWavegenSine(int channelIndex, double hz, double amplitude, double offset, QString* error) {
    return setupWavegen(channelIndex, QStringLiteral("Sine"), hz, amplitude, offset,
                        50, 0, {}, 0, 0, 0, -1, error);
}
bool DwfRuntime::setupWavegen(int channelIndex, const QString& type, double hz, double amplitude,
                              double offset, double symmetry, double phase, const QVector<double>& custom,
                              double runSeconds, double waitSeconds, int repeats, int master, QString* error) {
    if (!connected() || channelIndex < 0 || channelIndex > 1 ||
        !qIsFinite(hz) || hz <= 0 || hz > 25e6 ||
        !qIsFinite(amplitude) || amplitude < 0 || amplitude > 5 ||
        !qIsFinite(offset) || qAbs(offset)+amplitude > 5 ||
        !qIsFinite(symmetry) || symmetry <= 0 || symmetry >= 100 ||
        !qIsFinite(phase) || phase < 0 || phase > 360 ||
        !qIsFinite(runSeconds) || runSeconds < 0 || runSeconds > 3600 ||
        !qIsFinite(waitSeconds) || waitSeconds < 0 || waitSeconds > 3600 ||
        repeats < 0 || repeats > 100000 || (master != -1 && master != 0)) {
        if (error) *error = QStringLiteral("Wavegen parameters outside validated safety limits");
        return false;
    }
    const int node = 0; // AnalogOutNodeCarrier, dwf.h
    const QStringList shapes = {"DC","Sine","Square","Triangle","Ramp Up","Ramp Down","Noise", "Custom"};
    int function = shapes.indexOf(type);
    if (function < 0) { if (error) *error = QStringLiteral("Unsupported Wavegen waveform: ")+type; return false; }
    if (function == 7) function = 30; // funcCustom from dwf.h
    if (function == 30) {
        int minimum = 0, maximum = 0;
        if (!fWaveDataInfo(m_handle, channelIndex, node, &minimum, &maximum))
            return fail(error, QStringLiteral("Querying Wavegen custom buffer limits"));
        if (custom.size() < minimum || custom.size() > maximum) {
            if (error) *error = QStringLiteral("Custom data length %1 outside device limits %2..%3")
                .arg(custom.size()).arg(minimum).arg(maximum);
            return false;
        }
    }
    if (!fWaveMaster(m_handle, channelIndex, master) ||
        !fWaveEnable(m_handle, channelIndex, node, 1) ||
        !fWaveFunction(m_handle, channelIndex, node, static_cast<unsigned char>(function)) ||
        !fWaveHz(m_handle, channelIndex, node, hz) ||
        !fWaveAmplitude(m_handle, channelIndex, node, amplitude) ||
        !fWaveOffset(m_handle, channelIndex, node, offset) ||
        !fWaveSymmetry(m_handle, channelIndex, node, symmetry) ||
        !fWavePhase(m_handle, channelIndex, node, phase) ||
        !fWaveRun(m_handle, channelIndex, runSeconds) ||
        !fWaveWait(m_handle, channelIndex, waitSeconds) ||
        !fWaveRepeat(m_handle, channelIndex, repeats))
        return fail(error, QStringLiteral("Configuring waveform generator"));
    if (function == 30 && !fWaveData(m_handle, channelIndex, node,
                                    const_cast<double*>(custom.constData()), custom.size()))
        return fail(error, QStringLiteral("Uploading custom waveform samples"));
    return true;
}
// Release B: hardware carrier/FM/AM are independent DWF nodes.
// SDK node-amplitude semantics are indices in percent (not volts) for FM/AM.
bool DwfRuntime::setupWavegenAdvanced(int channelIndex,const WaveAdvanced& w,QString* error) {
    if(!connected()||channelIndex<0||channelIndex>=2) {
        if (error) *error = "Wavegen unavailable";
        return false;
    }
    const bool advanced=w.fm||w.am||w.triggerSource!=0||w.idle!=1||w.repeatTrigger;
    if(!advanced && !fWaveNodeInfo) return true; // older SDK, legacy Simple API
    int nodes=0;
    if((w.fm||w.am) && (!fWaveNodeInfo || !fWaveNodeInfo(m_handle,channelIndex,&nodes)))
        return fail(error,QStringLiteral("Querying analog output nodes"));
    const auto setupNode=[&](int node,bool enabled,const QString& type,double hz,double depth)->bool {
        if(!enabled) {
            // Do not assume an unsupported node can be disabled.
            return !(nodes & (1<<node)) || fWaveEnable(m_handle,channelIndex,node,0);
        }
        if(!(nodes&(1<<node))) {
            if (error) *error = QStringLiteral("Unsupported Wavegen modulation node %1").arg(node);
            return false;
        }
        const QStringList shapes={"DC","Sine","Square","Triangle","Ramp Up","Ramp Down","Noise"};
        const int fn=shapes.indexOf(type);
        if(fn<0||!fWaveEnable(m_handle,channelIndex,node,1)||
           !fWaveFunction(m_handle,channelIndex,node,static_cast<unsigned char>(fn))||
           !fWaveHz(m_handle,channelIndex,node,hz)||
           !fWaveAmplitude(m_handle,channelIndex,node,depth)||
           !fWaveOffset(m_handle,channelIndex,node,0))
            return fail(error,QStringLiteral("Configuring Wavegen modulation node %1").arg(node));
        return true;
    };
    if(fWaveNodeInfo) {
        if(!fWaveNodeInfo(m_handle,channelIndex,&nodes))return fail(error,"Querying Wavegen node mask");
        if(!setupNode(1,w.fm,w.fmType,w.fmHz,w.fmDepth)||
           !setupNode(2,w.am,w.amType,w.amHz,w.amDepth))return false;
    }
    // Mandatory only when the script requests the corresponding non-default option.
    if(w.triggerSource!=0) {
        int sources=0;
        if(!fWaveTriggerSourceSet||!fWaveTriggerSourceInfo||
           !fWaveTriggerSourceInfo(m_handle,channelIndex,&sources)||
           !(sources & (1<<w.triggerSource))) {
            if (error) *error = "Wavegen trigger source not supported";
            return false;
        }
    }
    if(fWaveTriggerSourceSet && !fWaveTriggerSourceSet(m_handle,channelIndex,
          static_cast<unsigned char>(w.triggerSource)))return fail(error,"Configuring Wavegen trigger source");
    if(w.idle!=1) {
        int modes=0;
        if(!fWaveIdleSet||!fWaveIdleInfo||!fWaveIdleInfo(m_handle,channelIndex,&modes)||
           !(modes&(1<<w.idle))) {
            if (error) *error = "Wavegen idle mode not supported";
            return false;
        }
    }
    if(fWaveIdleSet && !fWaveIdleSet(m_handle,channelIndex,w.idle))
        return fail(error,"Configuring Wavegen idle output");
    if(w.repeatTrigger&&!fWaveRepeatTriggerSet) {
        if (error) *error = "Repeat-trigger API unavailable";
        return false;
    }
    if(fWaveRepeatTriggerSet && !fWaveRepeatTriggerSet(m_handle,channelIndex,w.repeatTrigger?1:0))
        return fail(error,"Configuring Wavegen repeat trigger");
    return true;
}
QVariantMap DwfRuntime::wavegenCapabilities(int channelIndex,QString* error) {
    QVariantMap out;
    if(!connected()||channelIndex<0||channelIndex>1) {
        if (error) *error = "Disconnected or invalid Wavegen channel";
        return out;
    }
    out["channel"]=channelIndex+1;
    int nodes=1, idle=0, triggers=0, carrier=0, fm=0, am=0;
    if(fWaveNodeInfo && !fWaveNodeInfo(m_handle,channelIndex,&nodes))
        {fail(error,"Wavegen node capability query");return {};}
    out["nodeMask"]=nodes;
    if(fWaveFunctionInfo) {
        if(!fWaveFunctionInfo(m_handle,channelIndex,0,&carrier))
            {fail(error,"Carrier function info");return {};}
        if((nodes&2)&&!fWaveFunctionInfo(m_handle,channelIndex,1,&fm))
            {fail(error,"FM function info");return {};}
        if((nodes&4)&&!fWaveFunctionInfo(m_handle,channelIndex,2,&am))
            {fail(error,"AM function info");return {};}
    }
    out["carrierFunctionMask"]=carrier;out["fmFunctionMask"]=fm;out["amFunctionMask"]=am;
    if(fWaveTriggerSourceInfo && !fWaveTriggerSourceInfo(m_handle,channelIndex,&triggers))
        {fail(error,"Wavegen trigger source info");return {};}
    if(fWaveIdleInfo && !fWaveIdleInfo(m_handle,channelIndex,&idle))
        {fail(error,"Wavegen idle mode info");return {};}
    out["triggerSourceMask"]=triggers;out["idleMask"]=idle;
    out["playSupported"]=(nodes&1)&&fWavePlayStatus&&fWavePlayData&&fWaveDataInfo;
    out["repeatTriggerAvailable"]=(fWaveRepeatTriggerSet!=nullptr);
    out["statusAvailable"]=(fWaveStatus!=nullptr);
    int lo=0,hi=0;
    if(fWaveDataInfo && fWaveDataInfo(m_handle,channelIndex,0,&lo,&hi)) {
        out["bufferMinSamples"]=lo;out["bufferMaxSamples"]=hi;
    }
    return out;
}
QVariantMap DwfRuntime::wavegenStatus(int channelIndex,QString* error) {
    QVariantMap out;
    if(!connected()||channelIndex<0||channelIndex>1||!fWaveStatus) {
        if (error) *error = "Wavegen status unavailable";
        return out;
    }
    unsigned char state=0;
    if(!fWaveStatus(m_handle,channelIndex,&state)) {
        fail(error,"Reading Wavegen status");return {};
    }
    out["channel"]=channelIndex+1;out["stateCode"]=int(state);
    // Optional physical-device function readback. FUNC values 0..6 follow
    // dwf.h, 30=Custom, 31=Play. The field is deliberately absent when the
    // installed SDK does not export the getter or the read fails.
    if(fWaveFunctionGet) {
        unsigned char function=255;
        if(fWaveFunctionGet(m_handle,channelIndex,0,&function)) {
            out["carrierFunctionCode"]=int(function);
            const QStringList names={"DC","Sine","Square","Triangle","Ramp Up","Ramp Down","Noise"};
            const QString name = int(function)<names.size()
                ? names.at(int(function)) : function==30 ? QStringLiteral("Custom")
                : function==31 ? QStringLiteral("Play") : QStringLiteral("Unknown");
            out["carrierFunction"]=name;
        }
    }
    // DWF state 0=Ready; remaining run time isn't meaningful in this state.
    // Never expose a negative stale SDK countdown as time remaining.
    const char* stateName = state==0 ? "Ready" : state==1 ? "Armed" :
       state==2 ? "Done" : state==3 ? "Running" : state==4 ? "Config" :
       state==5 ? "Prefill" : state==7 ? "Wait" : "Unknown";
    out["stateName"]=QString::fromLatin1(stateName);
    out["playLostSamples"]=m_playLost[channelIndex];
    out["playCorruptSamples"]=m_playCorrupt[channelIndex];
    double runRemaining=0;
    if(state!=0 && fWaveRunStatus && fWaveRunStatus(m_handle,channelIndex,&runRemaining) &&
        std::isfinite(runRemaining) && runRemaining>=0)
        out["runSecondsRemaining"]=runRemaining;
    return out;
}
// Finite, synchronous streaming API. Always stops output after completion/failure.
// The GUI does not block: this executes in the script worker. Cancellation is polled.
bool DwfRuntime::playWavegen(int channelIndex,double hz,const QVector<double>& samples,
                             double timeoutSeconds,const std::atomic_bool* stop,QString* error) {
    if(!connected()||channelIndex<0||channelIndex>1||
       !fWavePlayStatus||!fWavePlayData||!fWaveDataInfo||!fWaveStatus ||
       !qIsFinite(hz)||hz<1||hz>1e6||samples.size()<16||
       !qIsFinite(timeoutSeconds)||timeoutSeconds<=0||timeoutSeconds>120 ||
       double(samples.size())/hz>timeoutSeconds) {
        if (error) *error = "Invalid/unsupported bounded Wavegen Play request";
        return false;
    }
    int minSamples=0,maxSamples=0;
    if(!fWaveDataInfo(m_handle,channelIndex,0,&minSamples,&maxSamples))
        return fail(error,"Querying Wavegen Play buffer");
    if(maxSamples<minSamples || maxSamples<16 || samples.size()<minSamples) {
        if (error) *error = "Wavegen Play buffer too small";
        return false;
    }
    const int prefill=qMin(maxSamples,samples.size());
    const double runSeconds=double(samples.size())/hz;
    m_playLost[channelIndex]=0;m_playCorrupt[channelIndex]=0;
    int nodes=1;
    if(fWaveNodeInfo && !fWaveNodeInfo(m_handle,channelIndex,&nodes))
        return fail(error,"Querying Play-compatible nodes");
    if(!(nodes&1)) {if(error)*error="Carrier node not supported";return false;}
    // Reset stale trigger/master settings when switching from triggered bursts.
    if((nodes&2) && !fWaveEnable(m_handle,channelIndex,1,0))
        return fail(error,"Disabling FM for Play");
    if((nodes&4) && !fWaveEnable(m_handle,channelIndex,2,0))
        return fail(error,"Disabling AM for Play");
    if((fWaveTriggerSourceSet && !fWaveTriggerSourceSet(m_handle,channelIndex,0)) ||
       (fWaveRepeatTriggerSet && !fWaveRepeatTriggerSet(m_handle,channelIndex,0)) ||
       !fWaveMaster(m_handle,channelIndex,-1))
        return fail(error,"Resetting Play trigger/master state");
    if(!fWaveEnable(m_handle,channelIndex,0,1)||
       !fWaveFunction(m_handle,channelIndex,0,31)||
       !fWaveHz(m_handle,channelIndex,0,hz)||
       !fWaveAmplitude(m_handle,channelIndex,0,1)||
       !fWaveOffset(m_handle,channelIndex,0,0)||
       !fWaveRun(m_handle,channelIndex,runSeconds)||
       !fWaveWait(m_handle,channelIndex,0)||
       !fWaveRepeat(m_handle,channelIndex,1)||
       !fWaveData(m_handle,channelIndex,0,const_cast<double*>(samples.constData()),prefill)||
       !fWaveConfigure(m_handle,channelIndex,1))
        return fail(error,"Starting finite Wavegen Play");
    QElapsedTimer timer;timer.start();int sent=prefill;
    bool completed=false;QString problem;
    while(!completed) {
        if(stop && stop->load()) {problem="Wavegen Play cancelled";break;}
        if(timer.elapsed()>qint64(timeoutSeconds*1000)) {problem="Wavegen Play timed out";break;}
        unsigned char state=0;
        if(!fWaveStatus(m_handle,channelIndex,&state)) {problem="Wavegen Play state query failed: "+lastError();break;}
        int free=0,lost=0,corrupt=0;
        if(!fWavePlayStatus(m_handle,channelIndex,0,&free,&lost,&corrupt)) {
            problem="Wavegen Play FIFO query failed: "+lastError();break;
        }
        m_playLost[channelIndex]+=lost;m_playCorrupt[channelIndex]+=corrupt;
        if(lost>0||corrupt>0) {problem=QStringLiteral("Wavegen Play underrun: lost=%1 corrupt=%2").arg(m_playLost[channelIndex]).arg(m_playCorrupt[channelIndex]);break;}
        if(sent<samples.size() && free>0) {
            const int n=qMin(free,samples.size()-sent);
            if(!fWavePlayData(m_handle,channelIndex,0,const_cast<double*>(samples.constData()+sent),n)) {
                problem="Wavegen Play sending data failed: "+lastError();break;
            }
            sent+=n;
        }
        if(state==2) {completed=(sent==samples.size()); if(!completed)problem="Wavegen Play ended before all samples were sent";break;}
        QThread::msleep(1);
    }
    if(!fWaveConfigure(m_handle,channelIndex,0) && problem.isEmpty())
        problem="Stopping Wavegen Play failed: "+lastError();
    if(!completed||!problem.isEmpty()) {if(error)*error=problem;return false;}
    return true;
}

bool DwfRuntime::prepareWavegenSlave(int channelIndex, QString* error) {
    // Digilent's AnalogOut_Phase.py SDK example commits CH2 with
    // FDwfAnalogOutConfigure(hdwf, 1, 0) before starting CH1, which
    // owns the synchronized timing for Master=0. Merely staging the
    // slave's parameters with AutoConfigure disabled is insufficient.
    if (!connected() || channelIndex != 1 ||
        !fWaveConfigure(m_handle, channelIndex, 0))
        return fail(error, QStringLiteral("Preparing synchronized Wavegen slave"));
    return true;
}
bool DwfRuntime::startWavegen(int channelIndex, QString* error) {
    if (!connected() || !fWaveConfigure(m_handle,channelIndex,1)) return fail(error, QStringLiteral("Starting waveform generator"));
    return true;
}
bool DwfRuntime::stopWavegen(int channelIndex, QString* error) {
    if (!connected() || !fWaveConfigure(m_handle,channelIndex,0)) return fail(error, QStringLiteral("Stopping waveform generator"));
    return true;
}
bool DwfRuntime::staticIOCapabilities(quint32* outputMask, quint32* inputMask, QString* error) {
    if (!connected() || !fDigitalIOOutputInfo || !fDigitalIOInputInfo) {
        if (error) *error=QStringLiteral("Static Digital I/O SDK capability functions are unavailable");
        return false;
    }
    unsigned int outs=0, ins=0;
    if (!fDigitalIOOutputInfo(m_handle,&outs) || !fDigitalIOInputInfo(m_handle,&ins))
        return fail(error,QStringLiteral("Querying StaticIO pin capabilities"));
    if(outputMask) *outputMask=quint32(outs)&0xffffu;
    if(inputMask) *inputMask=quint32(ins)&0xffffu;
    return true;
}

bool DwfRuntime::staticIORead(quint32* inputMask, QString* error) {
    if (!connected() || !fDigitalIOStatus || !fDigitalIOInputStatus) {
        if (error) *error=QStringLiteral("Static Digital I/O SDK read functions are unavailable");
        return false;
    }
    unsigned int input=0;
    // DigitalIOStatus is the SDK snapshot operation. InputStatus alone can
    // return a stale value from the previous snapshot.
    if (!fDigitalIOStatus(m_handle) || !fDigitalIOInputStatus(m_handle,&input))
        return fail(error,QStringLiteral("Reading StaticIO input levels"));
    if (inputMask) *inputMask=quint32(input)&0xffffu;
    return true;
}

QVariantMap DwfRuntime::patternHardwareStatus() const {
    QVariantMap m{{QStringLiteral("sdkStatusAvailable"),false},
                  {QStringLiteral("sdkStateCode"),-1},
                  {QStringLiteral("sdkStateName"),QStringLiteral("Unavailable")},
                  {QStringLiteral("sdkRunning"),QVariant()},
                  {QStringLiteral("sdkRepeatCount"),QVariant()},
                  {QStringLiteral("sdkRepeatAvailable"),false}};
    if(!connected())return m;
    if(fPatternStatus) {
        unsigned char state=0;
        if(fPatternStatus(m_handle,&state)) {
            QString name;
            switch(state) {
                case 0:name=QStringLiteral("Ready");break;
                case 1:name=QStringLiteral("Armed");break;
                case 2:name=QStringLiteral("Done");break;
                case 3:name=QStringLiteral("Running");break;
                case 4:name=QStringLiteral("Config");break;
                case 5:name=QStringLiteral("Prefill");break;
                case 7:name=QStringLiteral("Wait");break;
                default:name=QStringLiteral("Unknown");break;
            }
            m[QStringLiteral("sdkStatusAvailable")]=true;
            m[QStringLiteral("sdkStateCode")]=int(state);
            m[QStringLiteral("sdkStateName")]=name;
            m[QStringLiteral("sdkRunning")]=(state==3);
        } else m[QStringLiteral("sdkError")]=QStringLiteral("FDwfDigitalOutStatus: ")+lastError();
    }
    if(fPatternRepeatGet) {
        unsigned int repeat=0;
        if(fPatternRepeatGet(m_handle,&repeat)) {
            m[QStringLiteral("sdkRepeatAvailable")]=true;
            m[QStringLiteral("sdkRepeatCount")]=qulonglong(repeat);
        } else m[QStringLiteral("sdkRepeatError")]=QStringLiteral("FDwfDigitalOutRepeatGet: ")+lastError();
    }
    return m;
}

bool DwfRuntime::patternStop(QString* error) {
    if(!m_patternTouched)return true;
    if(!connected() || !fPatternConfigure || !fPatternEnable || m_patternPinMask==0) {
        if(error)*error=QStringLiteral("DigitalOut stop functions unavailable; output state unknown");
        return false;
    }
    // Stop the common DigitalOut engine before releasing every pin WFScript
    // owns. Never claim STOP succeeded when even one channel could remain driven.
    const bool stopped=!!fPatternConfigure(m_handle,0);
    bool disabled=true;
    for(int pin=0;pin<16;++pin) {
        if(m_patternPinMask & (quint32(1)<<pin)) {
            if(!fPatternEnable(m_handle,pin,0)) disabled=false;
        }
    }
    const bool committed=!!fPatternConfigure(m_handle,0);
    if(!stopped || !disabled || !committed) {
        if(error)*error=QStringLiteral("DigitalOut stop/driver-disable not fully confirmed: ")+lastError();
        return false;
    }
    m_patternRunning=false;
    m_patternPin=-1;
    m_patternPinMask=0;
    m_patternTouched=false;
    return true;
}

bool DwfRuntime::patternClockConfigure(int pin,double hz,double dutyPercent,bool start,
                                       double* actualHz,double* actualDuty,double* clockHz,QString* error) {
    if(!connected() || !fPatternConfigure || !fPatternClockInfo || !fPatternCount ||
       !fPatternEnable || !fPatternOutput || !fPatternIdle || !fPatternType ||
       !fPatternDivider || !fPatternCounter || !fPatternCounterInit ||
       !fPatternCounterInfo || !fPatternDividerInfo || !fPatternRunTime ||
       !fPatternTrigger) {
        if(error)*error=QStringLiteral("Required DigitalOut clock/PWM SDK functions unavailable");
        return false;
    }
    if(!qIsFinite(hz) || hz<1 || hz>1e6 || !qIsFinite(dutyPercent) ||
       dutyPercent<1 || dutyPercent>99 || pin<0 || pin>15) {
        if(error)*error=QStringLiteral("DigitalOut: invalid pin, Hz (1..1M), or duty (1..99%)");
        return false;
    }
    if(m_patternTouched && !patternStop(error))return false;
    int channelCount=0;
    double baseClock=0;
    if(!fPatternCount(m_handle,&channelCount) || !fPatternClockInfo(m_handle,&baseClock))
        return fail(error,QStringLiteral("Querying DigitalOut capabilities"));
    if(pin>=channelCount || !qIsFinite(baseClock) || baseClock<=0) {
        if(error)*error=QStringLiteral("DigitalOut pin unavailable or invalid internal clock");
        return false;
    }
    // Static I/O output drivers must be released before the pattern starts.
    // Do not silently take ownership of pins controlled by another instrument.
    if(fDigitalIOOutputEnableGet) {
        unsigned int en=0;
        if(!fDigitalIOOutputEnableGet(m_handle,&en))
            return fail(error,QStringLiteral("Checking StaticIO output ownership"));
        if(en!=0) {
            if(error)*error=QStringLiteral("Release StaticIO outputs (StaticIO.stop()) before starting Patterns");
            return false;
        }
    }
    unsigned int cmin=0,cmax=0,dmin=0,dmax=0;
    if(!fPatternCounterInfo(m_handle,pin,&cmin,&cmax) ||
       !fPatternDividerInfo(m_handle,pin,&dmin,&dmax))
        return fail(error,QStringLiteral("Querying DigitalOut divider/counter limits"));
    if(cmax<2 || dmax<1) {
        if(error)*error=QStringLiteral("DigitalOut counter/divider limits unsupported");
        return false;
    }
    const double totalTicks=baseClock/hz;
    const double shareHigh=dutyPercent/100.0;
    // Find a divider that fits *both* high and low counts. Integer division
    // and rounding are explicit; expose achieved frequency/duty to the script.
    quint64 div=qMax<quint64>(qMax(1u,dmin),quint64(std::ceil(
        qMax(totalTicks*shareHigh,totalTicks*(1.0-shareHigh))/double(cmax))));
    unsigned int low=0,high=0;
    bool found=false;
    for(int i=0;i<1000 && div<=dmax;++i,++div) {
        const auto ticks=qRound64(totalTicks/double(div));
        const auto h=qRound64(double(ticks)*shareHigh);
        const auto l=ticks-h;
        const qint64 minCount = static_cast<qint64>(qMax(1u, cmin));
        const qint64 maxCount = static_cast<qint64>(cmax);
        if (h >= minCount && l >= minCount &&
            h <= maxCount && l <= maxCount) {
            high=unsigned(h); low=unsigned(l);found=true; break;
        }
    }
    if(!found) {
        if(error)*error=QStringLiteral("Cannot realize requested DigitalOut frequency/duty with SDK counters");
        return false;
    }
    // Native DwfDigitalOutTypePulse=0, OutputPushPull=0, IdleZet=3.
    // Trigger source None=0. Run time 0 is continuous generation.
    // Preserve other instrument pins; this revision owns only the selected pin.
    // From here on an SDK command may have modified channel state.
    // Keep the ownership marker so emergency stop can always be attempted.
    m_patternTouched=true;
    m_patternPin=pin;
    m_patternPinMask=quint32(1)<<pin;
    if(!fPatternEnable(m_handle,pin,0) ||
       !fPatternOutput(m_handle,pin,0) || !fPatternIdle(m_handle,pin,3) ||
       !fPatternType(m_handle,pin,0) ||
       !fPatternDivider(m_handle,pin,unsigned(div)) ||
       !fPatternCounterInit(m_handle,pin,0,1u) ||
       !fPatternCounter(m_handle,pin,low,high) ||
       !fPatternTrigger(m_handle,0) || !fPatternRunTime(m_handle,0.0)) {
        return fail(error,QStringLiteral("Programming DigitalOut clock/PWM"));
    }
    m_patternTouched=true;
    m_patternPin=pin;
    m_patternPinMask=quint32(1)<<pin;
    if(!fPatternEnable(m_handle,pin,1) || !fPatternConfigure(m_handle,start?1:0))
        return fail(error,QStringLiteral("Committing DigitalOut clock/PWM"));
    m_patternRunning=start;
    if(actualHz)*actualHz=baseClock/(double(div)*(double(high)+double(low)));
    if(actualDuty)*actualDuty=100.0*double(high)/double(high+low);
    if(clockHz)*clockHz=baseClock;
    return true;
}


// DigitalOut custom mode = 1; push-pull = 0; idle high-Z = 3.
// Unlike launching independent Pulse counters, one custom revolution-long
// buffer per pin makes A/B/index share *one* hardware timebase and restart.
bool DwfRuntime::patternEncoderConfigure(int aPin,int bPin,int indexPin,int ppr,
                                         double rpm,bool reverse,bool start,
                                         double* actualRpm,double* clockHz,QString* error) {
    if(!connected() || !fPatternConfigure || !fPatternClockInfo || !fPatternCount ||
       !fPatternEnable || !fPatternOutput || !fPatternIdle || !fPatternType ||
       !fPatternDivider || !fPatternDividerInit || !fPatternDividerInfo ||
       !fPatternDataInfo || !fPatternDataSet || !fPatternRunTime ||
       !fPatternTrigger) {
        if(error)*error=QStringLiteral("DigitalOut Custom Pattern SDK functions are unavailable");
        return false;
    }
    if(aPin<0 || bPin<0 || indexPin<0 || aPin>15 || bPin>15 || indexPin>15 ||
       aPin==bPin || aPin==indexPin || bPin==indexPin || ppr<1 || ppr>1024 ||
       !qIsFinite(rpm) || rpm<1 || rpm>10000) {
        if(error)*error=QStringLiteral("Encoder: use distinct DIO pins 0..15, 1..1024 PPR and 1..10000 RPM");
        return false;
    }
    if(m_patternTouched && !patternStop(error))return false;
    int count=0; double clock=0;
    if(!fPatternCount(m_handle,&count) || !fPatternClockInfo(m_handle,&clock))
        return fail(error,QStringLiteral("Querying DigitalOut count/clock"));
    if(qMax(aPin,qMax(bPin,indexPin))>=count || !qIsFinite(clock) || clock<=0) {
        if(error)*error=QStringLiteral("Encoder output pins or DigitalOut clock unavailable");
        return false;
    }
    // Guard cross-instrument ownership BEFORE modifying any DigitalOut pins.
    if(fDigitalIOOutputEnableGet) {
        unsigned int enable=0;
        if(!fDigitalIOOutputEnableGet(m_handle,&enable))
            return fail(error,QStringLiteral("Checking StaticIO ownership"));
        if(enable!=0) {
            if(error)*error=QStringLiteral("Release StaticIO outputs (StaticIO.stop()) before starting the encoder");
            return false;
        }
    }
    const unsigned int quarterStates=unsigned(4*ppr);
    const int pins[3]={aPin,bPin,indexPin};
    unsigned int divMin=1,divMax=0xffffffffu;
    unsigned int minDataBits=0xffffffffu;
    for(int pin:pins) {
        unsigned int minimum=0,maximum=0,maxBits=0;
        if(!fPatternDividerInfo(m_handle,pin,&minimum,&maximum) ||
           !fPatternDataInfo(m_handle,pin,&maxBits))
            return fail(error,QStringLiteral("Querying encoder DigitalOut capacity"));
        minDataBits=qMin(minDataBits,maxBits);
        divMin=qMax(divMin,qMax(1u,minimum)); divMax=qMin(divMax,maximum);
    }
    if(divMax<divMin || minDataBits<quarterStates) {
        if(error)*error=QStringLiteral("Encoder: %1 PPR needs at least %2 custom bits per pin (available %3)")
            .arg(ppr).arg(quarterStates).arg(minDataBits);
        return false;
    }
    const double quarterHz=(rpm/60.0)*double(quarterStates);
    const double idealDivider=clock/quarterHz;
    if(!qIsFinite(idealDivider) || idealDivider<=0) {
        if(error)*error=QStringLiteral("Encoder requested rate exceeds native DigitalOut timing limits");
        return false;
    }
    // Some devices limit the DigitalOut divider. For low RPM, repeat each
    // quarter-state N times in the custom buffer so the divider fits, while
    // keeping the three channels exactly phase coherent and the index width
    // equal to one true quadrature state. Check BOTH memory and divider caps.
    const unsigned int maxRepeat=minDataBits/quarterStates;
    quint64 divider=0;
    unsigned int repeat=0;
    const double minimumRepeat=idealDivider/(double(divMax)+0.5);
    const quint64 firstRepeat=qMax<quint64>(1,quint64(std::ceil(minimumRepeat)));
    for(quint64 n=firstRepeat;n<=maxRepeat;++n) {
        const double candidate=idealDivider/double(n);
        if(candidate<double(divMin)-0.5)break;
        const auto d=quint64(std::llround(candidate));
        if(d>=divMin && d<=divMax) {repeat=unsigned(n);divider=d;break;}
    }
    if(repeat==0) {
        if(error)*error=QStringLiteral("Encoder: requested %1 PPR / %2 RPM exceeds custom buffer or divider capacity")
            .arg(ppr).arg(rpm,0,'f',3);
        return false;
    }
    const unsigned int bitCount=quarterStates*repeat;
    // At every quadrature state the signals are Gray-coded:
    // forward: 00, 10, 11, 01; reverse: 00, 01, 11, 10.
    // Index is one quarter-state HIGH at the start of each revolution.
    const int forwardA[4]={0,1,1,0},forwardB[4]={0,0,1,1};
    const int reverseA[4]={0,0,1,1},reverseB[4]={0,1,1,0};
    const int* aStates=reverse?reverseA:forwardA;
    const int* bStates=reverse?reverseB:forwardB;
    QVector<quint8> bits[3];
    for(auto& buf:bits) buf.fill(0,int((bitCount+7u)/8u));
    for(unsigned int n=0;n<bitCount;++n) {
        const unsigned int quarter=n/repeat;
        const int state=int(quarter%4);
        const bool values[3]={bool(aStates[state]),bool(bStates[state]),quarter==0};
        for(int c=0;c<3;++c) if(values[c])
            bits[c][int(n/8)] |= quint8(1u<<(n%8));
    }
    // Record ALL owned pins before SDK mutation, including partial-failure
    // paths, so the bridge can attempt to disable each one during cleanup.
    m_patternTouched=true;
    m_patternRunning=false;
    m_patternPin=aPin;
    m_patternPinMask=(quint32(1)<<aPin)|(quint32(1)<<bPin)|(quint32(1)<<indexPin);
    // Disable the drivers while programming; a single final Configure starts
    // all three channels together. No loop makes separate start commands.
    for(int pin:pins) if(!fPatternEnable(m_handle,pin,0))
        return fail(error,QStringLiteral("Disabling encoder outputs before configuration"));
    for(int i=0;i<3;++i) {
        const int pin=pins[i];
        if(!fPatternOutput(m_handle,pin,0) || !fPatternIdle(m_handle,pin,3) ||
           !fPatternType(m_handle,pin,1) || !fPatternDivider(m_handle,pin,unsigned(divider)) ||
           !fPatternDividerInit(m_handle,pin,unsigned(divider)) ||
           !fPatternDataSet(m_handle,pin,bits[i].constData(),bitCount))
            return fail(error,QStringLiteral("Programming encoder custom data on DIO%1").arg(pin));
    }
    // Run=0 means a continuous hardware run and was already used by the
    // physically tested v5.6.49.1 encoder. Repeat=0 is documented as infinite,
    // but on at least one AD3 SDK/firmware combination RepeatGet did not
    // confirm it. A repeat readback is NOT proof that DigitalOut is stopped.
    // Do not let this advisory feature prevent the known-good run=0 path.
    if(!fPatternTrigger(m_handle,0) || !fPatternRunTime(m_handle,0.0))
        return fail(error,QStringLiteral("Setting continuous encoder run mode"));
    if(fPatternRepeatSet) {
        // Best-effort extra request. Firmware may reject or normalize it.
        // The actual post-start generator state and a complete Logic1 capture
        // decide whether the output is continuous, not this optional readback.
        (void)fPatternRepeatSet(m_handle,0u);
    }
    for(int pin:pins) if(!fPatternEnable(m_handle,pin,1))
        return fail(error,QStringLiteral("Enabling encoder DIO%1").arg(pin));
    if(!fPatternConfigure(m_handle,start?1:0))
        return fail(error,QStringLiteral("Starting three-channel encoder DigitalOut"));
    m_patternRunning=start;
    if(actualRpm)*actualRpm=60.0*clock/(double(divider)*double(bitCount));
    if(clockHz)*clockHz=clock;
    return true;
}


// v5.6.50: General-purpose synchronized hardware bit sequences (not a
// native WaveForms scripting API). A character is ONE logical bit period;
// repeatFactor stretches each bit in the custom buffer when the AD3 divider
// alone cannot achieve a low requested rate. This is a shared hardware start.
bool DwfRuntime::patternSequenceConfigure(const QVector<int>& pins,
        const QStringList& bitStrings, double bitRateHz, bool start,
        double* actualRateHz, double* clockHz, int* repeatFactor, QString* error) {
    if(!connected() || !fPatternConfigure || !fPatternClockInfo || !fPatternCount ||
       !fPatternEnable || !fPatternOutput || !fPatternIdle || !fPatternType ||
       !fPatternDivider || !fPatternDividerInit || !fPatternDividerInfo ||
       !fPatternDataInfo || !fPatternDataSet || !fPatternRunTime || !fPatternTrigger) {
        if(error)*error=QStringLiteral("DigitalOut Custom Pattern SDK functions unavailable");
        return false;
    }
    const qsizetype channels=pins.size();
    if(channels<1 || channels>3 || bitStrings.size()!=channels ||
       !qIsFinite(bitRateHz) || bitRateHz<10 || bitRateHz>1000000) {
        if(error)*error=QStringLiteral("Sequences require 1..3 pin/bit-string pairs, 10..1,000,000 bits/s");
        return false;
    }
    const qsizetype length=bitStrings.first().size();
    if(length<2 || length>1024) {
        if(error)*error=QStringLiteral("Each repeating bit string must contain 2..1024 bits");
        return false;
    }
    quint32 ownedMask=0;
    for(qsizetype i=0;i<channels;++i) {
        const int pin=pins.at(i);
        if(pin<0 || pin>15 || (ownedMask&(quint32(1)<<pin))) {
            if(error)*error=QStringLiteral("Sequence output DIO pins must be unique integers 0..15");
            return false;
        }
        ownedMask|=(quint32(1)<<pin);
        const QString& bits=bitStrings.at(i);
        if(bits.size()!=length) {
            if(error)*error=QStringLiteral("All sequence bit strings must have identical lengths");
            return false;
        }
        for(const QChar c:bits) if(c!=QLatin1Char('0') && c!=QLatin1Char('1')) {
            if(error)*error=QStringLiteral("Sequence data permits only 0 and 1 (no spaces)");
            return false;
        }
    }
    if(m_patternTouched && !patternStop(error)) return false;
    int channelCount=0; double baseClock=0;
    if(!fPatternCount(m_handle,&channelCount) || !fPatternClockInfo(m_handle,&baseClock))
        return fail(error,QStringLiteral("Querying sequence DigitalOut count/clock"));
    if(!qIsFinite(baseClock) || baseClock<=0) {
        if(error)*error=QStringLiteral("Invalid DigitalOut clock");
        return false;
    }
    for(int pin:pins) if(pin>=channelCount) {
        if(error)*error=QStringLiteral("DigitalOut pin DIO%1 is not supported by this device").arg(pin);
        return false;
    }
    // Conservative: existing StaticIO drivers must be released first.
    if(fDigitalIOOutputEnableGet) {
        unsigned int enabled=0;
        if(!fDigitalIOOutputEnableGet(m_handle,&enabled))
            return fail(error,QStringLiteral("Checking StaticIO ownership before sequence"));
        if(enabled!=0) {
            if(error)*error=QStringLiteral("Release StaticIO outputs before starting a sequence");
            return false;
        }
    }
    unsigned int minDivider=1,maxDivider=0xffffffffu,maxBits=0xffffffffu;
    for(int pin:pins) {
        unsigned int dmin=0,dmax=0,capacity=0;
        if(!fPatternDividerInfo(m_handle,pin,&dmin,&dmax) ||
           !fPatternDataInfo(m_handle,pin,&capacity))
            return fail(error,QStringLiteral("Querying DigitalOut custom sequence capacity"));
        minDivider=qMax(minDivider,qMax(1u,dmin));
        maxDivider=qMin(maxDivider,dmax);
        maxBits=qMin(maxBits,capacity);
    }
    if(maxDivider<minDivider || maxBits<static_cast<unsigned int>(length)) {
        if(error)*error=QStringLiteral("The device cannot hold %1 sequence bits/channel (capacity %2)")
            .arg(length).arg(maxBits);
        return false;
    }
    const double idealDivider=baseClock/bitRateHz;
    const quint64 maxRepeat=quint64(maxBits)/quint64(length);
    const quint64 firstRepeat=qMax<quint64>(1,quint64(std::ceil(idealDivider/(double(maxDivider)+0.5))));
    quint64 divider=0,repeat=0;
    for(quint64 n=firstRepeat;n<=maxRepeat;++n) {
        const double ideal=idealDivider/double(n);
        if(ideal<double(minDivider)-0.5)break;
        const quint64 candidate=quint64(std::llround(ideal));
        if(candidate>=minDivider && candidate<=maxDivider) {repeat=n;divider=candidate;break;}
    }
    if(!repeat || !divider) {
        if(error)*error=QStringLiteral("Sequence bit rate %1 Hz exceeds DigitalOut divider/buffer capability")
            .arg(bitRateHz,0,'f',2);
        return false;
    }
    const unsigned int nativeLength=unsigned(quint64(length)*repeat);
    QVector<QVector<quint8>> data;
    data.reserve(channels);
    for(qsizetype c=0;c<channels;++c) {
        QVector<quint8> packed(int((nativeLength+7u)/8u),0);
        const QString& pattern=bitStrings.at(c);
        for(unsigned int j=0;j<nativeLength;++j)
            if(pattern.at(int(j/repeat))==QLatin1Char('1'))
                packed[int(j/8)]|=quint8(1u<<(j%8));
        data.append(std::move(packed));
    }
    m_patternTouched=true;
    m_patternRunning=false;
    m_patternPin=pins.first();
    m_patternPinMask=ownedMask; // preserve all partially configured pins for cleanup
    for(int pin:pins) if(!fPatternEnable(m_handle,pin,0))
        return fail(error,QStringLiteral("Disabling sequence outputs"));
    for(qsizetype i=0;i<channels;++i) {
        const int pin=pins.at(i);
        if(!fPatternOutput(m_handle,pin,0) || !fPatternIdle(m_handle,pin,3) ||
           !fPatternType(m_handle,pin,1) ||
           !fPatternDivider(m_handle,pin,unsigned(divider)) ||
           !fPatternDividerInit(m_handle,pin,unsigned(divider)) ||
           !fPatternDataSet(m_handle,pin,data.at(i).constData(),nativeLength))
            return fail(error,QStringLiteral("Programming custom sequence on DIO%1").arg(pin));
    }
    // Do not treat RepeatGet readback as a startup gate. Real AD3 readback
    // can differ from zero even while continuous Run=0 is working correctly.
    if(!fPatternTrigger(m_handle,0) || !fPatternRunTime(m_handle,0.0))
        return fail(error,QStringLiteral("Setting continuous sequence mode"));
    if(fPatternRepeatSet)(void)fPatternRepeatSet(m_handle,0u);
    for(int pin:pins) if(!fPatternEnable(m_handle,pin,1))
        return fail(error,QStringLiteral("Enabling custom DIO%1").arg(pin));
    if(!fPatternConfigure(m_handle,start?1:0))
        return fail(error,QStringLiteral("Starting synchronized DigitalOut sequences"));
    m_patternRunning=start;
    if(actualRateHz)*actualRateHz=baseClock/(double(divider)*double(repeat));
    if(clockHz)*clockHz=baseClock;
    if(repeatFactor)*repeatFactor=int(repeat);
    return true;
}

bool DwfRuntime::staticIOStop(QString* error) {
    if(m_patternRunning) {
        if(error)*error=QStringLiteral("Stop Patterns before changing StaticIO output ownership");
        return false;
    }

    if (!connected() || !fDigitalIOOutputEnableSet || !fDigitalIOConfigure) {
        if(error) *error=QStringLiteral("Static Digital I/O SDK stop functions are unavailable");
        return false;
    }
    // Never reset DigitalIn or DigitalOut here; simply release StaticIO pins.
    if (!fDigitalIOOutputEnableSet(m_handle,0u) || !fDigitalIOConfigure(m_handle))
        return fail(error,QStringLiteral("Releasing StaticIO outputs to high impedance"));
    return true;
}

bool DwfRuntime::staticIOApply(quint32 enableMask, quint32 outputMask, QString* error) {
    if(m_patternRunning) {
        if(error)*error=QStringLiteral("Patterns currently owns a digital output; stop Patterns before StaticIO changes");
        return false;
    }

    if (!connected() || !fDigitalIOConfigure || !fDigitalIOOutputEnableSet ||
        !fDigitalIOOutputSet || !fDigitalIOOutputEnableGet) {
        if(error) *error=QStringLiteral("Static Digital I/O SDK output functions are unavailable");
        return false;
    }
    quint32 allowed=0, readable=0;
    if (!staticIOCapabilities(&allowed,&readable,error)) return false;
    if ((enableMask & ~allowed)!=0 || (enableMask & ~0xffffu)!=0 || (outputMask & ~0xffffu)!=0) {
        if(error) *error=QStringLiteral("Requested StaticIO output mask contains unsupported pins");
        return false;
    }
    unsigned int previous=0;
    if (!fDigitalIOOutputEnableGet(m_handle,&previous))
        return fail(error,QStringLiteral("Reading StaticIO output direction"));
    // First release outputs moving to Z, then preload new values while new
    // output drivers remain disabled, then enable the requested drivers.
    // These explicit commits also work when SDK AutoConfigure is disabled.
    const unsigned int retained=(previous & enableMask) & 0xffffu;
    if (retained != (previous & 0xffffu)) {
        if (!fDigitalIOOutputEnableSet(m_handle,retained) || !fDigitalIOConfigure(m_handle))
            return fail(error,QStringLiteral("Releasing StaticIO input pins"));
    }
    if (!fDigitalIOOutputSet(m_handle,unsigned(outputMask)) || !fDigitalIOConfigure(m_handle))
        return fail(error,QStringLiteral("Preloading StaticIO output levels"));
    if (!fDigitalIOOutputEnableSet(m_handle,unsigned(enableMask)) || !fDigitalIOConfigure(m_handle))
        return fail(error,QStringLiteral("Enabling StaticIO outputs"));
    return true;
}

bool DwfRuntime::setupLogic(int samples, double requestedRateHz, double* effectiveRateHz, QString* error) {
    m_logicLastValidSamples=-1;
    m_logicLastStateCode=-1;
    m_logicLastBytesRequested=0;
    if (!connected() || samples < 16 || samples > 1'048'576 ||
        !qIsFinite(requestedRateHz) || requestedRateHz < 1 || !effectiveRateHz) {
        if (error) *error=QStringLiteral("Invalid DigitalIn sample count, rate, or disconnected device");
        return false;
    }
    // Do not invent an AD3 sampling clock: query the active instrument.
    if (!fLogicClockInfo) {
        if (error) *error=QStringLiteral("SDK has no DigitalInInternalClockInfo export");
        return false;
    }
    double clockHz=0;
    if (!fLogicClockInfo(m_handle,&clockHz)) return fail(error,QStringLiteral("Querying DigitalIn clock"));
    if (!qIsFinite(clockHz) || clockHz<=0 || requestedRateHz>clockHz) {
        if (error) *error=QStringLiteral("Requested DigitalIn rate exceeds device clock or clock invalid");
        return false;
    }
    if (fLogicSizeInfo) {
        int maximum=0;
        if (!fLogicSizeInfo(m_handle,&maximum)) return fail(error,QStringLiteral("Querying DigitalIn maximum buffer"));
        if (maximum<samples) {
            if (error) *error=QStringLiteral("Requested DigitalIn %1 samples exceeds device maximum %2").arg(samples).arg(maximum);
            return false;
        }
    }
    // Dividers are positive integers. Round to the closest available rate.
    const double rawDivider=clockHz/requestedRateHz;
    if (rawDivider>double(0xffffffffu)) {
        if (error) *error=QStringLiteral("Requested DigitalIn rate is below divider range");
        return false;
    }
    const unsigned int divider=qMax(1u,static_cast<unsigned int>(qRound64(rawDivider)));
    // fLogicDivider signature is (HDWF,unsigned int), same ABI as int on Win64.
    if (!fLogicFormat(m_handle,16) || !fLogicSize(m_handle,samples) ||
        !fLogicDivider(m_handle,static_cast<int>(divider)))
        return fail(error,QStringLiteral("Setting DigitalIn buffer and divider"));
    // trigsrcNone=0 per dwf.h. Configure an immediate, untriggered acquisition;
    // the v5.6.47 Page generates the test edges after arming.
    if (fLogicTriggerSource && !fLogicTriggerSource(m_handle,0))
        return fail(error,QStringLiteral("Disabling DigitalIn trigger"));
    if (!fLogicConfigure(m_handle,1,0)) return fail(error,QStringLiteral("Configuring DigitalIn"));
    *effectiveRateHz=clockHz/double(divider);
    return true;
}
bool DwfRuntime::logicStart(QString* error) {
    if (!connected() || !fLogicConfigure(m_handle, 0, 1)) return fail(error, QStringLiteral("Starting logic acquisition"));
    return true;
}
bool DwfRuntime::logicStop(QString* error) {
    if (!connected() || !fLogicConfigure(m_handle,0,0))
        return fail(error,QStringLiteral("Stopping DigitalIn acquisition"));
    return true;
}
bool DwfRuntime::logicWait(double seconds, const std::atomic_bool& stop, QString* error) {
    QElapsedTimer timer; timer.start();
    const qint64 timeoutMs = qint64(qBound(0.01, seconds, 3600.0) * 1000);
    while (!stop.load()) {
        unsigned char state = 0;
        if (!connected() || !fLogicStatus(m_handle,1,&state)) return fail(error, QStringLiteral("Logic acquisition status"));
        m_logicLastStateCode=int(state);
        if (state == 2) {
            if(fLogicSamplesValid) {
                int valid=-1;
                if(fLogicSamplesValid(m_handle,&valid)) m_logicLastValidSamples=valid;
            }
            return true;
        }
        if (timer.elapsed() >= timeoutMs) { if(error) *error = QStringLiteral("Logic acquisition timed out"); return false; }
        QThread::msleep(5);
    }
    if (error) *error = QStringLiteral("Script was stopped");
    return false;
}
bool DwfRuntime::readLogic(int samples, QVector<quint16>& out, QString* error) {
    if (!connected() || samples < 1 || samples > 1'048'576) return fail(error, QStringLiteral("Reading logic data"));
    // IMPORTANT: FDwfDigitalInStatusData's third argument is BYTES, not samples.
    // v5.6.49.1 requested only 4096 bytes for 4096 16-bit words, producing
    // 2048 valid words followed by 2048 zeros (the apparent half-length signal).
    static_assert(sizeof(quint16)==2, "DigitalIn format requires 16-bit words");
    const int bytes=samples*int(sizeof(quint16)); // max 2 MiB under guard above
    out.resize(samples);
    out.fill(0); // always initialize, including after a failed SDK call
    m_logicLastBytesRequested=bytes;
    if (!fLogicData(m_handle, out.data(), bytes)) {
        out.clear();
        return fail(error, QStringLiteral("Logic samples (16-bit byte count)"));
    }
    return true;
}

// v5.6.44: AD3 programmable supplies. AnalogIO nodes are not Wavegen channels.
bool DwfRuntime::suppliesAvailable() const {
    return connected() && m_name.contains(QStringLiteral("Analog Discovery 3"), Qt::CaseInsensitive) &&
           fSupplyMasterEnable && fSupplyConfigure && fSupplyNodeSet;
}
bool DwfRuntime::stopSupplies(QString* error) {
    if (!suppliesAvailable()) {
        if (error) *error=QStringLiteral("AD3 AnalogIO supplies are unavailable on this device/SDK");
        return false;
    }
    // Master first: no analog outputs remain energized while any other settings change.
    if (!fSupplyMasterEnable(m_handle, 0)) return fail(error,QStringLiteral("Disabling AD3 supply master"));
    if (!fSupplyConfigure(m_handle)) return fail(error,QStringLiteral("Committing AD3 supply master OFF"));
    return true;
}
bool DwfRuntime::configureSupplies(bool master, bool positiveEnable, double positiveVolts,
                                   bool negativeEnable, double negativeVolts, QString* error) {
    if (!suppliesAvailable()) {
        if (error) *error=QStringLiteral("AD3 programmable V+/V- supplies are unavailable (requires Analog Discovery 3)");
        return false;
    }
    if (!qIsFinite(positiveVolts) || !qIsFinite(negativeVolts) ||
        positiveVolts < 0.5 || positiveVolts > 5.0 ||
        negativeVolts > -0.5 || negativeVolts < -5.0) {
        if (error) *error=QStringLiteral("AD3 supply range: V+ 0.5..5.0 V, V- -5.0..-0.5 V");
        return false;
    }
    // A failed partial reconfiguration must fail-closed. Disable master on SDK failure.
    auto problem=[&](const QString& operation) {
        const QString detail=operation+QStringLiteral(": ")+lastError();
        QString ignored;
        stopSupplies(&ignored);
        if (error) *error= detail+(ignored.isEmpty() ? QString() : QStringLiteral("; emergency stop: ")+ignored);
        return false;
    };
    // Set voltage and individual channel enables before switching the master on.
    if (!fSupplyNodeSet(m_handle, 0, 1, positiveVolts)) return problem(QStringLiteral("Setting V+ voltage"));
    if (!fSupplyNodeSet(m_handle, 1, 1, negativeVolts)) return problem(QStringLiteral("Setting V- voltage"));
    if (!fSupplyNodeSet(m_handle, 0, 0, positiveEnable ? 1.0 : 0.0)) return problem(QStringLiteral("Enabling V+ channel"));
    if (!fSupplyNodeSet(m_handle, 1, 0, negativeEnable ? 1.0 : 0.0)) return problem(QStringLiteral("Enabling V- channel"));
    if (!fSupplyConfigure(m_handle)) return problem(QStringLiteral("Committing AD3 supply nodes"));
    if (!fSupplyMasterEnable(m_handle, master ? 1 : 0)) return problem(QStringLiteral("Setting AD3 supply master"));
    if (!fSupplyConfigure(m_handle)) return problem(QStringLiteral("Committing AD3 supply master"));
    return true;
}
bool DwfRuntime::suppliesReadback(double* positiveVolts, double* negativeVolts, QString* error) {
    if (!suppliesAvailable() || !fSupplyStatus || !fSupplyNodeStatus) {
        if (error) *error=QStringLiteral("AD3 supply voltage status/readback unsupported");
        return false;
    }
    if (!fSupplyStatus(m_handle)) return fail(error,QStringLiteral("Reading AD3 AnalogIO status"));
    double plus=0.0,minus=0.0;
    if (!fSupplyNodeStatus(m_handle,0,1,&plus) || !fSupplyNodeStatus(m_handle,1,1,&minus))
        return fail(error,QStringLiteral("Reading AD3 supply voltage nodes"));
    if (positiveVolts) *positiveVolts=plus;
    if (negativeVolts) *negativeVolts=minus;
    return true;
}
