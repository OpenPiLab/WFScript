#include "ScriptWorker.h"
#include "DwfRuntime.h"
#include "WaveFormsBindings.h"
#include "PowerSuppliesBridge.h"
#include "StaticIOBridge.h"
#include "PatternsBridge.h"
#include "ScriptHelpers.h"
#include "ScriptLibraryBridge.h"
#include "ScriptErrorDetails.h"

#include <QJSValue>
#include <QMutexLocker>
#include <QStringList>
#include <QFileInfo>

ScriptWorker::ScriptWorker(QObject* parent) : QObject(parent) {}
void ScriptWorker::requestStop() {
    m_stopRequested.store(true);
    QMutexLocker locker(&m_mutex);
    if (m_activeEngine) m_activeEngine->setInterrupted(true);
}

void ScriptWorker::execute(const QString& source, const QString& fileName,
                           bool physicalDevice, int deviceIndex, int configIndex) {
    // Do not clear a Stop request made before the worker thread began.
    if (m_stopRequested.load()) {
        emit finished(false,QStringLiteral("Script interrupted before startup"));
        return;
    }
    DwfRuntime runtime;
    MockInstrumentState mock;
    QString error;
    if (physicalDevice && !runtime.open(deviceIndex, configIndex, &error)) {
        emit outputReady(QStringLiteral("Device connection failed: %1").arg(error));
        emit finished(false, QStringLiteral("Unable to connect to WaveForms SDK device."));
        return;
    }
    emit outputReady(physicalDevice
       ? QStringLiteral("REAL HARDWARE: %1 (%2), config %3").arg(runtime.deviceName(),runtime.serial()).arg(configIndex+1)
       : QStringLiteral("SIMULATION: no hardware I/O"));

    // All QObjects and QJSEngine are constructed, accessed and destroyed on the worker thread.
    QJSEngine engine;
    ConsoleBridge console;
    ScopeBridge scope(physicalDevice ? &runtime : nullptr,
                      physicalDevice ? nullptr : &mock, &m_stopRequested);
    WavegenBridge wavegen(physicalDevice ? &runtime : nullptr,
                          physicalDevice ? nullptr : &mock, &m_stopRequested);
    PowerSuppliesBridge supplies(physicalDevice ? &runtime : nullptr);
    StaticIOBridge staticIO(physicalDevice ? &runtime : nullptr);
    PatternsBridge patterns(physicalDevice ? &runtime : nullptr);
    LogicBridge logic(physicalDevice ? &runtime : nullptr,
                      physicalDevice ? nullptr : &mock, &m_stopRequested);
    DeviceBridge device(physicalDevice ? &runtime : nullptr, !physicalDevice);
    ScriptUtilities utilities(&m_stopRequested);
    connect(&scope, &ScopeBridge::captureReady, this, &ScriptWorker::scopeFrameReady, Qt::DirectConnection);
    connect(&console, &ConsoleBridge::printed, this,
            &ScriptWorker::outputReady, Qt::DirectConnection);
    connect(&console, &ConsoleBridge::clearRequested, this,
            &ScriptWorker::consoleClearRequested, Qt::DirectConnection);

    QObject* objects[] = {&console,&scope,&wavegen,&supplies,&staticIO,&patterns,&logic,&device,&utilities};
    for (QObject* object : objects) QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    // QObject pointers exposed through nested properties refer to stack-owned
    // member objects. Explicitly prevent JS garbage collection from deleting them.
    auto* logicTime=static_cast<LogicTime*>(logic.time());
    for(QObject* object:{static_cast<QObject*>(logicTime),logicTime->rate(),logicTime->samples()})
        QJSEngine::setObjectOwnership(object,QJSEngine::CppOwnership);
    // The Patterns nested parameter QObjects are owned by native C++.
    auto& pc=patterns.channelGroup().primary();
    for(QObject* item : {patterns.channels(),static_cast<QObject*>(&pc),pc.pin(),pc.type(),
                         pc.output(),pc.idle(),pc.frequency(),pc.duty()})
        QJSEngine::setObjectOwnership(item,QJSEngine::CppOwnership);
    auto* trigger = static_cast<ScopeTrigger*>(scope.trigger());
    auto* scopeTime = static_cast<ScopeTime*>(scope.time());
    auto* scopeC1 = static_cast<ScopeChannel*>(scope.channel1());
    auto* scopeC2 = static_cast<ScopeChannel*>(scope.channel2());
    auto* waveC1 = static_cast<WavegenChannel*>(wavegen.channel1());
    auto* waveC2 = static_cast<WavegenChannel*>(wavegen.channel2());
    auto* waveS1 = static_cast<WavegenSimple*>(waveC1->simple());
    auto* waveS2 = static_cast<WavegenSimple*>(waveC2->simple());
    QObject* nested[] = {
        scopeTime, scopeTime->samples(), scopeTime->rate(), scopeTime->position(), scopeTime->mode(), scopeTime->recordLength(),
        trigger,trigger->trigger(),trigger->type(),trigger->source(),trigger->condition(),
        trigger->level(),trigger->hysteresis(), trigger->lengthCondition(), trigger->length(), trigger->holdOff(), trigger->filter(),
        scopeC1, scopeC1->range(), scopeC1->offset(), scopeC1->attenuation(), scopeC1->sampleMode(),
        scopeC2, scopeC2->range(), scopeC2->offset(), scopeC2->attenuation(), scopeC2->sampleMode(),
        waveC1, waveC2, waveC1->mode(), waveC2->mode(), waveS1, waveS2,
        waveC1->custom(),waveC2->custom(),waveC1->burst(),waveC2->burst(),
        waveS1->type(), waveS1->frequency(), waveS1->period(), waveS1->amplitude(), waveS1->offset(),
        waveS1->symmetry(),waveS1->phase(),
        waveS2->type(), waveS2->frequency(), waveS2->period(), waveS2->amplitude(), waveS2->offset(),
        waveS2->symmetry(),waveS2->phase(),
    };
    for (QObject* object : nested) QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    for (WavegenChannel* ch : {waveC1,waveC2}) {
        auto* burst=static_cast<WavegenBurst*>(ch->burst());
        for (QObject* obj : {burst->run(),burst->waitTime(),burst->repeat(),burst->master()})
            QJSEngine::setObjectOwnership(obj,QJSEngine::CppOwnership);
    }
    // Release B nested Wavegen objects are C++ members, never JS-owned.
    for (WavegenChannel* ch : {waveC1,waveC2}) {
        auto* mod=static_cast<WavegenModulation*>(ch->modulation());
        auto* sw=static_cast<WavegenSweep*>(ch->sweep());
        auto* trig=static_cast<WavegenTrigger*>(ch->trigger());
        auto* play=static_cast<WavegenPlay*>(ch->playSettings());
        QObject* objs[]={mod,mod->fm(),mod->am(),sw,sw->frequency(),sw->amplitude(),
                          trig,trig->source(),trig->idle(),play,play->rate()};
        for(auto* o:objs) QJSEngine::setObjectOwnership(o,QJSEngine::CppOwnership);
        for(QObject* n : {mod->fm(), mod->am()}) {
            auto* m=static_cast<WavegenModulationNode*>(n);
            for(auto* v:{m->type(),m->frequency(),m->depth()})
                QJSEngine::setObjectOwnership(v,QJSEngine::CppOwnership);
        }
        for(QObject* n:{sw->frequency(),sw->amplitude()}) {
            auto* axis=static_cast<WavegenSweepAxis*>(n);
            for(auto* v:{axis->start(),axis->stop(),axis->time()})
                QJSEngine::setObjectOwnership(v,QJSEngine::CppOwnership);
        }
    }
    // All supply parameter QObjects are C++ members with application-owned lifetime.
    auto* outputs=&supplies.outputs();
    auto* pos=&outputs->positiveChannel();
    auto* neg=&outputs->negativeChannel();
    for (QObject* obj : {supplies.masterEnable(), supplies.output(),
           static_cast<QObject*>(pos), pos->enable(), pos->voltage(),
           static_cast<QObject*>(neg), neg->enable(), neg->voltage()})
        QJSEngine::setObjectOwnership(obj,QJSEngine::CppOwnership);
    // Two groups of eight pins and read-only Input objects remain C++-owned.
    for(QObject* group : {staticIO.channel0(),staticIO.channel1()}) {
        QJSEngine::setObjectOwnership(group,QJSEngine::CppOwnership);
    }
    for(auto* group: {static_cast<StaticIOGroup*>(staticIO.channel0()),static_cast<StaticIOGroup*>(staticIO.channel1())}) {
        QJSEngine::setObjectOwnership(group->input(),QJSEngine::CppOwnership);
        QJSEngine::setObjectOwnership(group->mode(),QJSEngine::CppOwnership);
    }
    for(int i=0;i<16;++i) {
        auto* pin=staticIO.pin(i);
        QJSEngine::setObjectOwnership(pin,QJSEngine::CppOwnership);
        QJSEngine::setObjectOwnership(pin->input(),QJSEngine::CppOwnership);
    }
    ScriptLibraryBridge libraries(&engine,fileName);
    QJSEngine::setObjectOwnership(&libraries,QJSEngine::CppOwnership);
    if (!libraries.valid()) { emit finished(false,libraries.error()); return; }
    auto global = engine.globalObject();
    global.setProperty(QStringLiteral("__wfLibraryHost"),engine.newQObject(&libraries));
    global.setProperty(QStringLiteral("__bridge"), engine.newQObject(&console));
    global.setProperty(QStringLiteral("__utils"), engine.newQObject(&utilities));
    global.setProperty(QStringLiteral("Scope1"),engine.newQObject(&scope));
    global.setProperty(QStringLiteral("Wavegen1"),engine.newQObject(&wavegen));
    global.setProperty(QStringLiteral("Logic1"),engine.newQObject(&logic));
    global.setProperty(QStringLiteral("Supplies"),engine.newQObject(&supplies));
    global.setProperty(QStringLiteral("Supplies1"),global.property(QStringLiteral("Supplies")));
    global.setProperty(QStringLiteral("StaticIO"),engine.newQObject(&staticIO));
    global.setProperty(QStringLiteral("Patterns1"),engine.newQObject(&patterns));
    global.setProperty(QStringLiteral("Patterns"),global.property(QStringLiteral("Patterns1")));
    global.setProperty(QStringLiteral("Device"),engine.newQObject(&device));
    // WaveForms allows unnumbered aliases when only the first instrument is intended.
    global.setProperty(QStringLiteral("Scope"),global.property(QStringLiteral("Scope1")));
    global.setProperty(QStringLiteral("Wavegen"),global.property(QStringLiteral("Wavegen1")));
    global.setProperty(QStringLiteral("Logic"),global.property(QStringLiteral("Logic1")));

    const QJSValue boot = engine.evaluate(QStringLiteral(R"JS(
        var print = function(...args) {
            __bridge.write(args.map(function(v) { return String(v); }).join(' '));
        };
        var console = { log: print, warn: print, error: print };
        var wait = function(seconds) { return seconds === undefined ? __utils.wait() : __utils.wait(seconds); };
    )JS") + ScriptHelpers::source() + ScriptLibraryBridge::bootstrapSource(), QStringLiteral("<script-environment>"));
    if (boot.isError()) {
        emit finished(false, QStringLiteral("JavaScript bootstrap failed: %1").arg(boot.toString()));
        return;
    }
    { QMutexLocker lock(&m_mutex); m_activeEngine=&engine;
      if (m_stopRequested.load()) engine.setInterrupted(true); }
    QStringList stack;
    const QJSValue result = (fileName.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive)
                             && QFileInfo::exists(fileName))
        ? engine.importModule(fileName) : engine.evaluate(source,fileName,1,&stack);
    const bool interrupted = engine.isInterrupted() || m_stopRequested.load();
    { QMutexLocker lock(&m_mutex); m_activeEngine=nullptr; }

    // Hardware outputs are explicitly stopped at script completion; scripts do not
    // leave a waveform running after the device handle is closed.
    logic.stop();
    if (!patterns.stop()) emit outputReady(QStringLiteral("WARNING: Patterns output shutdown not confirmed: %1").arg(patterns.lastError()));
    if (!staticIO.stop()) emit outputReady(QStringLiteral("WARNING: StaticIO high-Z shutdown not confirmed: %1").arg(staticIO.lastError()));
    supplies.stop();
    wavegen.stop();
    if (interrupted) {
        emit finished(false,QStringLiteral("Script interrupted by user"));
    } else if (result.isError() || !stack.isEmpty()) {
        const auto detail=ScriptErrorFormatting::fromException(result,fileName,stack);
        const QString sourceName=libraries.displayNameForPath(detail.source);
        if (detail.line>0) emit scriptError(detail.line,detail.message);
        emit scriptDiagnostic(sourceName,detail.line,detail.column,detail.message,
                              libraries.displayStack(detail.stack));
        emit outputReady(QStringLiteral("[Script ERROR] %1:%2%3 — %4")
            .arg(sourceName).arg(detail.line)
            .arg(detail.column>0 ? QStringLiteral(":%1").arg(detail.column) : QString())
            .arg(detail.message));
        if (!detail.stack.isEmpty()) emit outputReady(detail.stack);
        emit finished(false, QStringLiteral("Script failed"));
    } else {
        if (!result.isUndefined()) emit outputReady(QStringLiteral("Result: %1").arg(result.toString()));
        emit finished(true,QStringLiteral("Script completed"));
    }
}
