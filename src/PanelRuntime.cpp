#include "PanelRuntime.h"
#include "plot/PlotBridge.h"
#include "PanelEventCatalog.h"
#include "ScriptWorker.h"
#include "PowerSuppliesBridge.h"
#include "StaticIOBridge.h"
#include "PatternsBridge.h"
#include "ScriptHelpers.h"
#include "ScriptLibraryBridge.h"
#include "ScriptErrorDetails.h"
#include "TestRunnerBridge.h"
#include "RunInformationSchema.h"
#include <QJSValue>
#include <QJSValueList>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QStringList>

PanelRuntime::PanelRuntime(QObject* parent) : QObject(parent) {
    qRegisterMetaType<PlotCommand>("PlotCommand");
}
void PanelRuntime::requestStop() {
    m_stopRequested.store(true);
    QMutexLocker lock(&m_engineMutex);
    // An idle worker can run onShutdown() normally. Only interrupt JavaScript
    // that is actively executing (including an infinite callback).
    if(m_engine && (m_executingJs.load() || m_runnerJsActive.load()>0)) {
        m_interruptedByStop.store(true);
        m_engine->setInterrupted(true);
    }
}

void PanelRuntime::registerObjects(QJSEngine* engine) {
    // Mirror ScriptWorker's ownership policy; every exposed QObject is C++-owned.
    QObject* objects[] = {m_ui,m_console,m_scope,m_wavegen,m_supplies,m_staticIO,m_patterns,m_logic,m_device,m_utilities,m_plots,m_testRunner};
    for (auto* object:objects) QJSEngine::setObjectOwnership(object,QJSEngine::CppOwnership);
    auto* logicTime=static_cast<LogicTime*>(m_logic->time());
    for(QObject* object:{static_cast<QObject*>(logicTime),logicTime->rate(),logicTime->samples()})
        QJSEngine::setObjectOwnership(object,QJSEngine::CppOwnership);
    auto* trg=static_cast<ScopeTrigger*>(m_scope->trigger());
    auto* st=static_cast<ScopeTime*>(m_scope->time());
    auto* c1=static_cast<ScopeChannel*>(m_scope->channel1());
    auto* c2=static_cast<ScopeChannel*>(m_scope->channel2());
    auto* w1=static_cast<WavegenChannel*>(m_wavegen->channel1());
    auto* w2=static_cast<WavegenChannel*>(m_wavegen->channel2());
    auto* ws1=static_cast<WavegenSimple*>(w1->simple());
    auto* ws2=static_cast<WavegenSimple*>(w2->simple());
    QObject* nested[]={st,st->samples(),st->rate(),st->position(), st->mode(), st->recordLength(),trg,trg->trigger(),trg->type(),trg->source(),
        trg->condition(),trg->level(),trg->hysteresis(), trg->lengthCondition(), trg->length(), trg->holdOff(), trg->filter(),c1,c1->range(),c1->offset(),c1->attenuation(), c1->sampleMode(),c2,c2->range(),c2->offset(),c2->attenuation(), c2->sampleMode(),
        w1,w2,w1->mode(),w2->mode(),ws1,ws2,
        w1->custom(),w2->custom(),w1->burst(),w2->burst(),
        ws1->type(),ws1->frequency(),ws1->period(),ws1->amplitude(),ws1->offset(),ws1->symmetry(),ws1->phase(),
        ws2->type(),ws2->frequency(),ws2->period(),ws2->amplitude(),ws2->offset(),ws2->symmetry(),ws2->phase()};
    for(auto* object:nested) QJSEngine::setObjectOwnership(object,QJSEngine::CppOwnership);
    for (WavegenChannel* ch : {w1,w2}) {
        auto* burst=static_cast<WavegenBurst*>(ch->burst());
        for (QObject* obj : {burst->run(),burst->waitTime(),burst->repeat(),burst->master()})
            QJSEngine::setObjectOwnership(obj,QJSEngine::CppOwnership);
    }
    // Release B nested Wavegen objects are C++ members, never JS-owned.
    for (WavegenChannel* ch : {w1,w2}) {
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
    auto* outputs=&m_supplies->outputs();
    auto* pos=&outputs->positiveChannel();
    auto* neg=&outputs->negativeChannel();
    for (QObject* obj : {m_supplies->masterEnable(), m_supplies->output(),
             static_cast<QObject*>(pos),pos->enable(),pos->voltage(),
             static_cast<QObject*>(neg),neg->enable(),neg->voltage()})
        QJSEngine::setObjectOwnership(obj,QJSEngine::CppOwnership);
    QJSEngine::setObjectOwnership(m_staticIO->channel0(),QJSEngine::CppOwnership);
    QJSEngine::setObjectOwnership(m_staticIO->channel1(),QJSEngine::CppOwnership);
    for(auto* group: {static_cast<StaticIOGroup*>(m_staticIO->channel0()),static_cast<StaticIOGroup*>(m_staticIO->channel1())}) {
        QJSEngine::setObjectOwnership(group->input(),QJSEngine::CppOwnership);
        QJSEngine::setObjectOwnership(group->mode(),QJSEngine::CppOwnership);
    }
    for(int i=0;i<16;++i) {
        auto* dio=m_staticIO->pin(i);
        QJSEngine::setObjectOwnership(dio,QJSEngine::CppOwnership);
        QJSEngine::setObjectOwnership(dio->input(),QJSEngine::CppOwnership);
    }
    auto& pc=m_patterns->channelGroup().primary();
    for(QObject* item : {m_patterns->channels(),static_cast<QObject*>(&pc),pc.pin(),pc.type(),
                         pc.output(),pc.idle(),pc.frequency(),pc.duty()})
        QJSEngine::setObjectOwnership(item,QJSEngine::CppOwnership);
    auto global=engine->globalObject();
    global.setProperty("__panelHost",engine->newQObject(m_ui));
    global.setProperty("TestRunner",engine->newQObject(m_testRunner));
    global.setProperty("__plotHost",engine->newQObject(m_plots));
    global.setProperty("__bridge",engine->newQObject(m_console));
    global.setProperty("__utils",engine->newQObject(m_utilities));
    global.setProperty("Scope1",engine->newQObject(m_scope));
    global.setProperty("Wavegen1",engine->newQObject(m_wavegen));
    global.setProperty("Logic1",engine->newQObject(m_logic));
    global.setProperty("Supplies",engine->newQObject(m_supplies));
    global.setProperty("Supplies1",global.property("Supplies"));
    global.setProperty("StaticIO",engine->newQObject(m_staticIO));
    global.setProperty("Patterns1",engine->newQObject(m_patterns));
    global.setProperty("Patterns",global.property("Patterns1"));
    global.setProperty("Device",engine->newQObject(m_device));
    global.setProperty("Scope",global.property("Scope1"));
    global.setProperty("Wavegen",global.property("Wavegen1"));
    global.setProperty("Logic",global.property("Logic1"));
}

void PanelRuntime::failedStartup(const QString& reason) {
    emit lifecycleChanged(QStringLiteral("Failed"), reason);
    finishStop();
}

void PanelRuntime::finishStop() {
    if (m_stopped) return;
    const bool orderly = m_ready && m_engine && !m_interruptedByStop.load() &&
                         !m_engine->isInterrupted();
    m_stopped=true;
    m_ready=false;
    m_stopRequested.store(true);
    if(orderly) runShutdownHook();
    else if(m_lifecycle.shutdownEnabled)
        emit lifecycleChanged(QStringLiteral("ShutdownSkipped"),
            m_interruptedByStop.load() ? QStringLiteral("JavaScript was interrupted; native cleanup only")
                                       : QStringLiteral("Panel did not finish initialization"));
    emit lifecycleChanged(QStringLiteral("Stopping"),QStringLiteral("Releasing panel resources"));
    cleanup();
    // Do not report Stopped yet: the QThread event loop is still exiting.
    // MainWindow publishes Stopped only after QThread::finished.
    emit lifecycleChanged(QStringLiteral("CleanupComplete"),
                          QStringLiteral("Panel resources released; waiting for worker thread exit"));
    emit stopped();
}

void PanelRuntime::start(const QString& source,const QVariantMap& initialValues,
                         bool physical,int deviceIndex,int configIndex,
                         const QString& projectFile,const QVariantMap& embedded,
                         const QString& scriptName,
                         const PanelLifecycleSettings& lifecycle,
                         const QJsonArray& controls,
                         const QJsonObject& testDefinition,const QString& testLibrary,
                         const QJsonObject& runInformation,
                         const QVariantMap& runInformationInput,
                         const QJsonObject& reportStorage) {
    if (m_engine || m_stopped) return;
    m_scriptName=scriptName.isEmpty() ? QStringLiteral("Panel Script.js") : scriptName;
    m_lifecycle=lifecycle;
    m_managedHandlers.clear();
    m_runInformationIds.clear();
    for(const auto& entry:controls) {
        const QJsonObject control=entry.toObject();
        const QString id=control.value(QStringLiteral("id")).toString();
        const QString type=control.value(QStringLiteral("type")).toString();
        if(type==QStringLiteral("runInformation"))m_runInformationIds.insert(id);
        const QJsonObject bindings=control.value(QStringLiteral("events")).toObject();
        for(auto it=bindings.begin();it!=bindings.end();++it) {
            const QJsonObject b=it.value().toObject();
            if(b.value(QStringLiteral("enabled")).toBool() && PanelEventCatalog::supports(type,it.key()) &&
               PanelEventCatalog::validHandler(b.value(QStringLiteral("handler")).toString()))
                m_managedHandlers.insert(id+QStringLiteral("::")+it.key(),b.value(QStringLiteral("handler")).toString());
        }
    }
    // A Stop request can race with the worker thread's first started signal.
    // Never clear that request during startup.
    if (m_stopRequested.load()) { finishStop(); return; }
    emit lifecycleChanged(QStringLiteral("Starting"),m_scriptName);
    QString error;
    m_runtime=std::make_unique<DwfRuntime>();
    if (physical && !m_runtime->open(deviceIndex,configIndex,&error)) {
        emit failed(QStringLiteral("Panel hardware connection failed: %1").arg(error));
        failedStartup(QStringLiteral("Hardware connection failed"));
        return;
    }
    if (m_stopRequested.load()) { finishStop(); return; }
    emit printed(physical
        ? QStringLiteral("PANEL HARDWARE: %1, configuration %2")
            .arg(m_runtime->deviceName()).arg(configIndex+1)
        : QStringLiteral("PANEL SIMULATION: hardware outputs disabled"));
    { QMutexLocker lock(&m_engineMutex); m_engine=new QJSEngine;
      if(m_stopRequested.load())m_engine->setInterrupted(true); }
    m_ui=new PanelUiBridge(m_engine,initialValues,m_runInformationIds);
    // ui.get(formId) and native runner input must reflect the SAME values
    // immediately on startup (including visible edits retained after Stop).
    QVariantMap initialRunInfo=RunInformationSchema::defaults(runInformation);
    for(auto it=runInformationInput.cbegin();it!=runInformationInput.cend();++it)
        if(initialRunInfo.contains(it.key()))initialRunInfo.insert(it.key(),it.value());
    for(const QString& formId:m_runInformationIds)m_ui->receiveEvent(formId,initialRunInfo);
    m_testRunner=new TestRunnerBridge(m_engine,
        [this](const QString& id,const QVariant& value){return m_ui && m_ui->setNative(id,value);},
        &m_stopRequested);
    m_testRunner->configureRunInformation(runInformation);
    m_testRunner->configureArchive(reportStorage,physical,deviceIndex,configIndex,
                                    QFileInfo(projectFile).fileName(),
                                    physical?m_runtime->deviceName():QString(),
                                    physical?m_runtime->serial():QString());
    m_testRunner->setNativeRunInformation(initialRunInfo);
    connect(m_testRunner,&TestRunnerBridge::message,this,&PanelRuntime::printed,Qt::DirectConnection);
    connect(m_testRunner,&TestRunnerBridge::scriptCallbackActive,this,
        [this](bool active){if(active)m_runnerJsActive.fetch_add(1);
                            else m_runnerJsActive.fetch_sub(1);},Qt::DirectConnection);
    connect(m_testRunner,&TestRunnerBridge::stepException,this,
        [this](const QString& id,const QJSValue& exception){
            reportError(QStringLiteral("TestRunner step %1").arg(id),exception);
        },Qt::DirectConnection);
    m_console=new ConsoleBridge;
    m_scope=new ScopeBridge(physical?m_runtime.get():nullptr,physical?nullptr:&m_mock,&m_stopRequested);
    QSet<QString> plotIds;
    for(const QJsonValue& entry:controls) {
        const QJsonObject c=entry.toObject();
        if(c.value(QStringLiteral("type")).toString()==QStringLiteral("plot") || c.value(QStringLiteral("type")).toString()==QStringLiteral("digital_plot"))
            plotIds.insert(c.value(QStringLiteral("id")).toString());
    }
    m_plots=new PlotBridge(plotIds,m_scope);
    m_wavegen=new WavegenBridge(physical?m_runtime.get():nullptr,physical?nullptr:&m_mock,&m_stopRequested);
    m_supplies=new PowerSuppliesBridge(physical?m_runtime.get():nullptr);
    m_staticIO=new StaticIOBridge(physical?m_runtime.get():nullptr);
    m_patterns=new PatternsBridge(physical?m_runtime.get():nullptr);
    m_logic=new LogicBridge(physical?m_runtime.get():nullptr,physical?nullptr:&m_mock,&m_stopRequested);
    m_device=new DeviceBridge(physical?m_runtime.get():nullptr,!physical);
    m_utilities=new ScriptUtilities(&m_stopRequested);
    connect(m_ui,&PanelUiBridge::updateRequested,this,&PanelRuntime::controlUpdate,Qt::DirectConnection);
    connect(m_console,&ConsoleBridge::printed,this,&PanelRuntime::printed,Qt::DirectConnection);
    connect(m_console,&ConsoleBridge::clearRequested,this,&PanelRuntime::consoleClearRequested,Qt::DirectConnection);
    connect(m_scope,&ScopeBridge::captureReady,this,&PanelRuntime::scopeFrameReady,Qt::DirectConnection);
    connect(m_plots,&PlotBridge::commandReady,this,&PanelRuntime::plotCommandReady,Qt::DirectConnection);
    registerObjects(m_engine);
    m_libraries=new ScriptLibraryBridge(m_engine,projectFile,embedded);
    if (m_stopRequested.load()) { finishStop();return; }
    if (!m_libraries->valid()) {
        emit failed(m_libraries->error());
        failedStartup(QStringLiteral("Library initialization failed"));
        return;
    }
    QJSEngine::setObjectOwnership(m_libraries,QJSEngine::CppOwnership);
    m_engine->globalObject().setProperty("__wfLibraryHost",m_engine->newQObject(m_libraries));
    const QJSValue boot=m_engine->evaluate(QStringLiteral(R"JS(
        var print = function(...args) { __bridge.write(args.map(String).join(' ')); };
        var console = {log:print, warn:print, error:print};
        var wait = function(seconds) { return seconds === undefined ? __utils.wait() : __utils.wait(seconds); };
        var __panelHandlers = {};
        var ui = {
            get: function(id) { return __panelHost.get(id); },
            set: function(id,value) { return __panelHost.set(id,value); },
            on: function(id,name,callback) {
                if (typeof callback !== 'function') throw new Error('Callback required');
                var key=id+'::'+name;
                (__panelHandlers[key] || (__panelHandlers[key]=[])).push(callback);
            }
        };
        // A Panel plot owns its own series. No implicit ScopeBridge broadcast.
        var plots = {
            setSeries: function(id,name,x,y,options) {
                if(!__plotHost.setSeries(id,name,x,y,options || {})) throw new Error(__plotHost.lastError);
                return true;
            },
            append: function(id,name,x,y,options) {
                if(!__plotHost.appendSeries(id,name,x,y,options || {})) throw new Error(__plotHost.lastError);
                return true;
            },
            appendPoint: function(id,name,x,y) {
                if(!__plotHost.appendPoint(id,name,x,y)) throw new Error(__plotHost.lastError);
                return true;
            },
            setSamples: function(id,name,y,rateHz,startSeconds,options) {
                if(typeof startSeconds==='object' && options===undefined) {
                    options=startSeconds; startSeconds=0;
                }
                if(!__plotHost.setSamples(id,name,y,rateHz,
                       startSeconds===undefined?0:startSeconds,options || {}))
                    throw new Error(__plotHost.lastError);
                return true;
            },
            fromScope: function(id,channel,name) {
                channel=channel===undefined?1:channel;
                name=name===undefined?('CH'+channel):name;
                if(!__plotHost.fromScope(id,channel,name)) throw new Error(__plotHost.lastError);
                return true;
            },
            configure: function(id,settings) {
                if(!__plotHost.configure(id,settings)) throw new Error(__plotHost.lastError);
                return true;
            },
            styleSeries: function(id,name,options) {
                if(!__plotHost.styleSeries(id,name,options || {})) throw new Error(__plotHost.lastError);
                return true;
            },
            removeSeries: function(id,name) {
                if(!__plotHost.removeSeries(id,name)) throw new Error(__plotHost.lastError);
                return true;
            },
            clear: function(id) {
                if(!__plotHost.clear(id)) throw new Error(__plotHost.lastError);
                return true;
            }
        };
        function __panelDispatch(id,name,value) {
            var callbacks=__panelHandlers[id+'::'+name] || [];
            callbacks.forEach(function(callback) { callback(value); });
        }
    )JS")+ScriptHelpers::source()+ScriptLibraryBridge::bootstrapSource(),"<panel-bootstrap>");
    if (m_stopRequested.load() || m_engine->isInterrupted()) { finishStop();return; }
    if(boot.isError()) {
        reportError("Panel JavaScript bootstrap",boot);
        failedStartup(QStringLiteral("JavaScript bootstrap failed"));
        return;
    }
    // Native project test: no Panel script is permitted to register, reload or
    // configure a second, conflicting executive. Resolve handlers from ONE
    // embedded module after the module loader has initialized.
    if(!testDefinition.isEmpty()) {
        const QByteArray quotedName=QJsonDocument(QJsonArray{testLibrary}).toJson(QJsonDocument::Compact);
        const QString expression=QStringLiteral("modules.load(")+QString::fromUtf8(quotedName.mid(1,quotedName.size()-2))+QStringLiteral(")");
        const QJSValue library=m_engine->evaluate(expression,QStringLiteral("<test-library-bind>"));
        if(library.isError() || !library.isObject()) {
            emit failed(QStringLiteral("Test library %1 could not be loaded: %2").arg(testLibrary,library.toString()));
            failedStartup(QStringLiteral("Native test library binding failed"));return;
        }
        const QJSValue doc=ScriptValueConversion::toJavaScript(m_engine,testDefinition.toVariantMap());
        if(!m_testRunner->loadDefinition(doc,library)) {
            emit failed(QStringLiteral("Project test definition invalid: %1").arg(m_testRunner->lastError()));
            failedStartup(QStringLiteral("Native test definition rejected"));return;
        }
        m_testRunner->setProjectManaged(true);
        emit printed(QStringLiteral("C++ Test Executive loaded project definition: %1 (%2 steps, timeout %3 ms)")
            .arg(testDefinition.value(QStringLiteral("title")).toString())
            .arg(testDefinition.value(QStringLiteral("steps")).toArray().size())
            .arg(m_testRunner->summary().value(QStringLiteral("totalTimeoutMs")).toInt()));
    }
    QStringList stack;
    m_executingJs.store(true);
    const QJSValue result=m_engine->evaluate(source,m_scriptName,1,&stack);
    m_executingJs.store(false);
    if (m_stopRequested.load() || m_engine->isInterrupted()) { finishStop();return; }
    if(result.isError() || !stack.isEmpty()) {
        reportError("Panel script initialization",result,stack);
        failedStartup(QStringLiteral("Panel script initialization failed"));
        return;
    }
    if(m_lifecycle.initializeEnabled && !runInitializeHook()){
        if(m_stopRequested.load() || m_engine->isInterrupted()) { finishStop(); return; }
        failedStartup(QStringLiteral("onInitialize() failed"));
        return;
    }
    if(m_stopRequested.load() || m_engine->isInterrupted()) { finishStop(); return; }
    for(auto it=m_managedHandlers.cbegin();it!=m_managedHandlers.cend();++it) {
        if(!m_engine->globalObject().property(it.value()).isCallable()) {
            const QString message=QStringLiteral("Handler %1 for %2 is not a callable global function. "
                "Create or assign the function in the Panel Events inspector.").arg(it.value(),it.key());
            emit scriptDiagnostic(m_scriptName,0,0,QStringLiteral("Panel event binding"),message,{});
            failedStartup(QStringLiteral("Missing designer-assigned event handler"));
            return;
        }
    }
    // Manual registrations are preserved. Make duplicate delivery visible
    // so the designer never silently overrides user-written ui.on() code.
    const QJSValue legacy=m_engine->globalObject().property(QStringLiteral("__panelHandlers"));
    for(auto it=m_managedHandlers.cbegin();it!=m_managedHandlers.cend();++it) {
        const QJSValue callbacks=legacy.property(it.key());
        if(callbacks.isArray() && callbacks.property(QStringLiteral("length")).toInt()>0)
            emit printed(QStringLiteral("WARNING: %1 has both a designer-assigned handler (%2) "
                                        "and manual ui.on() callback(s); both will execute.")
                             .arg(it.key(),it.value()));
    }
    m_ready=true;
    emit lifecycleChanged(QStringLiteral("Running"),m_scriptName);
    emit ready();
}


bool PanelRuntime::runInitializeHook() {
    emit lifecycleChanged(QStringLiteral("Initializing"),QStringLiteral("Calling onInitialize()"));
    QJSValue fn=m_engine->globalObject().property(QStringLiteral("onInitialize"));
    if(!fn.isCallable()) {
        emit scriptDiagnostic(m_scriptName,0,0,QStringLiteral("Panel onInitialize()"),
            QStringLiteral("onInitialize() is enabled in Panel Settings but no callable function "
                           "onInitialize() was found. Use Create Function in Panel Settings."),{});
        return false;
    }
    m_executingJs.store(true);
    const QJSValue outcome=fn.call();
    m_executingJs.store(false);
    if(m_stopRequested.load() || m_engine->isInterrupted())return false;
    if(outcome.isError()) {
        reportError(QStringLiteral("Panel onInitialize()"),outcome);
        return false;
    }
    emit lifecycleChanged(QStringLiteral("Initialized"),QStringLiteral("onInitialize() completed"));
    return true;
}

void PanelRuntime::runShutdownHook() {
    if(!m_lifecycle.shutdownEnabled) return;
    if(!m_engine || m_engine->isInterrupted())return;
    QJSValue fn=m_engine->globalObject().property(QStringLiteral("onShutdown"));
    if(!fn.isCallable()) {
        emit scriptDiagnostic(m_scriptName,0,0,QStringLiteral("Panel onShutdown()"),
            QStringLiteral("onShutdown() is enabled but is not a callable function; "
                           "continuing native cleanup."),{});
        return;
    }
    emit lifecycleChanged(QStringLiteral("ShutdownHook"),QStringLiteral("Calling onShutdown()"));
    m_executingJs.store(true);
    const QJSValue outcome=fn.call();
    m_executingJs.store(false);
    if(m_engine->isInterrupted()) {
        emit lifecycleChanged(QStringLiteral("ShutdownInterrupted"),
                              QStringLiteral("onShutdown() interrupted; continuing native cleanup"));
    } else if(outcome.isError()) {
        reportError(QStringLiteral("Panel onShutdown()"),outcome);
    } else {
        emit lifecycleChanged(QStringLiteral("ShutdownComplete"),QStringLiteral("onShutdown() completed"));
    }
}

void PanelRuntime::reportError(const QString& context,const QJSValue& result,
                               const QStringList& frames) {
    auto detail=ScriptErrorFormatting::fromException(result,m_scriptName,frames);
    if(m_libraries) detail.source=m_libraries->displayNameForPath(detail.source);
    if(m_libraries)detail.stack=m_libraries->displayStack(detail.stack);
    emit scriptDiagnostic(detail.source,detail.line,detail.column,context,detail.message,detail.stack);
}
void PanelRuntime::eventFromPanel(const QString& id,const QString& name,const QVariant& value) {
    if (!m_engine || !m_ready || m_stopRequested.load()) return;
    // Event payloads and widget state are different concepts. A Button click
    // passes true to its callback, but ui.get(buttonId) must remain the caption.
    // Slider/Toggle events do update persistent ui.get() state.
    if(PanelEventCatalog::carriesPersistentState(name))m_ui->receiveEvent(id,value);
    // OptionSelector emits the selected string (not a numerical design-time value).
    // Update the JavaScript bridge BEFORE dispatching the callback, so ui.get(id)
    // returns the selection the user just made. Do not route this through the
    // PanelCanvas numeric value persistence: the native widget owns its text state.
    if(name==QStringLiteral("selectionChanged"))m_ui->receiveEvent(id,value);
    if(name==QStringLiteral("runInformationChanged") && m_runInformationIds.contains(id)) {
        m_testRunner->setNativeRunInformation(value.toMap());
        // ui.get() on ANY form returns the same pending values, including
        // on hidden Pages and immediately inside the triggering callback.
        const QVariantMap pending=m_testRunner->pendingRunInfo();
        for(const QString& formId:m_runInformationIds)m_ui->receiveEvent(formId,pending);
    }
    const QJSValue fn=m_engine->globalObject().property("__panelDispatch");
    QJSValueList args{QJSValue(id),QJSValue(name),ScriptValueConversion::toJavaScript(m_engine,value)};
    m_executingJs.store(true);
    const QJSValue result=fn.call(args);
    m_executingJs.store(false);
    // User Stop is not an exception in an event callback. Do not emit a
    // spurious JavaScript error for QJSEngine interruption.
    if (m_stopRequested.load() || m_engine->isInterrupted()) return;
    if(result.isError())reportError(QStringLiteral("Panel event %1.%2 (ui.on)").arg(id,name),result);
    const QString bound=m_managedHandlers.value(id+QStringLiteral("::")+name);
    if(bound.isEmpty())return;
    const QJSValue handler=m_engine->globalObject().property(bound);
    if(!handler.isCallable()) {
        emit scriptDiagnostic(m_scriptName,0,0,QStringLiteral("Panel event %1.%2").arg(id,name),
            QStringLiteral("Assigned handler %1 is missing or not callable").arg(bound),{});
        return;
    }
    m_executingJs.store(true);
    // The function object is compiled from its original named source file.
    // No dynamic eval or anonymous wrappers; preserve file/line/stack on errors.
    const QJSValue outcome=handler.call(QJSValueList{ScriptValueConversion::toJavaScript(m_engine,value)});
    m_executingJs.store(false);
    if(m_stopRequested.load() || m_engine->isInterrupted())return;
    if(outcome.isError())reportError(QStringLiteral("Panel event %1.%2 (%3)").arg(id,name,bound),outcome);
}
void PanelRuntime::cleanup() {
    if(m_testRunner)m_testRunner->shutdown(); // cancel native timers before QJSEngine deletion
    if(m_logic)m_logic->stop();
    if(m_patterns && !m_patterns->stop())
        emit printed(QStringLiteral("WARNING: Patterns DigitalOut shutdown not confirmed: %1").arg(m_patterns->lastError()));
    if(m_staticIO && !m_staticIO->stop())
        emit printed(QStringLiteral("WARNING: StaticIO high-Z shutdown not confirmed: %1").arg(m_staticIO->lastError()));
    if(m_supplies)m_supplies->stop();
    if(m_wavegen)m_wavegen->stop();
    // Destroy C++ QObject wrappers before QJSEngine and DwfRuntime, on this worker thread.
    delete m_libraries;m_libraries=nullptr;
    delete m_utilities;m_utilities=nullptr;
    delete m_device;m_device=nullptr;
    delete m_logic;m_logic=nullptr;
    delete m_patterns;m_patterns=nullptr;
    delete m_staticIO;m_staticIO=nullptr;
    delete m_supplies;m_supplies=nullptr;
    delete m_wavegen;m_wavegen=nullptr;
    delete m_plots;m_plots=nullptr;
    delete m_scope;m_scope=nullptr;
    delete m_console;m_console=nullptr;
    delete m_testRunner;m_testRunner=nullptr;
    delete m_ui;m_ui=nullptr;
    {QMutexLocker lock(&m_engineMutex);delete m_engine;m_engine=nullptr;}
    if(m_runtime){m_runtime->close();m_runtime.reset();}
}
void PanelRuntime::stop() {
    finishStop();
}
