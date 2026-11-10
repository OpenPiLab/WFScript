#pragma once

#include <QObject>
#include <QJSValue>
#include <QVariantList>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>
#include <QSet>
#include <QDateTime>
#include <QJsonObject>
#include <functional>
#include <atomic>

class QJSEngine;

// v5.6.57: native cooperative Test Executive, on the Panel JavaScript worker
// thread. A single QJSEngine is shared with the Panel and its imported libraries.
// All scheduling, clocks, timeout decisions, state and result aggregation are C++.
// No nested event loops, GUI pointers, SDK handles, or second JS engine.
class TestRunnerBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
public:
    using UiWriter=std::function<bool(const QString&,const QVariant&)>;
    explicit TestRunnerBridge(QJSEngine* engine,UiWriter writer,
                              std::atomic_bool* stopRequested,QObject* parent=nullptr);
    ~TestRunnerBridge() override = default;

    void setProjectManaged(bool managed) {m_projectManaged=managed;}
    Q_INVOKABLE bool configure(const QJSValue& options);
    Q_INVOKABLE bool onFinished(const QJSValue& callback); // JS notification of terminal state
    Q_INVOKABLE bool add(const QJSValue& definition,const QJSValue& handlers=QJSValue());
    // v5.6.61: versioned, data-only, transactional sequence definitions.
    // Library functions are looked up by export name; no JavaScript eval.
    Q_INVOKABLE bool loadDefinition(const QJSValue& document,const QJSValue& library=QJSValue());
    Q_INVOKABLE bool loadDefinitionFile(const QString& filePath,const QJSValue& library=QJSValue());
    Q_INVOKABLE bool saveDefinition(const QString& filePath);
    Q_INVOKABLE QVariantMap definition() const {return m_loadedDefinition;}
    Q_INVOKABLE bool clear();
    Q_INVOKABLE bool reset();
    Q_INVOKABLE bool run();
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool abort(); // emergency: skips sequence cleanup
    Q_INVOKABLE bool terminate(); // cooperative stop: runs cleanup
    // Script-facing methods return genuine JS arrays/objects (not opaque QVariant wrappers).
    // Native C++ persistence and tests use the data accessors below.
    Q_INVOKABLE QJSValue report() const;
    QVariantMap reportData() const;
    // Status of the last automatic archive: disabled, pending, saved or error.
    // Read-only; the archived report remains independent of this status.
    Q_INVOKABLE QVariantMap archiveInfo() const {return m_archiveInfo;}
    void configureArchive(const QJsonObject& storage,bool physical=false,
                          int deviceIndex=-1,int configIndex=-1,const QString& projectName={},
                          const QString& deviceName={},const QString& deviceSerial={});
    // Explicit operator/script action only: never save on Panel startup.
    // Stores a completed run atomically as UTF-8 JSON or flat CSV.
    Q_INVOKABLE bool saveReport(const QString& filePath,const QString& format=QString());
    Q_INVOKABLE QString exportReport(const QString& format=QString());
    // v5.6.59: per-execution identification and C++-owned step-local data.
    // runInfo is set before a run; variables exist only for the active run.
    Q_INVOKABLE bool setRunInfo(const QJSValue& details);
    void configureRunInformation(const QJsonObject& schema);
    void setNativeRunInformation(const QVariantMap& values);
    Q_INVOKABLE QVariantMap runInfo() const {return m_runInfo;}
    Q_INVOKABLE QVariantMap pendingRunInfo() const {return m_runInformationInput;}
    Q_INVOKABLE bool setVariable(const QString& name,const QJSValue& value);
    Q_INVOKABLE QVariant variable(const QString& name) const {return m_variables.value(name);}
    Q_INVOKABLE QVariantMap variables() const {return m_variables;}
    Q_INVOKABLE bool measure(const QString& name,double value,double low,double high,
                             const QString& unit=QString(),const QString& resultWidget=QString());
    Q_INVOKABLE QVariantMap summary() const;
    Q_INVOKABLE QJSValue results() const;
    QVariantList resultsData() const;
    Q_INVOKABLE QString state() const {return m_state;}
    QString lastError() const {return m_pendingError.isEmpty()?m_lastError:m_pendingError;}
    // Native runtime shutdown: cancels the timer before destroying the engine.
    // Deliberately never calls JS cleanup after an engine interrupt.
    void shutdown();

signals:
    void stepException(const QString& stepId,const QJSValue& exception);
    void message(const QString& message);
    void scriptCallbackActive(bool active); // direct signal; Panel Stop can interrupt poll callbacks
    void sequenceFinished(const QVariantMap& finalSummary);

private:
    struct Step {
        QString id,name,description,mode=QStringLiteral("once"),
                group=QStringLiteral("main"),type=QStringLiteral("test");
        QJSValue start,poll,finish;
        int intervalMs=50,timeoutMs=0,durationMs=0;
        double limitLow=0,limitHigh=0;
        QString unit,resultWidget;
        QString status=QStringLiteral("pending"),detail;
        qint64 elapsedMs=-1;
        QString startedAtUtc,finishedAtUtc;
    };
    QJSEngine* m_engine=nullptr;
    UiWriter m_write;
    std::atomic_bool* m_stopRequested=nullptr;
    QTimer m_timer;
    QJSValue m_finishedHandler;
    QVector<Step> m_steps;
    QVariantList m_measurements;
    QVariantMap m_loadedDefinition;
    QVariantMap m_runInfo,m_variables;
    QJsonObject m_runInformationSchema;
    QJsonObject m_archiveSettings;
    QVariantMap m_archiveInfo;
    QString m_projectName,m_deviceName,m_deviceSerial;
    QString m_instrumentMode=QStringLiteral("unspecified");
    int m_deviceIndex=-1,m_configIndex=-1;
    QVariantMap m_runInformationInput; // native snapshots; never QJSValue across GUI threads
    QSet<QString> m_resultWidgets;
    QElapsedTimer m_runClock,m_stepClock;
    QString m_pendingError; // rejected pre-run input, never mutates the previous report
    QString m_state=QStringLiteral("idle"),m_lastError,
            m_executionId,m_startedAtUtc,m_finishedAtUtc,
            m_title=QStringLiteral("Automated Test"),m_sequenceId,
            m_tableId,m_logId,m_summaryId,m_activeDetail;
    bool m_projectManaged=false;
    bool m_stopOnFailure=true,m_running=false,m_sessionActive=false,
         m_abortRequested=false,m_activeFailed=false,m_activeInvalid=false,
         m_autoRun=false,m_pumping=false,m_shuttingDown=false,m_notifyingCompletion=false,
         m_cleanupMode=false,m_gracefulStopRequested=false,m_hadError=false;
    int m_totalTimeoutMs=0,m_active=-1,m_cursor=0,m_completed=0,m_stepPass=0,m_stepFail=0,
        m_actionDone=0,m_skipped=0,
        m_measurePass=0,m_measureFail=0,m_tickCount=0;
    qint64 m_elapsedMs=0,m_lastPublishMs=0;

    bool setError(const QString& error);
    bool stopping() const;
    void write(const QString& id,const QVariant& value);
    void log(const QString& level,const QString& detail,const QString& source=QStringLiteral("TestRunner"));
    QVariantList stepsAsVariant() const;
    void publishSteps();
    void patchStep(const Step& step);
    void publishSummary();
    void clearResults(bool publish);
    bool beginSession();
    void archiveFinishedRun();
    void pump();
    void startStep();
    void onTick();
    void scheduleTick();
    QJSValue invoke(const QJSValue& fn,const QJSValueList& args={});
    void completeStep(const QString& status,const QString& message=QString());
    void endSession(bool aborted=false);
    void goToCleanup(const QString& reason);
    int cleanupIndex() const;
    bool checkDeadlines();
};
