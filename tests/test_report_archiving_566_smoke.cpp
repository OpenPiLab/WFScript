// v5.6.66 native C++/QJSEngine archive contract (no GUI and no DWF device).
// Covers terminal statuses, unique immutable JSON, disabled mode, write errors,
// run metadata, onFinished ordering and unchanged TestRunner verdict.
#include "TestRunnerBridge.h"
#include "TestReportStorage.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <atomic>

#define REQUIRE(expr,desc) do { if(!(expr)){qCritical()<<"ARCHIVE TEST FAILED:"<<desc;return 1;} }while(false)
static bool eval(QJSEngine& engine,const char* source) {
    const QJSValue result=engine.evaluate(QString::fromUtf8(source));
    if(result.isError())qCritical().noquote()<<"Script error:"<<result.toString();
    return !result.isError() && result.toBool();
}
static QJsonObject readJson(const QString& path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid(),"temporary report folder available");
    const QString dir=tmp.filePath(QStringLiteral("Test Reports"));
    const QJsonObject config{{QStringLiteral("autoSave"),true},{QStringLiteral("directory"),dir}};
    const QJsonObject off{{QStringLiteral("autoSave"),false},{QStringLiteral("directory"),dir}};
    QString error;
    REQUIRE(TestReportStorage::validate(config,&error),"valid archive configuration");
    REQUIRE(!TestReportStorage::validate({{QStringLiteral("autoSave"),"yes"}},&error),"invalid toggle rejected");
    REQUIRE(!TestReportStorage::validate({{QStringLiteral("directory"),QStringLiteral("relative-folder")}},&error),
            "relative paths rejected");
    REQUIRE(!QDir(dir).exists(),"no automatic folder creation during configuration");

    QJSEngine js;
    std::atomic_bool stopped{false};
    TestRunnerBridge runner(&js,[](const QString&,const QVariant&){return true;},&stopped);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    js.globalObject().setProperty(QStringLiteral("TestRunner"),js.newQObject(&runner));
    runner.configureArchive(config,false,0,0,QStringLiteral("ProductionTest.wfpanel"));
    REQUIRE(!QDir(dir).exists(),"no archive on configure/startup");
    const bool firstStarted=eval(js,R"JS(
      globalThis.finishedStatus='';
      TestRunner.onFinished(function(){ finishedStatus=TestRunner.archiveInfo().status; }) &&
      TestRunner.configure({title:'Archive smoke',stopOnFailure:true}) &&
      TestRunner.add({id:'setup',name:'Initialize station',group:'setup',type:'action'},function(){return true;}) &&
      TestRunner.add({id:'measure',name:'Measure V+',group:'main',type:'test'},function(){
         return TestRunner.measure('V+',3.3,3.2,3.4,'V');
      }) &&
      TestRunner.add({id:'cleanup',name:'Restore outputs',group:'cleanup',type:'action'},function(){return true;}) &&
      TestRunner.setRunInfo({uutId:'CTRL:001',operatorId:'Bob'}) && TestRunner.run();
    )JS");
    if(!firstStarted)qCritical().noquote()<<"Initial archive run rejected:"<<runner.lastError();
    REQUIRE(firstStarted,"successful native run");
    REQUIRE(runner.state()==QStringLiteral("passed"),"passed result");
    QVariantMap archive=runner.archiveInfo();
    REQUIRE(archive.value(QStringLiteral("status")).toString()==QStringLiteral("saved"),"archive saved");
    REQUIRE(js.globalObject().property(QStringLiteral("finishedStatus")).toString()==QStringLiteral("saved"),
            "onFinished observes completed archive result");
    const QString first=archive.value(QStringLiteral("path")).toString();
    REQUIRE(QFileInfo::exists(first),"first report exists");
    QJsonObject firstRun=readJson(first);
    REQUIRE(firstRun.value(QStringLiteral("state")).toString()==QStringLiteral("passed"),"stored verdict");
    REQUIRE(firstRun.value(QStringLiteral("runInfo")).toObject().value(QStringLiteral("uutId")).toString()==QStringLiteral("CTRL:001"),"UUT frozen");
    REQUIRE(firstRun.value(QStringLiteral("steps")).toArray().size()==3,"all groups archived");
    REQUIRE(firstRun.value(QStringLiteral("instrument")).toObject().value(QStringLiteral("mode")).toString()==QStringLiteral("simulation"),"simulation provenance");
    REQUIRE(firstRun.value(QStringLiteral("projectName")).toString()==QStringLiteral("ProductionTest.wfpanel"),"source project identified");
    REQUIRE(QDir(dir).entryList({QStringLiteral("*.json")},QDir::Files).size()==1,"one file per completed run");

    REQUIRE(eval(js,R"JS(
      TestRunner.clear() &&
      TestRunner.add({id:'fail',name:'Expected failure',group:'main',type:'test'},function(){return false;}) &&
      TestRunner.add({id:'cleanup',name:'Restore outputs',group:'cleanup',type:'action'},function(){return true;}) &&
      TestRunner.setRunInfo({uutId:'CTRL-002'})
    )JS"),"configure failed run");
    REQUIRE(!eval(js,"TestRunner.run();"),"failed test has false synchronous return");
    REQUIRE(runner.state()==QStringLiteral("failed"),"failure remains failure");
    const QString second=runner.archiveInfo().value(QStringLiteral("path")).toString();
    REQUIRE(QFileInfo::exists(second) && second!=first,"second run uniquely archived");
    const QJsonObject failed=readJson(second);
    REQUIRE(failed.value(QStringLiteral("steps")).toArray().size()==2,"fail and cleanup recorded");
    REQUIRE(failed.value(QStringLiteral("steps")).toArray().at(1).toObject()
        .value(QStringLiteral("status")).toString()==QStringLiteral("done"),"cleanup was completed before archiving");
    REQUIRE(readJson(first)==firstRun,"first report not rewritten by subsequent runs");

    // An invalid folder may not turn a passing UUT into a failing test.
    const QString blocked=tmp.filePath(QStringLiteral("not-a-directory"));
    QFile block(blocked);
    REQUIRE(block.open(QIODevice::WriteOnly),"create path blocker");
    block.write("file, not folder");block.close();
    runner.configureArchive({{QStringLiteral("autoSave"),true},
       {QStringLiteral("directory"),blocked}},true,1,2,QStringLiteral("Hardware.wfpanel"));
    REQUIRE(eval(js,R"JS(
      TestRunner.clear() &&
      TestRunner.add({id:'pass',name:'Passing test',group:'main'},function(){return true;}) &&
      TestRunner.run()
    )JS"),"passing run despite inaccessible archive path");
    REQUIRE(runner.state()==QStringLiteral("passed"),"disk failure never changes result verdict");
    REQUIRE(runner.archiveInfo().value(QStringLiteral("status")).toString()==QStringLiteral("error"),
            "archive error reported separately");
    REQUIRE(!runner.archiveInfo().value(QStringLiteral("error")).toString().isEmpty(),"useful disk error");
    REQUIRE(readJson(first)==firstRun,"previous report intact after disk failure");

    runner.configureArchive(off);
    REQUIRE(eval(js,"TestRunner.reset() && TestRunner.run();"),"successful run with archiving disabled");
    REQUIRE(runner.archiveInfo().value(QStringLiteral("status")).toString()==QStringLiteral("disabled"),
            "per-project archive policy respected");
    REQUIRE(QDir(dir).entryList({QStringLiteral("*.json")},QDir::Files).size()==2,
            "disabled mode does not create files");
    qInfo()<<"PASS automatic TestRunner archive smoke";
    return 0;
}
