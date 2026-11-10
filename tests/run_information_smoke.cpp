// v5.6.64: project-owned native operator metadata validation and run snapshot.
#include "TestRunnerBridge.h"
#include "RunInformationSchema.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVariantMap>
#include <QDebug>
#include <atomic>

#define REQUIRE(x) do{if(!(x))qFatal("RunInfo test failed: %s at line %d",#x,__LINE__);}while(false)
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QJsonObject schema{{"fields",QJsonArray{
        QJsonObject{{"id","uutId"},{"label","UUT Serial Number"},{"type","text"},{"required",true}},
        QJsonObject{{"id","partNumber"},{"label","Part Number"},{"type","text"},{"default","PCB-1042"}},
        QJsonObject{{"id","revision"},{"label","Revision"},{"type","choice"},
                    {"options",QJsonArray{"Rev A","Rev B","Rev C"}},{"default","Rev C"}},
        QJsonObject{{"id","approved"},{"label","Approved"},{"type","boolean"},{"default",false}}
    }}};
    QString error;
    REQUIRE(RunInformationSchema::validate(schema,&error));
    const QJsonObject bad{{"fields",QJsonArray{
        QJsonObject{{"id","uutId"},{"label","First"},{"type","text"}},
        QJsonObject{{"id","uutId"},{"label","Again"},{"type","text"}}
    }}};
    REQUIRE(!RunInformationSchema::validate(bad,&error));
    QJSEngine engine;
    std::atomic_bool stop(false);
    TestRunnerBridge runner(&engine,[](const QString&,const QVariant&){return true;},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty("TestRunner",engine.newQObject(&runner));
    const QJSValue result=engine.evaluate(QStringLiteral(R"JS(
      var functions={check:function(){return true;},tidy:function(){return true;}};
      var definition={schemaVersion:1,id:'uut-check',title:'UUT Check',
        config:{stopOnFailure:true,totalTimeoutMs:15000},steps:[
          {id:'check',name:'Check',type:'test',mode:'once',handler:'check'},
          {id:'cleanup',name:'Cleanup',group:'cleanup',type:'action',mode:'once',handler:'tidy'}]};
      TestRunner.loadDefinition(definition,functions);
    )JS"));
    REQUIRE(!result.isError() && result.toBool());
    runner.setProjectManaged(true);
    runner.configureRunInformation(schema);
    REQUIRE(!engine.evaluate("TestRunner.setRunInfo({uutId:'bypass'})").toBool());
    REQUIRE(!runner.run());
    REQUIRE(runner.state()=="idle");
    REQUIRE(runner.lastError().contains("UUT Serial Number"));
    REQUIRE(runner.reportData().value("executionId").toString().isEmpty());
    runner.setNativeRunInformation(QVariantMap{{"uutId","UNIT-A"},{"partNumber","PCB-1042"},
        {"revision","Rev C"},{"approved",false}});
    REQUIRE(runner.pendingRunInfo().value("uutId").toString()=="UNIT-A");
    REQUIRE(runner.run());
    REQUIRE(runner.state()=="passed");
    const QVariantMap first=runner.reportData();
    const QString id=first.value("executionId").toString();
    REQUIRE(!id.isEmpty());
    REQUIRE(first.value("runInfo").toMap().value("uutId").toString()=="UNIT-A");
    REQUIRE(first.value("steps").toList().last().toMap().value("status").toString()=="done");
    // Edits for the NEXT run must never rewrite the previous record.
    runner.setNativeRunInformation(QVariantMap{{"uutId",""}});
    REQUIRE(!runner.run());
    REQUIRE(runner.lastError().contains("UUT Serial Number"));
    REQUIRE(runner.reportData().value("state").toString()=="passed");
    REQUIRE(runner.reportData().value("executionId").toString()==id);
    runner.setNativeRunInformation(QVariantMap{{"uutId","UNIT-B"}});
    REQUIRE(engine.evaluate("TestRunner.pendingRunInfo().uutId").toString()=="UNIT-B");
    REQUIRE(engine.evaluate("TestRunner.runInfo().uutId").toString()=="UNIT-A");
    REQUIRE(runner.reportData().value("runInfo").toMap().value("uutId").toString()=="UNIT-A");
    REQUIRE(runner.reset());
    REQUIRE(runner.run());
    REQUIRE(runner.state()=="passed");
    const QVariantMap second=runner.reportData();
    REQUIRE(second.value("runInfo").toMap().value("uutId").toString()=="UNIT-B");
    REQUIRE(second.value("executionId").toString()!=id);
    runner.shutdown();
    qInfo()<<"PASS: native required UUT validation, snapshots, reset and repeated runs";
    return 0;
}
