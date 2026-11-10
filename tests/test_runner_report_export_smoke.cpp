// Native Qt/QJSEngine contract: explicit completed-run JSON + CSV export.
// Uses a temporary directory; never accesses AD3 or user Documents.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <atomic>

static bool runJs(QJSEngine& engine,const QString& code) {
    QJSValue v=engine.evaluate(code);
    if(v.isError()||!v.toBool()) {qCritical().noquote()<<"JS failed"<<v.toString();return false;}
    return true;
}
#define CHECK(condition,what) do { if(!(condition)){qCritical()<<"FAIL:"<<what;return 1;} }while(0)
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stop{false};
    TestRunnerBridge runner(&engine,[](const QString&,const QVariant&){return true;},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty("TestRunner",engine.newQObject(&runner));
    QTemporaryDir folder;
    CHECK(folder.isValid(),"temp folder");
    const QString json=folder.filePath("complete.json"),csv=folder.filePath("complete.csv");
    CHECK(!runner.saveReport(json,"json"),"never export before a completed run");
    CHECK(runJs(engine,QStringLiteral(R"JS(
        TestRunner.configure({title:'Demo',stopOnFailure:true}) &&
        TestRunner.add({id:'setup',name:'Power up',group:'setup',type:'action'},function(){return true;}) &&
        TestRunner.add({id:'voltage',name:'=UNTRUSTED name',group:'main',type:'test'},
            function(){return TestRunner.measure('V+',3.301,3.25,3.35,'V');}) &&
        TestRunner.add({id:'shutdown',name:'Restore',group:'cleanup',type:'action'},function(){return true;}) &&
        TestRunner.setRunInfo({uutId:'UNIT-001'}) && TestRunner.run()
    )JS")),"run complete native test");
    CHECK(runner.state()=="passed","passed native terminal state");
    CHECK(!runner.saveReport(folder.filePath("bad.txt"),"json"),"reject mismatched suffix");
    CHECK(!runner.saveReport(json,"xml"),"reject unsupported formats");
    CHECK(runner.saveReport(json,"json"),"save JSON");
    QFile file(json);CHECK(file.open(QIODevice::ReadOnly),"open JSON");
    QJsonParseError err{};QJsonDocument parsed=QJsonDocument::fromJson(file.readAll(),&err);
    CHECK(err.error==QJsonParseError::NoError && parsed.isObject(),"valid JSON");
    const QJsonObject result=parsed.object();
    CHECK(result.value("schemaVersion").toInt()==1,"report schema");
    CHECK(result.value("executionId").toString().size()>20,"execution UUID");
    CHECK(result.value("state").toString()=="passed","result status");
    CHECK(result.value("steps").toArray().size()==3,"three grouped steps");
    CHECK(result.value("measurements").toArray().size()==1,"measurement linked to step");
    CHECK(result.value("runInfo").toObject().value("uutId").toString()=="UNIT-001","UUT field");
    CHECK(runner.saveReport(csv,"csv"),"save CSV");
    QFile csvFile(csv);CHECK(csvFile.open(QIODevice::ReadOnly),"open CSV");
    const QByteArray csvText=csvFile.readAll();
    CHECK(csvText.contains("STEP") && csvText.contains("MEASUREMENT"),"CSV record kinds");
    CHECK(csvText.contains("'=UNTRUSTED name"),"spreadsheet injection avoided for step labels");
    CHECK(csvText.contains("3.301") && csvText.contains("voltage"),"numeric value and step association");
    CHECK(!runner.saveReport(folder.filePath("not-a-dir/out.json"),"json"),"reject absent folder");
    CHECK(runJs(engine,"TestRunner.reset();"),"reset");
    CHECK(!runner.saveReport(json,"json"),"reject after reset; do not overwrite prior report");
    qInfo()<<"PASS: native per-run JSON and CSV, safety checks and reset";
    return 0;
}
