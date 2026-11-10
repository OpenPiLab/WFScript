#pragma once

#include "DwfRuntime.h"
#include "WaveFormsBindings.h"
#include "ScopeFrame.h"
#include "plot/PlotCommand.h"
#include "PanelProject.h"
#include "PanelUiBridge.h"
#include <QObject>
#include <QJSEngine>
#include <QHash>
#include <QSet>
#include <QJsonArray>
#include <QMutex>
#include <QPointer>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <utility>

class PlotBridge;
class ConsoleBridge;
class ScriptLibraryBridge;
class TestRunnerBridge;

// One persistent JS engine and one DWF handle, both living on an isolated QThread.
// Never accesses QWidget pointers. UI changes cross by queued signals.
class PanelRuntime final : public QObject {
    Q_OBJECT
public:
    explicit PanelRuntime(QObject* parent=nullptr);
    void requestStop();
public slots:
    void start(const QString& script, const QVariantMap& initialValues,
               bool physical, int deviceIndex, int configIndex,
               const QString& projectFile = {}, const QVariantMap& embedded = {},
               const QString& scriptName = QStringLiteral("Panel Script.js"),
               const PanelLifecycleSettings& lifecycle = {},
               const QJsonArray& controls = {},
               const QJsonObject& testDefinition = {}, const QString& testLibrary = {},
               const QJsonObject& runInformation = {},
               const QVariantMap& runInformationInput = {},
               const QJsonObject& reportStorage = {});
    void eventFromPanel(const QString& id,const QString& name,const QVariant& value);
    void stop();
signals:
    void printed(const QString& text);
    void consoleClearRequested();
    void failed(const QString& error);
    void scriptDiagnostic(const QString& source, int line, int column,
                          const QString& context, const QString& message, const QString& stack);
    void ready();
    // Observable lifecycle; sent on the worker thread and delivered queued to the GUI.
    void lifecycleChanged(const QString& state, const QString& detail);
    void stopped();
    void controlUpdate(const QString& id,const QVariant& value);
    void scopeFrameReady(const ScopeFrame& frame);
    void plotCommandReady(const PlotCommand& command);
private:
    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_executingJs{false};
    std::atomic_bool m_interruptedByStop{false};
    std::atomic_int m_runnerJsActive{0}; // Timer-driven test callbacks also interruptible
    PanelLifecycleSettings m_lifecycle;
    std::unique_ptr<DwfRuntime> m_runtime;
    MockInstrumentState m_mock;
    QJSEngine* m_engine=nullptr;
    QMutex m_engineMutex; // protects pointer during cross-thread interrupt
    PanelUiBridge* m_ui=nullptr;
    TestRunnerBridge* m_testRunner=nullptr;
    QSet<QString> m_runInformationIds;
    ScopeBridge* m_scope=nullptr;
    PlotBridge* m_plots=nullptr;
    WavegenBridge* m_wavegen=nullptr;
    class PowerSuppliesBridge* m_supplies=nullptr;
    class StaticIOBridge* m_staticIO=nullptr;
    class PatternsBridge* m_patterns=nullptr;
    LogicBridge* m_logic=nullptr;
    DeviceBridge* m_device=nullptr;
    ScriptUtilities* m_utilities=nullptr;
    ScriptLibraryBridge* m_libraries=nullptr;
    QString m_scriptName;
    QHash<QString,QString> m_managedHandlers; // "id::event" -> global function name
    bool m_ready=false;  // worker-thread only: event callbacks require successful initialization
    bool m_stopped=false;  // worker-thread only: emit stopped exactly once
    ConsoleBridge* m_console=nullptr; // uses ScriptWorker.h
    void cleanup();
    void finishStop(); // worker-thread only, idempotent
    void failedStartup(const QString& reason);
    bool runInitializeHook();
    void runShutdownHook();
    void registerObjects(QJSEngine* engine);
    void reportError(const QString& context,const QJSValue& result,
                     const QStringList& frames = {});
};
