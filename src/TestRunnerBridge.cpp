#include "TestRunnerBridge.h"
#include "StructuredTestResults.h"
#include "TestSequenceLoops.h"
#include "TestSequenceCapacity.h"
#include "TestSequenceConditions.h"
#include "ScriptValueConversion.h"
#include "RunInformationSchema.h"
#include "TestReportStorage.h"
#include "TestResultsRepository.h"
#include <QCryptographicHash>
#include <QJSEngine>
#include <QDir>
#include <QFile>
#include <QJsonParseError>
#include <QJsonObject>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJSValue>
#include <QJSValueList>
#include <QVariantMap>
#include <QMetaType>
#include <QSet>
#include <QUuid>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <utility>

TestRunnerBridge::TestRunnerBridge(QJSEngine* engine,UiWriter writer,
                                   std::atomic_bool* stopRequested,QObject* parent)
    : QObject(parent),m_engine(engine),m_write(std::move(writer)),
      m_stopRequested(stopRequested) {
    // Qt 6.9 QJSEngine may not provide the ES globalThis alias by default.
    // Keep scripts portable while never overriding an existing engine global.
    if(m_engine) {
        QJSValue global=m_engine->globalObject();
        if(global.property(QStringLiteral("globalThis")).isUndefined())
            global.setProperty(QStringLiteral("globalThis"),global);
    }
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer); // requested 10ms is best effort, not hard realtime
    connect(&m_timer,&QTimer::timeout,this,&TestRunnerBridge::onTick);
}

bool TestRunnerBridge::setError(const QString& error) {
    m_lastError=error;
    if(m_running && m_active>=0)m_activeInvalid=true;
    emit message(QStringLiteral("TestRunner: ")+error);
    return false;
}
bool TestRunnerBridge::stopping() const {
    return m_shuttingDown || m_abortRequested || (m_stopRequested && m_stopRequested->load()) ||
           (m_engine && m_engine->isInterrupted());
}
void TestRunnerBridge::write(const QString& id,const QVariant& value) {
    if(!id.isEmpty() && m_write && !m_shuttingDown)m_write(id,value);
}
void TestRunnerBridge::log(const QString& level,const QString& detail,const QString& source) {
    write(m_logId,QVariantMap{{QStringLiteral("append"),
        QVariantMap{{QStringLiteral("level"),level},
                    {QStringLiteral("message"),detail.left(1000)},
                    {QStringLiteral("source"),source.left(120)}}}});
}
QVariantList TestRunnerBridge::stepsAsVariant() const {
    QVariantList rows;
    rows.reserve(m_steps.size());
    for(const Step& step:m_steps) {
        QVariantMap entry{{QStringLiteral("id"),step.id},{QStringLiteral("name"),step.name},
                          {QStringLiteral("description"),step.description},
                          {QStringLiteral("status"),step.status},
                          {QStringLiteral("message"),step.detail}};
        if(step.elapsedMs>=0)entry.insert(QStringLiteral("elapsedMs"),step.elapsedMs);
        rows.push_back(entry);
    }
    return rows;
}
void TestRunnerBridge::publishSteps() {
    const QVariantList all=stepsAsVariant();
    if(all.size()<=TestSequenceCapacity::visibleSequenceSteps) {
        write(m_sequenceId,all);return;
    }
    // One definition, then individual step patches. No re-sending sliding
    // windows: the QAbstractTableModel exposes all rows with a real scrollbar.
    write(m_sequenceId,QVariantMap{{QStringLiteral("virtualSteps"),all}});
}
void TestRunnerBridge::patchStep(const Step& step) {
    QVariantMap patch{{QStringLiteral("id"),step.id},
                      {QStringLiteral("status"),step.status},
                      {QStringLiteral("message"),step.detail}};
    if(step.elapsedMs>=0)patch.insert(QStringLiteral("elapsedMs"),step.elapsedMs);
    write(m_sequenceId,QVariantMap{{QStringLiteral("update"),patch}});
}
QVariantMap TestRunnerBridge::summary() const {
    const qint64 total=m_sessionActive?m_runClock.elapsed():m_elapsedMs;
    const qint64 step=(m_active>=0 && m_stepClock.isValid())?m_stepClock.elapsed():0;
    return QVariantMap{{QStringLiteral("title"),m_title},
        {QStringLiteral("state"),m_state},
        {QStringLiteral("totalSteps"),m_steps.size()},
        {QStringLiteral("completedSteps"),m_completed},
        {QStringLiteral("passedSteps"),m_stepPass},
        {QStringLiteral("failedSteps"),m_stepFail},
        {QStringLiteral("passedMeasurements"),m_measurePass},
        {QStringLiteral("failedMeasurements"),m_measureFail},
        {QStringLiteral("elapsedMs"),total},
        {QStringLiteral("stepElapsedMs"),step},
        {QStringLiteral("currentStepId"),m_active>=0?m_steps[m_active].id:QString()},
        {QStringLiteral("pollCount"),m_tickCount},
        {QStringLiteral("executionId"),m_executionId},
        {QStringLiteral("runInfo"),m_runInfo},
        {QStringLiteral("completedActions"),m_actionDone},
        {QStringLiteral("skippedSteps"),m_skipped},
        {QStringLiteral("phase"),m_cleanupMode?QStringLiteral("cleanup"):
            (m_active>=0?m_steps[m_active].group:QStringLiteral("main"))},
        {QStringLiteral("totalTimeoutMs"),m_totalTimeoutMs},
        {QStringLiteral("lastError"),m_lastError}};
}
// Run metadata is only primitive JSON-compatible data; the UUT/operator IDs
// belong to the run report, never to the Panel project or any global store.
void TestRunnerBridge::configureRunInformation(const QJsonObject& schema) {
    m_runInformationSchema=schema;
    m_runInformationInput=RunInformationSchema::defaults(schema);
}
void TestRunnerBridge::setNativeRunInformation(const QVariantMap& values) {
    // Pending fields can be edited even while an earlier run is executing.
    // beginSession() copies them into m_runInfo exactly once; that current-run
    // snapshot is never modified by subsequent operator input.
    for(auto it=values.cbegin();it!=values.cend();++it)
        if(m_runInformationInput.contains(it.key()))m_runInformationInput.insert(it.key(),it.value());
}
void TestRunnerBridge::configureArchive(const QJsonObject& storage,bool physical,
                                         int deviceIndex,int configIndex,const QString& projectName,
                                         const QString& deviceName,const QString& deviceSerial) {
    m_archiveSettings=storage;
    m_instrumentMode=physical?QStringLiteral("hardware"):QStringLiteral("simulation");
    m_deviceIndex=deviceIndex;
    m_configIndex=configIndex;
    m_projectName=projectName;
    m_deviceName=deviceName;
    m_deviceSerial=deviceSerial;
    m_archiveInfo=QVariantMap{{QStringLiteral("status"),TestReportStorage::enabled(storage)?QStringLiteral("idle"):QStringLiteral("disabled")},
        {QStringLiteral("autoSaveEnabled"),TestReportStorage::enabled(storage)},
        {QStringLiteral("directory"),TestReportStorage::directory(storage)}};
}
bool TestRunnerBridge::setRunInfo(const QJSValue& details) {
    if(!m_runInformationSchema.isEmpty()) {
        m_pendingError=QStringLiteral("Run Information fields are project-managed; edit them using the native widget");
        emit message(QStringLiteral("TestRunner: ")+m_pendingError);
        return false;
    }
    if(m_sessionActive || m_running || m_state!=QStringLiteral("idle"))
        return setError(QStringLiteral("setRunInfo requires an idle, reset executive"));
    if(!details.isObject() || details.isArray() || details.isCallable())
        return setError(QStringLiteral("setRunInfo() requires an object"));
    const QVariantMap data=details.toVariant(QJSValue::ConvertJSObjects).toMap();
    if(data.size()>16)return setError(QStringLiteral("runInfo supports at most 16 fields"));
    for(auto it=data.cbegin();it!=data.cend();++it) {
        if(it.key().isEmpty() || it.key().size()>40 ||
           !QRegularExpression(QStringLiteral("^[A-Za-z][A-Za-z0-9_]*$")).match(it.key()).hasMatch())
            return setError(QStringLiteral("Invalid runInfo field name: %1").arg(it.key()));
        const QVariant& v=it.value();
        if(v.metaType().id()!=QMetaType::QString &&
           v.metaType().id()!=QMetaType::Bool &&
           v.metaType().id()!=QMetaType::Int &&
           v.metaType().id()!=QMetaType::LongLong &&
           v.metaType().id()!=QMetaType::Double)
            return setError(QStringLiteral("runInfo fields must be strings, booleans or numbers"));
        if(v.metaType().id()==QMetaType::QString && v.toString().size()>200)
            return setError(QStringLiteral("runInfo strings must be <=200 characters"));
        if(v.metaType().id()==QMetaType::Double && !std::isfinite(v.toDouble()))
            return setError(QStringLiteral("runInfo numeric values must be finite"));
    }
    m_runInfo=data;
    m_lastError.clear();
    return true;
}
bool TestRunnerBridge::setVariable(const QString& name,const QJSValue& value) {
    if(!m_sessionActive || !m_running || m_active<0)
        return setError(QStringLiteral("setVariable() requires an active step handler"));
    if(name.isEmpty() || name.size()>64 ||
       !QRegularExpression(QStringLiteral("^[A-Za-z][A-Za-z0-9_]*$")).match(name).hasMatch())
        return setError(QStringLiteral("Invalid shared variable name"));
    if(!m_variables.contains(name) && m_variables.size()>=64)
        return setError(QStringLiteral("Maximum 64 shared variables per run"));
    const QVariant v=value.toVariant(QJSValue::ConvertJSObjects);
    const auto type=v.metaType().id();
    for(const QJsonValue& declaration:m_declaredVariables) {
        const QJsonObject d=declaration.toObject();
        if(d.value(QStringLiteral("name")).toString()!=name)continue;
        const QString expected=d.value(QStringLiteral("type")).toString();
        const bool valid=expected==QLatin1String("boolean")?type==QMetaType::Bool:
            expected==QLatin1String("string")?type==QMetaType::QString:
            (type==QMetaType::Double||type==QMetaType::Int||type==QMetaType::LongLong);
        if(!valid)return setError(QStringLiteral("Variable '%1' requires type %2").arg(name,expected));
    }
    if(type!=QMetaType::Bool && type!=QMetaType::QString &&
       type!=QMetaType::Double && type!=QMetaType::Int &&
       type!=QMetaType::LongLong)
        return setError(QStringLiteral("Shared variable value must be a string, boolean or finite number"));
    if(type==QMetaType::QString && v.toString().size()>500)
        return setError(QStringLiteral("Shared variable string exceeds 500 characters"));
    if(type==QMetaType::Double && !std::isfinite(v.toDouble()))
        return setError(QStringLiteral("Shared variable must be finite"));
    m_variables.insert(name,v);
    return true;
}
QVariantList TestRunnerBridge::resultsData() const {return m_measurements;}
QJSValue TestRunnerBridge::results() const {
    return ScriptValueConversion::toJavaScript(m_engine,resultsData());
}
QJSValue TestRunnerBridge::report() const {
    return ScriptValueConversion::toJavaScript(m_engine,reportData());
}
QVariantMap TestRunnerBridge::reportData() const {
    QVariantList records;
    records.reserve(m_steps.size());
    for(const Step& s:std::as_const(m_steps)) {
        QVariantMap record{{QStringLiteral("id"),s.id},
            {QStringLiteral("name"),s.name},{QStringLiteral("group"),s.group},
            {QStringLiteral("type"),s.type},{QStringLiteral("mode"),s.mode},
            {QStringLiteral("status"),s.status},{QStringLiteral("detail"),s.detail},
            {QStringLiteral("elapsedMs"),s.elapsedMs},
            {QStringLiteral("startedAtUtc"),s.startedAtUtc},
            {QStringLiteral("finishedAtUtc"),s.finishedAtUtc}};
        if(!s.parameters.isEmpty())record.insert(QStringLiteral("parameters"),s.parameters);
        if(!s.templateId.isEmpty())record.insert(QStringLiteral("templateId"),s.templateId);
        if(!s.measurementRules.isEmpty())record.insert(QStringLiteral("measurementsConfig"),s.measurementRules);
        if(!s.condition.isEmpty())record.insert(QStringLiteral("condition"),s.condition);
        if(s.hasResult)record.insert(QStringLiteral("result"),s.result);
        records.push_back(record);
    }
    return QVariantMap{{QStringLiteral("schemaVersion"),1},
        {QStringLiteral("executionId"),m_executionId},
        {QStringLiteral("title"),m_title},{QStringLiteral("state"),m_state},
        {QStringLiteral("startedAtUtc"),m_startedAtUtc},
        {QStringLiteral("finishedAtUtc"),m_finishedAtUtc},
        {QStringLiteral("elapsedMs"),m_sessionActive?m_runClock.elapsed():m_elapsedMs},
        {QStringLiteral("summary"),summary()},
        {QStringLiteral("steps"),records},
        {QStringLiteral("measurements"),m_measurements},
        {QStringLiteral("runInfo"),m_runInfo},
        {QStringLiteral("definitionId"),m_loadedDefinition.value(QStringLiteral("id"))},
        {QStringLiteral("definitionSha256"),m_loadedDefinition.isEmpty()?QString():
             QString::fromLatin1(QCryptographicHash::hash(
                 QJsonDocument::fromVariant(m_loadedDefinition).toJson(QJsonDocument::Compact),
                 QCryptographicHash::Sha256).toHex())},
        {QStringLiteral("instrument"),QVariantMap{
             {QStringLiteral("mode"),m_instrumentMode},
             {QStringLiteral("deviceIndex"),m_deviceIndex},
             {QStringLiteral("configIndex"),m_configIndex},
             {QStringLiteral("name"),m_deviceName},
             {QStringLiteral("serial"),m_deviceSerial}}},
        {QStringLiteral("projectName"),m_projectName},
        {QStringLiteral("variables"),m_variables}};
}
// CSV escaping doubles quotes and quotes all fields. Protect user-provided
// labels and identifiers from spreadsheet-formula injection on CSV import.
namespace {
QString csvCell(QString value,bool untrusted=true) {
    if(untrusted && !value.isEmpty()) {
        const QChar first=value.at(0);
        if(first==QLatin1Char('=') || first==QLatin1Char('+') ||
           first==QLatin1Char('-') || first==QLatin1Char('@') ||
           first==QLatin1Char('\t') || first==QLatin1Char('\r'))
            value.prepend(QLatin1Char('\''));
    }
    value.replace(QLatin1Char('"'),QStringLiteral("\"\""));
    return QLatin1Char('"')+value+QLatin1Char('"');
}
QString safeReportSlug(QString value) {
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),QStringLiteral("_"));
    return value.left(48);
}
}
bool TestRunnerBridge::saveReport(const QString& filePath,const QString& format) {
    if(m_sessionActive || m_running || !QStringList{
        QStringLiteral("passed"),QStringLiteral("failed"),
        QStringLiteral("error"),QStringLiteral("aborted")}.contains(m_state) ||
       m_executionId.isEmpty())
        return setError(QStringLiteral("Complete a test execution before exporting its report"));
    const QString kind=format.trimmed().isEmpty()?QStringLiteral("json"):format.trimmed().toLower();
    if(kind!=QStringLiteral("json") && kind!=QStringLiteral("csv"))
        return setError(QStringLiteral("Report format must be 'json' or 'csv'"));
    if(filePath.trimmed().isEmpty() || filePath.contains(QChar::Null))
        return setError(QStringLiteral("A valid destination file path is required"));
    const QFileInfo destination(filePath);
    if(destination.suffix().compare(kind,Qt::CaseInsensitive)!=0)
        return setError(QStringLiteral("Destination extension must match report format .%1").arg(kind));
    const QString folder=destination.absolutePath();
    if(!QDir(folder).exists())
        return setError(QStringLiteral("Report folder does not exist: %1").arg(folder));
    QByteArray content;
    const QVariantMap record=reportData();
    if(kind==QStringLiteral("json")) {
        content=QJsonDocument::fromVariant(record).toJson(QJsonDocument::Indented);
    } else {
        const QStringList headings={QStringLiteral("recordType"),QStringLiteral("executionId"),
            QStringLiteral("uutId"),QStringLiteral("stepId"),QStringLiteral("group"),
            QStringLiteral("type"),QStringLiteral("name"),QStringLiteral("status"),
            QStringLiteral("elapsedMs"),QStringLiteral("startedAtUtc"),QStringLiteral("finishedAtUtc"),
            QStringLiteral("value"),QStringLiteral("low"),QStringLiteral("high"),QStringLiteral("unit")};
        QStringList rows;
        rows<<headings.join(QLatin1Char(','));
        const QString runId=m_executionId;
        const QString uut=m_runInfo.value(QStringLiteral("uutId")).toString();
        const auto row=[&](const QString& recordType,const QString& stepId,
                const QString& group,const QString& type,const QString& name,
                const QString& status,const QString& elapsedMs,
                const QString& started,const QString& finished,
                const QString& value,const QString& low,
                const QString& high,const QString& unit) {
            const QStringList fields={csvCell(recordType),csvCell(runId),csvCell(uut),
                csvCell(stepId),csvCell(group),csvCell(type),csvCell(name),
                csvCell(status),csvCell(elapsedMs,false),csvCell(started),csvCell(finished),
                csvCell(value,false),csvCell(low,false),csvCell(high,false),csvCell(unit)};
            rows<<fields.join(QLatin1Char(','));
        };
        for(const QVariant& item:record.value(QStringLiteral("steps")).toList()) {
            const QVariantMap m=item.toMap();
            row(QStringLiteral("STEP"),m.value(QStringLiteral("id")).toString(),
                m.value(QStringLiteral("group")).toString(),m.value(QStringLiteral("type")).toString(),
                m.value(QStringLiteral("name")).toString(),m.value(QStringLiteral("status")).toString(),
                m.value(QStringLiteral("elapsedMs")).toString(),
                m.value(QStringLiteral("startedAtUtc")).toString(),
                m.value(QStringLiteral("finishedAtUtc")).toString(),{},{},{},{});
        }
        for(const QVariant& item:record.value(QStringLiteral("measurements")).toList()) {
            const QVariantMap m=item.toMap();
            row(QStringLiteral("MEASUREMENT"),m.value(QStringLiteral("stepId")).toString(),
                {},QStringLiteral("test"),m.value(QStringLiteral("name")).toString(),
                m.value(QStringLiteral("status")).toString(),{}, {}, {},
                m.value(QStringLiteral("value")).toString(),m.value(QStringLiteral("low")).toString(),
                m.value(QStringLiteral("high")).toString(),m.value(QStringLiteral("unit")).toString());
        }
        content=rows.join(QLatin1Char('\n')).toUtf8()+"\n";
    }
    QSaveFile file(destination.absoluteFilePath());
    if(!file.open(QIODevice::WriteOnly))
        return setError(QStringLiteral("Cannot open report: %1").arg(file.errorString()));
    if(file.write(content)!=content.size()) {
        const QString error=file.errorString();file.cancelWriting();
        return setError(QStringLiteral("Cannot write report: %1").arg(error));
    }
    if(!file.commit())
        return setError(QStringLiteral("Cannot commit report: %1").arg(file.errorString()));
    m_lastError.clear();
    emit message(QStringLiteral("TestRunner report saved: ")+destination.absoluteFilePath());
    return true;
}
QString TestRunnerBridge::exportReport(const QString& format) {
    const QString kind=format.trimmed().isEmpty()?QStringLiteral("json"):format.trimmed().toLower();
    if(kind!=QStringLiteral("json") && kind!=QStringLiteral("csv")) {
        setError(QStringLiteral("Report format must be 'json' or 'csv'"));return {};
    }
    if(m_sessionActive || m_executionId.isEmpty() || m_state==QStringLiteral("idle") ||
       m_state==QStringLiteral("running")) {
        setError(QStringLiteral("Complete a test execution before exporting its report"));return {};
    }
    const QString documents=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if(documents.isEmpty()) {setError(QStringLiteral("Documents folder is unavailable"));return {};}
    QDir destination(documents);
    if(!destination.mkpath(QStringLiteral("WFScript Reports"))) {
        setError(QStringLiteral("Cannot create WFScript Reports folder"));return {};
    }
    const QString uut=safeReportSlug(m_runInfo.value(QStringLiteral("uutId")).toString());
    const QString name=QStringLiteral("WFScript_%1_%2.%3")
        .arg(uut.isEmpty()?QStringLiteral("UUT"):uut,m_executionId,kind);
    const QString filePath=destination.filePath(QStringLiteral("WFScript Reports/")+name);
    if(!saveReport(filePath,kind))return {};
    return filePath;
}
void TestRunnerBridge::publishSummary() {
    const bool bad=m_state==QStringLiteral("failed") || m_state==QStringLiteral("error") ||
                   m_state==QStringLiteral("aborted");
    const QString color=bad?QStringLiteral("#ff777e"):QStringLiteral("#43dd9d");
    const QString heading=m_state==QStringLiteral("passed")?QStringLiteral("TEST PASSED"):
                          m_state==QStringLiteral("running")?QStringLiteral("TEST RUNNING"):
                          m_state==QStringLiteral("idle")?QStringLiteral("READY"):
                          m_state==QStringLiteral("aborted")?QStringLiteral("TEST ABORTED"):
                          QStringLiteral("TEST FAILED");
    const QString details=QStringLiteral("%1/%2 steps complete · %3 PASS, %4 FAIL measurements · %5 ms")
        .arg(m_completed).arg(m_steps.size()).arg(m_measurePass).arg(m_measureFail)
        .arg(m_sessionActive?m_runClock.elapsed():m_elapsedMs);
    const QString reason=m_lastError.isEmpty()?QString():
        QStringLiteral("<br><b>Reason: %1</b>").arg(m_lastError.toHtmlEscaped());
    const QString skipped=m_skipped>0?QStringLiteral(" · %1 skipped").arg(m_skipped):QString();
    const QString visible=m_measurements.size()>TestSequenceCapacity::visibleMeasurementRows?
        QStringLiteral("<br>%1 measurements available in the scrollable table and archived report")
            .arg(m_measurements.size()):QString();
    write(m_summaryId,QStringLiteral("<b><span style=\"color:%1;\">%2</span></b><br>%3%4%5%6")
        .arg(color,heading,(details+skipped).toHtmlEscaped(),reason,visible,QString()));
    // The virtual sequence model holds all step rows; global progress remains
    // separate from widget geometry and native measurement archiving.
    write(m_sequenceId,QVariantMap{{QStringLiteral("progress"),QVariantMap{
        {QStringLiteral("total"),m_steps.size()},
        {QStringLiteral("completed"),m_completed},
        {QStringLiteral("state"),m_state}}}});
}
void TestRunnerBridge::clearResults(bool publish) {
    m_timer.stop();
    m_state=QStringLiteral("idle");m_lastError.clear();m_pendingError.clear();m_elapsedMs=0;
    m_sessionActive=false;m_running=false;m_active=-1;m_cursor=0;
    m_cleanupMode=false;m_gracefulStopRequested=false;m_hadError=false;
    m_executionId.clear();m_startedAtUtc.clear();m_finishedAtUtc.clear();
    m_archiveInfo=QVariantMap{{QStringLiteral("status"),QStringLiteral("idle")},
        {QStringLiteral("autoSaveEnabled"),TestReportStorage::enabled(m_archiveSettings)},
        {QStringLiteral("directory"),TestReportStorage::directory(m_archiveSettings)}};
    m_completed=0;m_stepPass=0;m_stepFail=0;m_measurePass=0;m_measureFail=0;
    m_actionDone=0;m_skipped=0;
    m_activeFailed=false;m_activeInvalid=false;m_abortRequested=false;
    m_autoRun=false;m_tickCount=0;m_lastPublishMs=0;m_activeDetail.clear();
    m_stepToken.clear();m_stepCompletionQueued=false;
    m_measurements.clear();
    m_sequenceWindowStart=0;
    m_variables.clear(); // shared data is per-run, including fresh beginSession()
    m_conditionOutcomes.clear();
    for(Step& step:m_steps){step.status=QStringLiteral("pending");step.detail.clear();step.elapsedMs=-1;
        step.startedAtUtc.clear();step.finishedAtUtc.clear();step.result.clear();step.hasResult=false;}
    if(publish){
        publishSteps();
        write(m_tableId,QVariantMap{{QStringLiteral("reset"),true}});
        write(m_logId,QVariantMap{{QStringLiteral("clear"),true}});
        for(const QString& id:std::as_const(m_resultWidgets))
            write(id,QVariantMap{{QStringLiteral("reset"),true}});
        publishSummary();
    }
}
bool TestRunnerBridge::onFinished(const QJSValue& callback) {
    if(!callback.isCallable() && !callback.isNull() && !callback.isUndefined())
        return setError(QStringLiteral("onFinished() requires a function or null to clear"));
    m_finishedHandler=callback;
    return true;
}
bool TestRunnerBridge::configure(const QJSValue& options) {
    if(m_projectManaged)return setError(QStringLiteral("configure is disabled: edit the test in the Project Test Sequence Editor"));
    if(m_running || m_sessionActive)return setError(QStringLiteral("Cannot configure while a sequence is active"));
    if(!options.isObject() || options.isArray() || options.isCallable())
        return setError(QStringLiteral("configure() expects a settings object"));
    const QVariantMap map=options.toVariant(QJSValue::ConvertJSObjects).toMap();
    for(const char* key:{"title","sequenceId","tableId","logId","summaryId"}) {
        const QString k=QString::fromLatin1(key);
        if(map.contains(k) && map.value(k).metaType().id()!=QMetaType::QString)
            return setError(QStringLiteral("Property %1 must be a string").arg(k));
    }
    if(map.contains(QStringLiteral("stopOnFailure")) &&
       map.value(QStringLiteral("stopOnFailure")).metaType().id()!=QMetaType::Bool)
        return setError(QStringLiteral("stopOnFailure must be boolean"));
    if(map.contains(QStringLiteral("totalTimeoutMs"))) {
        bool ok=false;
        const double timeout=map.value(QStringLiteral("totalTimeoutMs")).toDouble(&ok);
        if(!ok || !std::isfinite(timeout) || std::floor(timeout)!=timeout ||
           timeout<0 || timeout>86400000)
            return setError(QStringLiteral("totalTimeoutMs must be an integer 0..86400000 (0 disables)"));
    }
    auto getString=[&map](const char* key,const QString& old) {
        const QString k=QString::fromLatin1(key);
        return map.contains(k)?map.value(k).toString().trimmed():old;
    };
    const QString title=getString("title",m_title);
    const QString seq=getString("sequenceId",m_sequenceId),table=getString("tableId",m_tableId);
    const QString logId=getString("logId",m_logId),sum=getString("summaryId",m_summaryId);
    if(title.size()>120 || seq.size()>100 || table.size()>100 || logId.size()>100 || sum.size()>100)
        return setError(QStringLiteral("Title or target widget ID is too long"));
    m_title=title;m_sequenceId=seq;m_tableId=table;m_logId=logId;m_summaryId=sum;
    if(map.contains(QStringLiteral("stopOnFailure")))m_stopOnFailure=map.value(QStringLiteral("stopOnFailure")).toBool();
    if(map.contains(QStringLiteral("totalTimeoutMs")))m_totalTimeoutMs=map.value(QStringLiteral("totalTimeoutMs")).toInt();
    m_lastError.clear();publishSteps();publishSummary();return true;
}
// v5.6.61: load a declarative definition and resolve exported handlers in
// one QJSEngine. Validate with the existing native configure/add validators in
// a silent staging executive. Only commit if the *entire* document is valid;
// failed imports preserve the previous runnable sequence and its result record.
bool TestRunnerBridge::loadDefinition(const QJSValue& document,const QJSValue& library) {
    if(m_projectManaged)return setError(QStringLiteral("loadDefinition is disabled: edit the test in the Project Test Sequence Editor"));
    if(m_sessionActive || m_running || m_notifyingCompletion)
        return setError(QStringLiteral("Cannot load a test definition during execution"));
    if(!document.isObject() || document.isArray() || document.isCallable())
        return setError(QStringLiteral("Definition must be an object"));
    const QVariant raw=document.toVariant(QJSValue::ConvertJSObjects);
    if(raw.metaType().id()!=QMetaType::QVariantMap)
        return setError(QStringLiteral("Definition must contain plain JSON data"));
    const QVariantMap data=raw.toMap();
    const QSet<QString> validTop={QStringLiteral("schemaVersion"),QStringLiteral("id"),
        QStringLiteral("title"),QStringLiteral("config"),QStringLiteral("steps"),QStringLiteral("variables")};
    for(auto it=data.cbegin();it!=data.cend();++it)
        if(!validTop.contains(it.key()))
            return setError(QStringLiteral("Unknown definition property: %1").arg(it.key()));
    const QVariant schema=data.value(QStringLiteral("schemaVersion"));
    if(!schema.isValid() || !QSet<int>{QMetaType::Int,QMetaType::LongLong,
       QMetaType::Double}.contains(schema.metaType().id()) || schema.toDouble()!=1.0)
        return setError(QStringLiteral("Unsupported definition schemaVersion (expected integer 1)"));
    const QString id=data.value(QStringLiteral("id")).toString();
    if(data.value(QStringLiteral("id")).metaType().id()!=QMetaType::QString ||
       !QRegularExpression(QStringLiteral("^[A-Za-z][A-Za-z0-9_-]{0,63}$")).match(id).hasMatch())
        return setError(QStringLiteral("Definition id must be 1..64 ASCII letters/digits/_/- starting with a letter"));
    if(data.value(QStringLiteral("title")).metaType().id()!=QMetaType::QString ||
       data.value(QStringLiteral("title")).toString().trimmed().isEmpty() ||
       data.value(QStringLiteral("title")).toString().size()>120)
        return setError(QStringLiteral("Definition title must be a nonempty string <=120 characters"));
    if(data.value(QStringLiteral("config")).metaType().id()!=QMetaType::QVariantMap)
        return setError(QStringLiteral("Definition config must be an object"));
    if(data.value(QStringLiteral("steps")).metaType().id()!=QMetaType::QVariantList)
        return setError(QStringLiteral("Definition steps must be an array"));
    const QJsonArray declared=QJsonObject::fromVariantMap(data).value(QStringLiteral("variables")).toArray();
    QString variableError;
    if(data.contains(QStringLiteral("variables")) &&
       (!QJsonObject::fromVariantMap(data).value(QStringLiteral("variables")).isArray() ||
        !TestSequenceConditions::validateVariables(declared,&variableError)))
        return setError(QStringLiteral("Test variables: ")+variableError);
    // Keep the authored compact definition intact. Expand bounded Main loops
    // in one transactional stage, then reuse all existing native validators.
    QJsonObject expanded;
    QString expandError;
    if(!TestSequenceLoops::expand(QJsonObject::fromVariantMap(data),&expanded,&expandError))
        return setError(QStringLiteral("Test loops: %1").arg(expandError));
    const QVariantList entries=expanded.value(QStringLiteral("steps")).toArray().toVariantList();
    if(entries.isEmpty() || entries.size()>TestSequenceCapacity::maxExpandedSteps)
        return setError(QStringLiteral("Expanded definition requires 1..%1 steps").arg(TestSequenceCapacity::maxExpandedSteps));
    QVariantMap config=data.value(QStringLiteral("config")).toMap();
    const QSet<QString> validConfig={QStringLiteral("title"),QStringLiteral("sequenceId"),
        QStringLiteral("tableId"),QStringLiteral("logId"),QStringLiteral("summaryId"),
        QStringLiteral("stopOnFailure"),QStringLiteral("totalTimeoutMs")};
    for(auto it=config.cbegin();it!=config.cend();++it)
        if(!validConfig.contains(it.key()))
            return setError(QStringLiteral("Unknown definition config property: %1").arg(it.key()));
    // The document title is authoritative; avoid two independent titles.
    if(config.contains(QStringLiteral("title")) && config.value(QStringLiteral("title"))!=data.value(QStringLiteral("title")))
        return setError(QStringLiteral("Definition title conflicts with config.title"));
    config.insert(QStringLiteral("title"),data.value(QStringLiteral("title")));
    if(!m_engine)return setError(QStringLiteral("No JavaScript runtime available"));
    TestRunnerBridge staging(m_engine,UiWriter{},m_stopRequested);
    staging.m_steps.reserve(entries.size());
    if(!staging.configure(m_engine->toScriptValue(config)))
        return setError(QStringLiteral("Definition config: %1").arg(staging.lastError()));
    const QSet<QString> validStep={QStringLiteral("id"),QStringLiteral("name"),
        QStringLiteral("description"),QStringLiteral("group"),QStringLiteral("type"),
        QStringLiteral("mode"),QStringLiteral("timeoutMs"),QStringLiteral("intervalMs"),
        QStringLiteral("durationMs"),QStringLiteral("handler"),QStringLiteral("handlers"),
        QStringLiteral("limits"),QStringLiteral("unit"),QStringLiteral("resultWidget"),
        QStringLiteral("parameters"),QStringLiteral("measurements"),QStringLiteral("templateId"),
        QStringLiteral("condition"),QStringLiteral("guards")};
    const QSet<QString> validRoles={QStringLiteral("start"),QStringLiteral("poll"),QStringLiteral("finish")};
    const QRegularExpression exportName(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
    auto lookup=[&](const QString& symbol,int index)->QJSValue {
        if(!exportName.match(symbol).hasMatch() || !library.isObject()) {
            setError(QStringLiteral("Step %1: invalid or unavailable library export '%2'").arg(index+1).arg(symbol));
            return QJSValue();
        }
        const QJSValue value=library.property(symbol);
        if(!value.isCallable())
            setError(QStringLiteral("Step %1: missing callable library export '%2'").arg(index+1).arg(symbol));
        return value;
    };
    for(int i=0;i<entries.size();++i) {
        if(entries.at(i).metaType().id()!=QMetaType::QVariantMap)
            return setError(QStringLiteral("Step %1 must be an object").arg(i+1));
        const QVariantMap step=entries.at(i).toMap();
        for(auto it=step.cbegin();it!=step.cend();++it)
            if(!validStep.contains(it.key()))
                return setError(QStringLiteral("Step %1: unknown property '%2'").arg(i+1).arg(it.key()));
        if(step.contains(QStringLiteral("handler")) && step.contains(QStringLiteral("handlers")))
            return setError(QStringLiteral("Step %1: use handler or handlers, not both").arg(i+1));
        const QString mode=step.value(QStringLiteral("mode"),QStringLiteral("once")).toString().toLower();
        QVariantMap spec=step;
        spec.remove(QStringLiteral("handler"));spec.remove(QStringLiteral("handlers"));
        QJSValue callbacks;
        if(step.contains(QStringLiteral("handler"))) {
            if(step.value(QStringLiteral("handler")).metaType().id()!=QMetaType::QString ||
               (mode!=QStringLiteral("once") && mode!=QStringLiteral("callback")))
                return setError(QStringLiteral("Step %1: handler is a function name for once steps only").arg(i+1));
            callbacks=lookup(step.value(QStringLiteral("handler")).toString(),i);
            if(!callbacks.isCallable())return false;
        } else if(step.contains(QStringLiteral("handlers"))) {
            if(step.value(QStringLiteral("handlers")).metaType().id()!=QMetaType::QVariantMap)
                return setError(QStringLiteral("Step %1: handlers must be an object of export names").arg(i+1));
            const QVariantMap refs=step.value(QStringLiteral("handlers")).toMap();
            QJSValue resolved=m_engine->newObject();
            for(auto it=refs.cbegin();it!=refs.cend();++it) {
                if(!validRoles.contains(it.key()) || it.value().metaType().id()!=QMetaType::QString)
                    return setError(QStringLiteral("Step %1: handler roles must be start/poll/finish string names").arg(i+1));
                const QJSValue fn=lookup(it.value().toString(),i);
                if(!fn.isCallable())return false;
                resolved.setProperty(it.key(),fn);
            }
            callbacks=resolved;
        }
        if(!staging.add(m_engine->toScriptValue(spec),callbacks))
            return setError(QStringLiteral("Step %1: %2").arg(i+1).arg(staging.lastError()));
    }
    // All-or-nothing commit; no callbacks have executed or GUI state changed.
    m_steps=std::move(staging.m_steps);
    m_stepIds=std::move(staging.m_stepIds);
    m_title=staging.m_title;m_sequenceId=staging.m_sequenceId;
    m_tableId=staging.m_tableId;m_logId=staging.m_logId;m_summaryId=staging.m_summaryId;
    m_stopOnFailure=staging.m_stopOnFailure;m_totalTimeoutMs=staging.m_totalTimeoutMs;
    m_loadedDefinition=data;
    m_declaredVariables=declared;
    m_resultWidgets.clear();m_runInfo.clear();
    clearResults(true);
    return true;
}
bool TestRunnerBridge::loadDefinitionFile(const QString& filePath,const QJSValue& library) {
    if(m_sessionActive || m_running)return setError(QStringLiteral("Cannot import while a test is running"));
    if(!filePath.endsWith(QStringLiteral(".wftest.json"),Qt::CaseInsensitive))
        return setError(QStringLiteral("Definition file must end in .wftest.json"));
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
        return setError(QStringLiteral("Cannot open definition file: %1").arg(file.errorString()));
    if(file.size()>131072)return setError(QStringLiteral("Definition file exceeds 128 KiB"));
    const QByteArray bytes=file.read(131073);
    if(bytes.size()>131072)return setError(QStringLiteral("Definition file exceeds 128 KiB"));
    QJsonParseError parseError;
    const QJsonDocument parsed=QJsonDocument::fromJson(bytes,&parseError);
    if(parseError.error!=QJsonParseError::NoError || !parsed.isObject())
        return setError(QStringLiteral("Invalid definition JSON: %1").arg(parseError.errorString()));
    return loadDefinition(m_engine->toScriptValue(parsed.object().toVariantMap()),library);
}
bool TestRunnerBridge::saveDefinition(const QString& filePath) {
    if(m_sessionActive || m_running)return setError(QStringLiteral("Cannot export a definition while executing"));
    if(m_loadedDefinition.isEmpty())return setError(QStringLiteral("No declarative definition is loaded"));
    if(!filePath.endsWith(QStringLiteral(".wftest.json"),Qt::CaseInsensitive))
        return setError(QStringLiteral("Definition file must end in .wftest.json"));
    if(!QFileInfo(filePath).absoluteDir().exists())
        return setError(QStringLiteral("Definition parent folder does not exist"));
    QSaveFile file(filePath);
    if(!file.open(QIODevice::WriteOnly))
        return setError(QStringLiteral("Cannot write definition: %1").arg(file.errorString()));
    const QByteArray bytes=QJsonDocument::fromVariant(m_loadedDefinition).toJson(QJsonDocument::Indented);
    if(file.write(bytes)!=bytes.size() || !file.commit())
        return setError(QStringLiteral("Cannot commit definition file: %1").arg(file.errorString()));
    m_lastError.clear();return true;
}

bool TestRunnerBridge::add(const QJSValue& definition,const QJSValue& handlers) {
    if(m_projectManaged)return setError(QStringLiteral("add is disabled: edit the test in the Project Test Sequence Editor"));
    if(m_running || m_sessionActive)return setError(QStringLiteral("Cannot change steps during execution"));
    if(m_steps.size()>=TestSequenceCapacity::maxExpandedSteps)
        return setError(QStringLiteral("Maximum %1 steps").arg(TestSequenceCapacity::maxExpandedSteps));
    if(!definition.isObject() || definition.isArray() || definition.isCallable())
        return setError(QStringLiteral("add() requires a step definition object"));
    const QVariantMap map=definition.toVariant(QJSValue::ConvertJSObjects).toMap();
    for(const char* key:{"id","name"}) {
        if(map.value(QString::fromLatin1(key)).metaType().id()!=QMetaType::QString)
            return setError(QStringLiteral("Step %1 must be a string").arg(QString::fromLatin1(key)));
    }
    if(map.contains(QStringLiteral("description")) && map.value(QStringLiteral("description")).metaType().id()!=QMetaType::QString)
        return setError(QStringLiteral("Step description must be a string"));
    Step step;
    if(map.contains(QStringLiteral("guards"))) {
        if(map.value(QStringLiteral("guards")).metaType().id()!=QMetaType::QVariantList)
            return setError(QStringLiteral("Step guards must be an array"));
        step.guards=map.value(QStringLiteral("guards")).toList();
        for(const QVariant& guard:step.guards) {
            const QVariantMap g=guard.toMap();
            if(g.value(QStringLiteral("id")).toString().isEmpty() ||
                g.value(QStringLiteral("value")).metaType().id()!=QMetaType::Bool)
                return setError(QStringLiteral("Invalid conditional branch guard"));
        }
    }
    if(map.contains(QStringLiteral("condition"))) {
        if(map.value(QStringLiteral("condition")).metaType().id()!=QMetaType::QVariantMap)
            return setError(QStringLiteral("Condition must be an object"));
        step.condition=map.value(QStringLiteral("condition")).toMap();
        QString conditionError;
        if(!TestSequenceConditions::validate(QJsonObject::fromVariantMap(step.condition),m_declaredVariables,&conditionError))
            return setError(conditionError);
    }
    step.id=map.value(QStringLiteral("id")).toString().trimmed();
    step.name=map.value(QStringLiteral("name")).toString().trimmed();
    step.description=map.value(QStringLiteral("description")).toString();
    step.templateId=map.value(QStringLiteral("templateId")).toString();
    if(map.contains(QStringLiteral("parameters"))) {
        if(map.value(QStringLiteral("parameters")).metaType().id()!=QMetaType::QVariantMap)
            return setError(QStringLiteral("Step parameters must be a JSON object"));
        const QJsonDocument json=QJsonDocument::fromVariant(map.value(QStringLiteral("parameters")));
        if(!json.isObject() || json.toJson(QJsonDocument::Compact).size()>8192)
            return setError(QStringLiteral("Step parameters exceed the 8 KiB JSON limit"));
        step.parameters=map.value(QStringLiteral("parameters")).toMap();
    }
    if(map.contains(QStringLiteral("measurements"))) {
        if(map.value(QStringLiteral("measurements")).metaType().id()!=QMetaType::QVariantMap)
            return setError(QStringLiteral("Step measurements must be a named result rule object"));
        step.measurementRules=map.value(QStringLiteral("measurements")).toMap();
        QVector<StructuredTestResults::Rule> rules;
        QString error;
        if(!StructuredTestResults::parse(step.measurementRules,&rules,&error))
            return setError(error);
    }
    step.mode=map.value(QStringLiteral("mode"),QStringLiteral("once")).toString().trimmed().toLower();
    for(const char* k:{"group","type"}) {
        const QString key=QString::fromLatin1(k);
        if(map.contains(key) && map.value(key).metaType().id()!=QMetaType::QString)
            return setError(QStringLiteral("Step %1 must be a string").arg(key));
    }
    step.group=map.value(QStringLiteral("group"),QStringLiteral("main")).toString().trimmed().toLower();
    step.type=map.value(QStringLiteral("type"),QStringLiteral("test")).toString().trimmed().toLower();
    if(!QStringList{QStringLiteral("setup"),QStringLiteral("main"),QStringLiteral("cleanup")}.contains(step.group))
        return setError(QStringLiteral("Step group must be setup, main or cleanup"));
    if(!QStringList{QStringLiteral("action"),QStringLiteral("test"),QStringLiteral("numeric")}.contains(step.type))
        return setError(QStringLiteral("Step type must be action or test"));
    if(step.type==QStringLiteral("numeric")) {
        if(step.mode==QStringLiteral("delay"))
            return setError(QStringLiteral("Numeric measurement cannot use delay mode; use an Action delay then a numeric test"));
        const QVariantMap limits=map.value(QStringLiteral("limits")).toMap();
        if(!step.measurementRules.isEmpty()) {
            if(map.contains(QStringLiteral("limits")) || map.contains(QStringLiteral("unit")) ||
               map.contains(QStringLiteral("resultWidget")))
                return setError(QStringLiteral("Named measurements replace scalar limits, unit and resultWidget"));
        } else {
            if(map.value(QStringLiteral("limits")).metaType().id()!=QMetaType::QVariantMap ||
               !limits.contains(QStringLiteral("low")) || !limits.contains(QStringLiteral("high")))
                return setError(QStringLiteral("Numeric step requires limits {low,high} or named measurements"));
            bool lowOk=false,highOk=false;
            step.limitLow=limits.value(QStringLiteral("low")).toDouble(&lowOk);
            step.limitHigh=limits.value(QStringLiteral("high")).toDouble(&highOk);
            if(!lowOk || !highOk || !std::isfinite(step.limitLow) || !std::isfinite(step.limitHigh) ||
               step.limitLow>step.limitHigh)return setError(QStringLiteral("Invalid numeric limits"));
        }
        step.unit=map.value(QStringLiteral("unit")).toString();
        step.resultWidget=map.value(QStringLiteral("resultWidget")).toString();
        if(step.unit.size()>32 || step.resultWidget.size()>100)
            return setError(QStringLiteral("Numeric unit or result widget is too long"));
    }
    if(step.type!=QStringLiteral("numeric") && !step.measurementRules.isEmpty())
        return setError(QStringLiteral("Named measurement rules require a numeric step"));
    if(step.id.isEmpty() || step.id.size()>64 || step.name.isEmpty() || step.name.size()>120 ||
       step.description.size()>320)
        return setError(QStringLiteral("Invalid step ID, name or description length"));
    if(!QStringList{QStringLiteral("once"),QStringLiteral("poll"),QStringLiteral("delay"),
                    QStringLiteral("callback"),QStringLiteral("condition")}.contains(step.mode))
        return setError(QStringLiteral("Step mode must be once, poll or delay"));
    if(m_stepIds.contains(step.id))
        return setError(QStringLiteral("Duplicate step ID: %1").arg(step.id));
    auto readInteger=[&map,this](const QString& key,int min,int max,int fallback,int& dest)->bool {
        if(!map.contains(key)){dest=fallback;return true;}
        bool ok=false;
        const double n=map.value(key).toDouble(&ok);
        if(!ok || !std::isfinite(n) || std::floor(n)!=n || n<min || n>max) {
            setError(QStringLiteral("%1 must be an integer %2..%3").arg(key).arg(min).arg(max));
            return false;
        }
        dest=static_cast<int>(n);return true;
    };
    if(!readInteger(QStringLiteral("timeoutMs"),0,86400000,0,step.timeoutMs) ||
       !readInteger(QStringLiteral("intervalMs"),10,60000,50,step.intervalMs) ||
       !readInteger(QStringLiteral("durationMs"),0,86400000,0,step.durationMs))return false;
    if(step.mode==QStringLiteral("condition") &&
       (step.group!=QLatin1String("main") || step.type!=QLatin1String("action") || step.condition.isEmpty()))
        return setError(QStringLiteral("Conditional steps require action type in Main and a condition"));
    if(step.mode!=QStringLiteral("condition") && !step.condition.isEmpty())
        return setError(QStringLiteral("Condition config requires condition execution mode"));
    if(step.mode==QStringLiteral("delay") && step.durationMs<=0)
        return setError(QStringLiteral("Delay steps require durationMs greater than zero"));
    if(handlers.isCallable()) {
        step.start=handlers;
    } else if(handlers.isObject() && !handlers.isArray()) {
        step.start=handlers.property(QStringLiteral("start"));
        step.poll=handlers.property(QStringLiteral("poll"));
        step.finish=handlers.property(QStringLiteral("finish"));
        for(const QJSValue& callback:{step.start,step.poll,step.finish})
            if(!callback.isUndefined() && !callback.isNull() && !callback.isCallable())
                return setError(QStringLiteral("Step handler start/poll/finish must be functions"));
    } else if(!handlers.isUndefined() && !handlers.isNull()) {
        return setError(QStringLiteral("Step handler must be a function or {start,poll,finish}"));
    }
    if((step.mode==QStringLiteral("once") || step.mode==QStringLiteral("callback")) &&
       !step.start.isCallable())
        return setError(QStringLiteral("Once steps require a callable function"));
    if(step.mode==QStringLiteral("poll") && !step.poll.isCallable())
        return setError(QStringLiteral("Poll steps require {poll:function}; start and finish are optional"));
    if(step.mode==QStringLiteral("callback") && step.poll.isCallable())
        return setError(QStringLiteral("Callback steps do not use poll handlers"));
    // Delay-only steps may have optional start/finish handlers.
    // Stable insertion within Setup/Main/Cleanup groups independent of add() order.
    const auto rank=[](const QString& group){return group==QStringLiteral("setup")?0:
        (group==QStringLiteral("main")?1:2);};
    int insertAt=m_steps.size();
    // The collection is group-sorted; scanning from the end keeps the common
    // ordered expanded definition O(N), not O(N^2) for 10,000 steps.
    while(insertAt>0 && rank(m_steps.at(insertAt-1).group)>rank(step.group))--insertAt;
    m_steps.insert(insertAt,step);
    m_stepIds.insert(step.id);
    m_loadedDefinition.clear(); // manual edits no longer match the loaded document
    m_lastError.clear();publishSteps();publishSummary();return true;
}
bool TestRunnerBridge::clear() {
    if(m_projectManaged)return setError(QStringLiteral("clear is disabled: edit the test in the Project Test Sequence Editor"));
    if(m_running || m_sessionActive)return setError(QStringLiteral("Cannot clear steps while running"));
    m_steps.clear();m_stepIds.clear();m_loadedDefinition.clear();m_declaredVariables=QJsonArray{};clearResults(true);m_resultWidgets.clear();m_runInfo.clear();return true;
}
bool TestRunnerBridge::reset() {
    if(m_running)return setError(QStringLiteral("Cannot reset inside a step callback"));
    // End a pending step through its finish callback before clearing results.
    if(m_active>=0 && m_sessionActive)
        completeStep(QStringLiteral("aborted"),QStringLiteral("Reset requested"));
    clearResults(true);m_runInfo.clear();return true;
}
bool TestRunnerBridge::terminate() {
    if(!m_sessionActive)return setError(QStringLiteral("Test is not running"));
    m_gracefulStopRequested=true;
    if(!m_running) {
        if(m_active>=0 && !m_cleanupMode)
            completeStep(QStringLiteral("aborted"),QStringLiteral("Graceful termination requested"));
        else if(m_active<0)goToCleanup(QStringLiteral("Graceful termination requested"));
    }
    return true;
}
bool TestRunnerBridge::abort() {
    if(!m_sessionActive)return setError(QStringLiteral("Test is not running"));
    m_abortRequested=true;
    if(!m_running) {
        if(m_active>=0)completeStep(QStringLiteral("aborted"),QStringLiteral("Operator aborted the step"));
        else endSession(true);
    }
    return true;
}
bool TestRunnerBridge::measure(const QString& name,double value,double low,double high,
                               const QString& unit,const QString& resultWidget) {
    if(!m_running || m_active<0)return setError(QStringLiteral("measure() must be called inside a step callback"));
    if(name.trimmed().isEmpty() || name.size()>120 || unit.size()>32 || resultWidget.size()>100 ||
       !std::isfinite(value) || !std::isfinite(low) || !std::isfinite(high) || low>high) {
        m_activeInvalid=true;
        return setError(QStringLiteral("Invalid measurement name, numeric value, limits or unit"));
    }
    if(m_measurements.size()>=TestSequenceCapacity::maxMeasurementsPerRun)return setError(
        QStringLiteral("Maximum %1 measurements per test run").arg(TestSequenceCapacity::maxMeasurementsPerRun));
    const bool pass=value>=low && value<=high;
    m_measurements.push_back(QVariantMap{{QStringLiteral("name"),name},
        {QStringLiteral("value"),value},{QStringLiteral("low"),low},
        {QStringLiteral("high"),high},{QStringLiteral("unit"),unit},
        {QStringLiteral("stepId"),m_steps[m_active].id},
        {QStringLiteral("status"),pass?QStringLiteral("PASS"):QStringLiteral("FAIL")}});
    if(pass)++m_measurePass;else {++m_measureFail;m_activeFailed=true;}
    if(!resultWidget.isEmpty()) {
        m_resultWidgets.insert(resultWidget);
        write(resultWidget,value);
    }
    write(m_tableId,QVariantMap{{QStringLiteral("append"),m_measurements.last()}});
    m_activeDetail=QStringLiteral("%1: %2 %3 (%4 – %5 %3)")
        .arg(name).arg(value,0,'g',8).arg(unit).arg(low,0,'g',8).arg(high,0,'g',8);
    if(!pass)m_activeDetail+=QStringLiteral(" OUTSIDE LIMITS");
    log(pass?QStringLiteral("PASS"):QStringLiteral("FAIL"),m_activeDetail,QStringLiteral("Measurement"));
    return pass;
}
bool TestRunnerBridge::beginSession() {
    if(m_sessionActive)return true;
    if(m_steps.isEmpty())return setError(QStringLiteral("Add at least one step before running"));
    // Validate native operator fields BEFORE clearing the previous report or
    // activating the sequence. An incomplete UUT must never start hardware.
    if(!m_runInformationSchema.isEmpty()) {
        QVariantMap candidate;
        QString error;
        if(!RunInformationSchema::validateValues(m_runInformationSchema,m_runInformationInput,&candidate,&error)) {
            m_pendingError=QStringLiteral("Run Information: ")+error;
            emit message(QStringLiteral("TestRunner: ")+m_pendingError);
            return false; // preserve the previous completed record untouched
        }
        m_runInfo=candidate; // immutable snapshot for this execution
    }
    m_pendingError.clear();
    clearResults(true);
    for(const auto& entry:m_declaredVariables) {
        const QJsonObject d=entry.toObject();
        if(d.contains(QStringLiteral("initial")))
            m_variables.insert(d.value(QStringLiteral("name")).toString(),d.value(QStringLiteral("initial")).toVariant());
    }
    m_archiveInfo=QVariantMap{{QStringLiteral("status"),QStringLiteral("pending")},
        {QStringLiteral("autoSaveEnabled"),TestReportStorage::enabled(m_archiveSettings)},
        {QStringLiteral("directory"),TestReportStorage::directory(m_archiveSettings)}};
    m_sessionActive=true;m_state=QStringLiteral("running");m_runClock.start();
    m_executionId=QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_startedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    publishSummary();log(QStringLiteral("INFO"),m_title+QStringLiteral(" started"));
    return true;
}
QJSValue TestRunnerBridge::invoke(const QJSValue& fn,const QJSValueList& args) {
    // The owning PanelRuntime tracks this nested callback so GUI Stop can
    // interrupt JavaScript even when entered through a QTimer rather than a GUI event.
    m_running=true;
    emit scriptCallbackActive(true);
    const QJSValue value=fn.call(args);
    emit scriptCallbackActive(false);
    m_running=false;
    return value;
}
bool TestRunnerBridge::checkDeadlines() {
    if(m_active<0 || !m_sessionActive)return false;
    const Step& step=m_steps[m_active];
    if(!m_cleanupMode && m_totalTimeoutMs>0 && m_runClock.elapsed()>=m_totalTimeoutMs) {
        m_lastError=QStringLiteral("Total test timeout after %1 ms (limit %2 ms)")
            .arg(m_runClock.elapsed()).arg(m_totalTimeoutMs);
        completeStep(QStringLiteral("error"),m_lastError);return true;
    }
    if(step.timeoutMs>0 && m_stepClock.elapsed()>=step.timeoutMs) {
        m_lastError=QStringLiteral("Step %1 timed out after %2 ms (limit %3 ms)")
            .arg(step.id).arg(m_stepClock.elapsed()).arg(step.timeoutMs);
        completeStep(QStringLiteral("error"),m_lastError);return true;
    }
    return false;
}
void TestRunnerBridge::scheduleTick() {
    if(!m_sessionActive || m_active<0)return;
    const Step& step=m_steps[m_active];
    qint64 delay=step.mode==QStringLiteral("callback")?100:
        step.mode==QStringLiteral("delay")?
        std::max<qint64>(1,step.durationMs-m_stepClock.elapsed()):step.intervalMs;
    if(step.timeoutMs>0)
        delay=std::min(delay,std::max<qint64>(1,step.timeoutMs-m_stepClock.elapsed()));
    if(!m_cleanupMode && m_totalTimeoutMs>0)
        delay=std::min(delay,std::max<qint64>(1,m_totalTimeoutMs-m_runClock.elapsed()));
    m_timer.start(static_cast<int>(delay));
}
QJSValue TestRunnerBridge::stepContext(const Step& step) const {
    QJSValue context=m_engine->newObject();
    context.setProperty(QStringLiteral("id"),step.id);
    context.setProperty(QStringLiteral("name"),step.name);
    context.setProperty(QStringLiteral("token"),m_stepToken);
    context.setProperty(QStringLiteral("params"),m_engine->toScriptValue(step.parameters));
    context.setProperty(QStringLiteral("runId"),m_executionId);
    return context;
}

// Native, token-bound completion. No nested event loop and no reentrant
// completeStep() while a JS start handler is on the stack. Completion is
// processed on this same JS worker thread in the next Qt event turn.
bool TestRunnerBridge::isActive(const QString& token) const {
    return !token.isEmpty() && m_sessionActive && m_active>=0 &&
           m_steps.at(m_active).mode==QStringLiteral("callback") &&
           token==m_stepToken && !m_stepCompletionQueued &&
           !m_shuttingDown && !stopping() &&
           (!m_gracefulStopRequested || m_cleanupMode);
}
// A callback result is captured as a JSON-safe snapshot *at the time complete()
// is called*, not on the next event turn. This prevents script-side mutation
// between completion notification and evaluation.
bool TestRunnerBridge::complete(const QString& token,const QJSValue& value) {
    if(!isActive(token))return false;
    const Step& step=m_steps.at(m_active);
    const bool structured=value.isObject() && !value.isCallable();
    if(step.type==QStringLiteral("numeric")) {
        if(step.measurementRules.isEmpty()) {
            if(!value.isNumber() || !std::isfinite(value.toNumber()))return fail(token,
                QStringLiteral("Numeric callback result must be a finite number"));
        } else if(!structured) {
            return fail(token,QStringLiteral("Named measurement step requires a JSON object or array"));
        }
    } else if(!value.isUndefined() && !value.isNull() && !value.isBool() && !structured) {
        return fail(token,QStringLiteral("Action/test result must be boolean, undefined or JSON data"));
    }
    QVariant retained;
    if(structured) {
        retained=value.toVariant(QJSValue::ConvertJSObjects);
        QString error;
        if(!StructuredTestResults::validateResult(retained,&error))return fail(token,error);
    } else if(value.isNumber())retained=value.toNumber();
    else if(value.isBool())retained=value.toBool();
    // Null and undefined both mean an action completed with no returned data.
    m_stepCompletionQueued=true;
    QTimer::singleShot(0,this,[this,token,retained]{
        if(!m_sessionActive || m_active<0 || m_stepToken!=token || !m_stepCompletionQueued)return;
        m_stepCompletionQueued=false;
        if(stopping() || (m_gracefulStopRequested && !m_cleanupMode))return;
        if(checkDeadlines())return;
        finishStructuredResult(retained);
    });
    return true;
}
// Evaluate one snapshot, record every named measurement or none on a malformed
// result, and finish the step exactly once. Called only on the JS worker thread.
void TestRunnerBridge::finishStructuredResult(const QVariant& retained) {
    Step& current=m_steps[m_active];
    if(retained.isValid()) {
        current.result=retained;
        current.hasResult=true;
    }
    if(current.type==QStringLiteral("numeric")) {
        if(current.measurementRules.isEmpty()) {
            m_running=true;
            const bool passed=measure(current.name,retained.toDouble(),current.limitLow,
                                      current.limitHigh,current.unit,current.resultWidget);
            m_running=false;
            completeStep(m_activeInvalid?QStringLiteral("error"):
                         passed?QStringLiteral("pass"):QStringLiteral("fail"),m_activeDetail);
        } else {
            QVector<StructuredTestResults::Rule> rules;
            QVector<StructuredTestResults::Reading> results;
            QString error;
            if(!StructuredTestResults::parse(current.measurementRules,&rules,&error) ||
               !StructuredTestResults::evaluate(retained,rules,&results,&error) ||
               results.size()+m_measurements.size()>TestSequenceCapacity::maxMeasurementsPerRun) {
                if(error.isEmpty())error=QStringLiteral("Maximum %1 measurements per run").arg(TestSequenceCapacity::maxMeasurementsPerRun);
                m_lastError=QStringLiteral("Step %1: %2").arg(current.id,error);
                completeStep(QStringLiteral("error"),m_lastError);
                return;
            }
            // All result paths and numeric values are checked before any
            // measurement is appended; malformed results cannot create a
            // partially-passed step.
            int successes=0;
            // Batch native measurement delivery; the GUI model owns its full
            // incremental history while only painting visible Qt table rows.
            QVariantList tableBatch;
            tableBatch.reserve(results.size());
            for(const auto& reading:results) {
                const bool passed=reading.value>=reading.rule.low &&
                                  reading.value<=reading.rule.high;
                if(passed){++successes;++m_measurePass;}
                else {++m_measureFail;m_activeFailed=true;}
                m_measurements.push_back(QVariantMap{
                    {QStringLiteral("name"),reading.rule.name},
                    {QStringLiteral("value"),reading.value},
                    {QStringLiteral("low"),reading.rule.low},
                    {QStringLiteral("high"),reading.rule.high},
                    {QStringLiteral("unit"),reading.rule.unit},
                    {QStringLiteral("path"),reading.resolvedPath},
                    {QStringLiteral("stepId"),current.id},
                    {QStringLiteral("status"),passed?QStringLiteral("PASS"):QStringLiteral("FAIL")}});
                tableBatch.push_back(m_measurements.last());
                if(!reading.rule.resultWidget.isEmpty()) {
                    m_resultWidgets.insert(reading.rule.resultWidget);
                    write(reading.rule.resultWidget,reading.value);
                }
                const QString detail=QStringLiteral("%1 (%2): %3 %4 [%5 – %6]")
                    .arg(reading.rule.name,reading.resolvedPath)
                    .arg(reading.value,0,'g',8).arg(reading.rule.unit)
                    .arg(reading.rule.low,0,'g',8).arg(reading.rule.high,0,'g',8);
                log(passed?QStringLiteral("PASS"):QStringLiteral("FAIL"),
                    detail,QStringLiteral("Measurement"));
            }
            write(m_tableId,QVariantMap{{QStringLiteral("append"),tableBatch}});
            m_activeDetail=QStringLiteral("%1/%2 measurements passed")
                .arg(successes).arg(results.size());
            completeStep(m_activeInvalid?QStringLiteral("error"):
                         m_activeFailed?QStringLiteral("fail"):QStringLiteral("pass"),m_activeDetail);
        }
    } else {
        completeStep(retained.isValid() && retained.metaType().id()==QMetaType::Bool && !retained.toBool()
                     ?QStringLiteral("fail"):
                     current.type==QStringLiteral("action")?QStringLiteral("done"):
                     QStringLiteral("pass"));
    }
}
bool TestRunnerBridge::fail(const QString& token,const QString& reason) {
    if(!isActive(token))return false;
    m_stepCompletionQueued=true;
    const QString safeReason=reason.trimmed().isEmpty()?
        QStringLiteral("Asynchronous operation failed"):reason.left(300);
    QTimer::singleShot(0,this,[this,token,safeReason]{
        if(!m_sessionActive || m_active<0 || m_stepToken!=token || !m_stepCompletionQueued)return;
        m_stepCompletionQueued=false;
        if(stopping() || (m_gracefulStopRequested && !m_cleanupMode))return;
        m_lastError=QStringLiteral("Step %1: %2").arg(m_steps.at(m_active).id,safeReason);
        completeStep(QStringLiteral("error"),m_lastError);
    });
    return true;
}

bool TestRunnerBridge::resolveCondition(const Step& step,QVariant* out,QString* error) const {
    const QJsonObject c=QJsonObject::fromVariantMap(step.condition);
    const QString source=c.value(QStringLiteral("source")).toString();
    const QString key=c.value(QStringLiteral("key")).toString();
    if(source==QLatin1String("variable")) { *out=m_variables.value(key);return true; }
    if(source==QLatin1String("parameter")) {*out=step.parameters.value(key);return true;}
    if(source==QLatin1String("runInfo")) {*out=m_runInfo.value(key);return true;}
    if(source==QLatin1String("counter")) {
        if(key==QLatin1String("passedSteps"))*out=m_stepPass;
        else if(key==QLatin1String("failedSteps"))*out=m_stepFail;
        else if(key==QLatin1String("passedMeasurements"))*out=m_measurePass;
        else if(key==QLatin1String("failedMeasurements"))*out=m_measureFail;
        return true;
    }
    if(source==QLatin1String("measurement") || source==QLatin1String("stepStatus")) {
        const QString target=c.value(QStringLiteral("step")).toString();
        const QString path=c.value(QStringLiteral("path")).toString();
        for(int index=m_cursor-1;index>=0;--index) {
            const Step& prior=m_steps.at(index);
            if(prior.id!=target && prior.templateId!=target)continue;
            bool sameContext=true;
            for(auto it=step.parameters.cbegin();it!=step.parameters.cend();++it) {
                // A sweep's current value/iteration must match a previous leaf
                // from that iteration; a global Setup result can still be used.
                if(prior.parameters.contains(it.key()) && prior.parameters.value(it.key())!=it.value()) {
                    sameContext=false;break;
                }
            }
            if(!sameContext)continue;
            if(source==QLatin1String("stepStatus")){*out=prior.status;return true;}
            for(int n=m_measurements.size()-1;n>=0;--n) {
                const QVariantMap record=m_measurements.at(n).toMap();
                if(record.value(QStringLiteral("stepId")).toString()==prior.id &&
                   (path.isEmpty()||record.value(QStringLiteral("path")).toString()==path)) {
                    *out=record.value(QStringLiteral("value"));return true;
                }
            }
            return TestSequenceConditions::reject(error,QStringLiteral("No measurement path '%1' was recorded by '%2'").arg(path,target));
        }
        return TestSequenceConditions::reject(error,QStringLiteral("No completed step '%1' in this sweep/repeat context").arg(target));
    }
    return TestSequenceConditions::reject(error,QStringLiteral("Unsupported condition source"));
}

void TestRunnerBridge::startStep() {
    if(!m_sessionActive || m_active>=0 || m_cursor>=m_steps.size())return;
    Step& candidate=m_steps[m_cursor];
    for(const QVariant& guardValue:candidate.guards) {
        const QVariantMap guard=guardValue.toMap();
        const QString conditionId=guard.value(QStringLiteral("id")).toString();
        if(!m_conditionOutcomes.contains(conditionId)) {
            m_lastError=QStringLiteral("Missing conditional decision '%1'").arg(conditionId);
            m_hadError=true;goToCleanup(m_lastError);return;
        }
        if(m_conditionOutcomes.value(conditionId)!=guard.value(QStringLiteral("value")).toBool()) {
            candidate.status=QStringLiteral("skipped");candidate.detail=QStringLiteral("Conditional branch not selected");
            candidate.elapsedMs=0;candidate.finishedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            ++m_completed;++m_skipped;patchStep(candidate);++m_cursor;publishSummary();
            return;
        }
    }
    m_active=m_cursor;
    m_activeFailed=false;m_activeInvalid=false;m_activeDetail.clear();m_tickCount=0;
    m_stepToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_stepCompletionQueued=false;
    Step& step=m_steps[m_active];
    step.status=QStringLiteral("running");
    step.detail=step.mode==QStringLiteral("callback")?QStringLiteral("Waiting for completion callback..."):
                step.mode==QStringLiteral("once")?QStringLiteral("Executing..."):
                step.mode==QStringLiteral("delay")?QStringLiteral("Timed delay in progress"):
                                                   QStringLiteral("Polling for completion...");
    step.elapsedMs=0;
    step.startedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if(step.group==QStringLiteral("cleanup"))m_cleanupMode=true;
    m_stepClock.start();m_lastPublishMs=0;
    patchStep(step);
    log(QStringLiteral("INFO"),QStringLiteral("Starting %1 (%2)").arg(step.name,step.mode),QStringLiteral("Sequence"));
    if(step.mode==QLatin1String("condition")) {
        QVariant left;QString reason;bool decision=false;
        if(!resolveCondition(step,&left,&reason) ||
           !TestSequenceConditions::compare(left,QJsonObject::fromVariantMap(step.condition),&decision,&reason)) {
            m_lastError=QStringLiteral("IF %1: %2").arg(step.id,reason);
            completeStep(QStringLiteral("error"),m_lastError);return;
        }
        m_conditionOutcomes.insert(step.id,decision);
        step.result=decision;step.hasResult=true;
        completeStep(QStringLiteral("done"),QStringLiteral("IF %1 = %2 → %3")
            .arg(step.condition.value(QStringLiteral("source")).toString(),left.toString(),
                 decision?QStringLiteral("THEN"):QStringLiteral("ELSE")));
        return;
    }
    if(step.start.isCallable()) {
        const QJSValue value=invoke(step.start,QJSValueList{stepContext(step)});
        if(stopping()) {completeStep(QStringLiteral("aborted"),QStringLiteral("Execution interrupted"));return;}
        if(m_gracefulStopRequested && !m_cleanupMode){
            completeStep(QStringLiteral("aborted"),QStringLiteral("Graceful termination requested"));return;
        }
        if(value.isError()) {
            m_lastError=QStringLiteral("Step %1: %2").arg(step.id,value.toString());
            emit stepException(step.id,value);
            completeStep(QStringLiteral("error"),m_lastError);return;
        }
        // A synchronous callback cannot be preempted by Qt's event-loop timer.
        // Check its elapsed wall time immediately after it returns.
        if(checkDeadlines())return;
        if(m_activeInvalid){completeStep(QStringLiteral("error"),m_lastError);return;}
        if(step.mode==QStringLiteral("callback")) {
            if(!value.isUndefined() && !value.isNull()) {
                m_lastError=QStringLiteral("Callback handler must return undefined; use TestRunner.complete(token, value)");
                completeStep(QStringLiteral("error"),m_lastError);
                return;
            }
            scheduleTick();
            return;
        }
        if(step.mode==QStringLiteral("once") && value.isObject() && !value.isCallable()) {
            const QVariant payload=value.toVariant(QJSValue::ConvertJSObjects);
            QString error;
            if(!StructuredTestResults::validateResult(payload,&error) ||
               (step.type==QStringLiteral("numeric") && step.measurementRules.isEmpty())) {
                m_lastError=error.isEmpty()?QStringLiteral("Object result needs named measurement rules"):error;
                completeStep(QStringLiteral("error"),m_lastError);
            } else finishStructuredResult(payload);
            return;
        }
        if(step.type==QStringLiteral("numeric") && step.mode==QStringLiteral("once")) {
            if(!step.measurementRules.isEmpty()) {
                m_lastError=QStringLiteral("Named result step must return a JSON object or array");
                completeStep(QStringLiteral("error"),m_lastError);return;
            }
            if(!value.isNumber() || !std::isfinite(value.toNumber())) {
                m_lastError=QStringLiteral("Numeric test handler must return a finite number");
                completeStep(QStringLiteral("error"),m_lastError);return;
            }
            m_running=true;
            const bool passed=measure(step.name,value.toNumber(),step.limitLow,step.limitHigh,
                                      step.unit,step.resultWidget);
            m_running=false;
            completeStep(m_activeInvalid?QStringLiteral("error"):
                passed?QStringLiteral("pass"):QStringLiteral("fail"),m_activeDetail);
            return;
        }
        if(value.isBool() && !value.toBool()){
            completeStep(QStringLiteral("fail"),m_activeDetail.isEmpty()?QStringLiteral("Step returned false"):m_activeDetail);
            return;
        }
        if(step.mode==QStringLiteral("once")) {
            if(!value.isUndefined() && !value.isBool()) {
                m_lastError=QStringLiteral("Once step must return boolean or undefined");
                completeStep(QStringLiteral("error"),m_lastError);
            } else completeStep(m_activeFailed?QStringLiteral("fail"):
                (step.type==QStringLiteral("action")?QStringLiteral("done"):QStringLiteral("pass")),m_activeDetail);
            return;
        }
        if(!value.isUndefined() && !value.isBool() && !value.isNull()){
            m_lastError=QStringLiteral("Start handler must return boolean, null or undefined");
            completeStep(QStringLiteral("error"),m_lastError);return;
        }
    }
    if(checkDeadlines())return;
    if(m_gracefulStopRequested && !m_cleanupMode){
        completeStep(QStringLiteral("aborted"),QStringLiteral("Graceful termination requested"));return;
    }
    if(m_activeFailed && m_stopOnFailure){completeStep(QStringLiteral("fail"),m_activeDetail);return;}
    scheduleTick();
}
void TestRunnerBridge::onTick() {
    if(!m_sessionActive || m_active<0)return;
    if(stopping()) {completeStep(QStringLiteral("aborted"),QStringLiteral("Execution interrupted"));return;}
    if(checkDeadlines())return;
    Step& step=m_steps[m_active];
    if(step.mode==QStringLiteral("delay")) {
        if(m_stepClock.elapsed()>=step.durationMs)completeStep(
            step.type==QStringLiteral("action")?QStringLiteral("done"):QStringLiteral("pass"),
            QStringLiteral("Delay complete"));
        else scheduleTick();
        return;
    }
    if(step.mode==QStringLiteral("callback")) {
        // No JavaScript polling for callback steps. This timer only checks
        // deadlines and updates visual clocks at 10 Hz.
        if(m_stepClock.elapsed()-m_lastPublishMs>=100) {
            step.elapsedMs=m_stepClock.elapsed();m_lastPublishMs=step.elapsedMs;
            patchStep(step);publishSummary();
        }
        scheduleTick();return;
    }
    if(step.mode!=QStringLiteral("poll"))return;
    ++m_tickCount;
    const QJSValue value=invoke(step.poll,QJSValueList{
        QJSValue(static_cast<double>(m_stepClock.elapsed())),
        QJSValue(static_cast<double>(m_runClock.elapsed())),stepContext(step)});
    if(stopping()){completeStep(QStringLiteral("aborted"),QStringLiteral("Execution interrupted"));return;}
    if(m_gracefulStopRequested && !m_cleanupMode){
        completeStep(QStringLiteral("aborted"),QStringLiteral("Graceful termination requested"));return;
    }
    if(value.isError()){
        m_lastError=QStringLiteral("Step %1: %2").arg(step.id,value.toString());
        emit stepException(step.id,value);
        completeStep(QStringLiteral("error"),m_lastError);return;
    }
    if(m_activeInvalid){completeStep(QStringLiteral("error"),m_lastError);return;}
    if(checkDeadlines())return;
    if(m_activeFailed && m_stopOnFailure){completeStep(QStringLiteral("fail"),m_activeDetail);return;}
    if(step.type==QStringLiteral("numeric") && !step.measurementRules.isEmpty() &&
       value.isObject() && !value.isCallable()) {
        const QVariant payload=value.toVariant(QJSValue::ConvertJSObjects);
        QString error;
        if(!StructuredTestResults::validateResult(payload,&error)) {
            m_lastError=error;completeStep(QStringLiteral("error"),m_lastError);
        } else finishStructuredResult(payload);
        return;
    }
    if(step.type==QStringLiteral("numeric") && value.isNumber()) {
        if(!step.measurementRules.isEmpty()) {
            m_lastError=QStringLiteral("Named result step must return a JSON object or array");
            completeStep(QStringLiteral("error"),m_lastError);return;
        }
        m_running=true;
        const bool passed=measure(step.name,value.toNumber(),step.limitLow,step.limitHigh,
                                  step.unit,step.resultWidget);
        m_running=false;
        completeStep(m_activeInvalid?QStringLiteral("error"):
            passed?QStringLiteral("pass"):QStringLiteral("fail"),m_activeDetail);
        return;
    }
    if(step.type==QStringLiteral("numeric") && value.isBool()) {
        m_lastError=QStringLiteral("Numeric poll handler must return a number, null or undefined");
        completeStep(QStringLiteral("error"),m_lastError);return;
    }
    if(value.isBool()){
        completeStep(value.toBool() && !m_activeFailed?
            (step.type==QStringLiteral("action")?QStringLiteral("done"):QStringLiteral("pass")):
            QStringLiteral("fail"),m_activeDetail);
        return;
    }
    if(!value.isUndefined() && !value.isNull()){
        m_lastError=QStringLiteral("Polling callback must return true/false (complete) or undefined/null (pending)");
        completeStep(QStringLiteral("error"),m_lastError);return;
    }
    // Refresh visual clocks at <=10 Hz, not at each 10ms polling invocation.
    if(m_stepClock.elapsed()-m_lastPublishMs>=100){
        step.elapsedMs=m_stepClock.elapsed();m_lastPublishMs=step.elapsedMs;
        patchStep(step);publishSummary();
    }
    scheduleTick();
}
int TestRunnerBridge::cleanupIndex() const {
    for(int i=0;i<m_steps.size();++i)
        if(m_steps.at(i).group==QStringLiteral("cleanup"))return i;
    return m_steps.size();
}
void TestRunnerBridge::goToCleanup(const QString& reason) {
    if(!m_sessionActive)return;
    if(m_cleanupMode) {
        if(m_active<0 && m_cursor>=m_steps.size())endSession(false);
        return;
    }
    const int target=cleanupIndex();
    // Report skipped Main steps explicitly rather than leaving them as PENDING.
    if(target<m_steps.size()) {
        for(int i=m_cursor;i<target;++i) {
            Step& step=m_steps[i];
            if(step.status!=QStringLiteral("pending"))continue;
            step.status=QStringLiteral("skipped");
            step.detail=reason.left(300);
            step.elapsedMs=0;
            step.finishedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            ++m_completed;++m_skipped;
            patchStep(step);
        }
    }
    m_cursor=target;
    m_cleanupMode=true;
    m_autoRun=true; // safety cleanup always completes, even from single-step mode
    publishSummary();
    if(target>=m_steps.size()) {endSession(false);return;}
    log(QStringLiteral("INFO"),QStringLiteral("Entering Cleanup: ")+reason,QStringLiteral("Sequence"));
    if(m_autoRun && !m_pumping)
        QTimer::singleShot(0,this,[this]{pump();});
}
void TestRunnerBridge::completeStep(const QString& status,const QString& detail) {
    if(m_active<0 || !m_sessionActive)return;
    m_timer.stop();
    // Invalidate this token before finish() or any delayed driver callback.
    m_stepToken.clear();m_stepCompletionQueued=false;
    const int index=m_active;
    Step& step=m_steps[index];
    QString finalStatus=status;
    QString finalDetail=detail.isEmpty()?
        (m_activeDetail.isEmpty()?QStringLiteral("Completed"):m_activeDetail):detail;
    step.elapsedMs=m_stepClock.elapsed();
    if(step.finish.isCallable() && !m_shuttingDown && !(m_stopRequested && m_stopRequested->load()) &&
       !(m_engine && m_engine->isInterrupted())) {
        const QJSValue outcome=invoke(step.finish,QJSValueList{
            QJSValue(finalStatus),QJSValue(static_cast<double>(step.elapsedMs)),stepContext(step)});
        if(outcome.isError()) {
            emit stepException(step.id,outcome);
            m_lastError=QStringLiteral("Step %1 cleanup: %2").arg(step.id,outcome.toString());
            finalStatus=QStringLiteral("error");finalDetail=m_lastError;
        }
    }
    step.status=finalStatus;step.detail=finalDetail.left(300);
    step.finishedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    ++m_completed;
    if(finalStatus==QStringLiteral("pass"))++m_stepPass;
    else if(finalStatus==QStringLiteral("done"))++m_actionDone;
    else ++m_stepFail;
    if(finalStatus==QStringLiteral("error"))m_hadError=true;
    patchStep(step);
    log(finalStatus==QStringLiteral("pass")?QStringLiteral("PASS"):
        finalStatus==QStringLiteral("done")?QStringLiteral("INFO"):
        finalStatus==QStringLiteral("fail")?QStringLiteral("FAIL"):
        finalStatus==QStringLiteral("aborted")?QStringLiteral("WARN"):QStringLiteral("ERROR"),
        step.name+QStringLiteral(": ")+step.detail,QStringLiteral("Sequence"));
    m_active=-1;
    m_cursor=index+1;
    publishSummary();
    if(stopping()) {endSession(true);return;}
    if(m_cleanupMode) {
        // A cleanup failure is recorded, but does not skip remaining cleanup steps.
        if(m_cursor>=m_steps.size()) {endSession(false);return;}
    } else if(m_gracefulStopRequested || finalStatus==QStringLiteral("error") ||
              ((finalStatus==QStringLiteral("fail") || m_measureFail>0) && m_stopOnFailure)) {
        goToCleanup(m_gracefulStopRequested?QStringLiteral("Termination requested"):
            finalStatus==QStringLiteral("error")?QStringLiteral("Execution error"):
            QStringLiteral("Test failed"));
        if(!m_sessionActive)return;
    } else if(m_cursor>=m_steps.size()) {
        endSession(false);return;
    }
    if(m_autoRun && !m_pumping)
        QTimer::singleShot(0,this,[this]{pump();});
}
void TestRunnerBridge::pump() {
    if(m_pumping || !m_sessionActive || m_active>=0)return;
    m_pumping=true;
    while(m_sessionActive && m_active<0 && m_cursor<m_steps.size() && m_autoRun && !stopping()) {
        if(!m_cleanupMode && m_totalTimeoutMs>0 && m_runClock.elapsed()>=m_totalTimeoutMs){
            m_lastError=QStringLiteral("Total test timeout before step %1").arg(m_steps[m_cursor].id);
            m_hadError=true;
            goToCleanup(QStringLiteral("Total execution deadline exceeded"));
            continue;
        }
        startStep();
    }
    if(stopping() && m_sessionActive && m_active<0)endSession(true);
    m_pumping=false;
}
void TestRunnerBridge::archiveFinishedRun() {
    const QString directory=TestReportStorage::directory(m_archiveSettings);
    if(!TestReportStorage::enabled(m_archiveSettings)) {
        m_archiveInfo=QVariantMap{{QStringLiteral("status"),QStringLiteral("disabled")},
            {QStringLiteral("autoSaveEnabled"),false},
            {QStringLiteral("directory"),directory}};
        return;
    }
    QString path,error;
    const bool saved=TestResultsRepository::archive(reportData(),m_archiveSettings,&path,&error);
    m_archiveInfo=QVariantMap{{QStringLiteral("status"),saved?QStringLiteral("saved"):QStringLiteral("error")},
        {QStringLiteral("autoSaveEnabled"),true},
        {QStringLiteral("directory"),directory},
        {QStringLiteral("path"),path},
        {QStringLiteral("error"),error},
        {QStringLiteral("executionId"),m_executionId}};
    // A file-system failure must NEVER rewrite the test verdict or suppress Cleanup.
    if(saved)emit message(QStringLiteral("TestRunner archived JSON report: ")+path);
    else emit message(QStringLiteral("WARNING — Test result was NOT archived: ")+error);
}
void TestRunnerBridge::endSession(bool aborted) {
    if(!m_sessionActive)return;
    m_timer.stop();
    m_running=false;m_sessionActive=false;m_active=-1;m_autoRun=false;
    m_elapsedMs=m_runClock.elapsed();
    m_finishedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_state=aborted || m_gracefulStopRequested?QStringLiteral("aborted"):
        m_hadError || !m_lastError.isEmpty()?QStringLiteral("error"):
        m_stepFail>0 || m_measureFail>0?QStringLiteral("failed"):QStringLiteral("passed");
    publishSummary();
    log(m_state==QStringLiteral("passed")?QStringLiteral("PASS"):
        m_state==QStringLiteral("aborted")?QStringLiteral("WARN"):QStringLiteral("ERROR"),
        QStringLiteral("Sequence %1: %2/%3 steps, %4/%5 measurements passed (%6 ms)")
            .arg(m_state).arg(m_completed).arg(m_steps.size()).arg(m_measurePass)
            .arg(m_measurePass+m_measureFail).arg(m_elapsedMs));
    const QString finalReason=m_lastError.isEmpty()?QString():QStringLiteral(" — ERROR: ")+m_lastError;
    emit message(QStringLiteral("TestRunner %1: %2/%3 steps processed (%4 skipped); %5/%6 measurements passed (%7 ms)%8")
        .arg(m_state).arg(m_completed).arg(m_steps.size()).arg(m_skipped).arg(m_measurePass)
        .arg(m_measurePass+m_measureFail).arg(m_elapsedMs).arg(finalReason));
    if(!m_lastError.isEmpty())log(QStringLiteral("ERROR"),m_lastError,QStringLiteral("TestRunner"));
    archiveFinishedRun(); // After Cleanup, before onFinished and before another run can begin.
    const QVariantMap final=summary();
    emit sequenceFinished(final);
    if(m_finishedHandler.isCallable() && !m_shuttingDown &&
       !(m_stopRequested && m_stopRequested->load()) &&
       !(m_engine && m_engine->isInterrupted())) {
        m_notifyingCompletion=true;
        const QJSValue value=invoke(m_finishedHandler,QJSValueList{ScriptValueConversion::toJavaScript(m_engine,final)});
        m_notifyingCompletion=false;
        if(value.isError())emit stepException(QStringLiteral("onFinished"),value);
    }
}
bool TestRunnerBridge::next() {
    if(m_notifyingCompletion)return setError(QStringLiteral("Cannot start a run from onFinished(); use a later UI event"));
    if(m_running || m_active>=0)return setError(QStringLiteral("Current step is still running"));
    if(!beginSession())return false;
    if(m_cursor>=m_steps.size()){endSession();return m_state==QStringLiteral("passed");}
    m_autoRun=false;
    if(!m_cleanupMode && m_totalTimeoutMs>0 && m_runClock.elapsed()>=m_totalTimeoutMs){
        m_lastError=QStringLiteral("Total test timeout before next step");
        m_hadError=true;goToCleanup(QStringLiteral("Total execution deadline exceeded"));
        if(!m_sessionActive)return false;
    }
    startStep();
    // For asynchronous steps, true means the step was accepted; inspect state().
    return m_state==QStringLiteral("running") || m_state==QStringLiteral("passed");
}
bool TestRunnerBridge::run() {
    if(m_notifyingCompletion)return setError(QStringLiteral("Cannot start a run from onFinished(); use a later UI event"));
    if(m_running)return setError(QStringLiteral("Cannot nest TestRunner.run()"));
    if(!beginSession())return false;
    m_autoRun=true;
    if(m_active<0)pump();
    // For delayed/polling steps, run() returns when scheduling succeeds, while
    // the executive continues via Qt's event loop. Check state()/summary()
    // or listen to the sequenceFinished signal for the terminal outcome.
    return m_state==QStringLiteral("running") || m_state==QStringLiteral("passed");
}
void TestRunnerBridge::shutdown() {
    m_shuttingDown=true;
    m_timer.stop();
    m_stepToken.clear();m_stepCompletionQueued=false;
    m_sessionActive=false;m_active=-1;m_running=false;
}
