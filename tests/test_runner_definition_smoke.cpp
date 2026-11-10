// v5.6.61: real Qt/QJSEngine declarative .wftest.json loading,
// named library callbacks, atomic rejection and file round-trip.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
static QJSValue js(QJSEngine& e,const char* code) {
    const auto v=e.evaluate(QString::fromUtf8(code),QStringLiteral("<definition-smoke>"));
    if(v.isError())qFatal("JavaScript failed: %s",qPrintable(v.toString()));
    return v;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stop(false);
    int writes=0;
    TestRunnerBridge runner(&engine,[&](const QString&,const QVariant&){++writes;return true;},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    assert(js(engine,R"JS(
        var fns={init:function(){return true;}, check:function(){
            return TestRunner.measure('V+',3.301,3.25,3.35,'V');},
            tidy:function(){return true;}, begin:function(){},poll:function(){return true;}};
        var doc={schemaVersion:1,id:'voltage-check',title:'Voltage check',
            config:{sequenceId:'seq',tableId:'meas',stopOnFailure:true,totalTimeoutMs:2000},
            steps:[
                {id:'check',name:'Check voltage',group:'main',mode:'once',handler:'check'},
                {id:'tidy',name:'Restore',group:'cleanup',type:'action',mode:'once',handler:'tidy'},
                {id:'init',name:'Setup',group:'setup',type:'action',mode:'once',handler:'init'}
            ]};
        TestRunner.loadDefinition(doc,fns);
    )JS").toBool());
    assert(runner.definition().value(QStringLiteral("id")).toString()==QStringLiteral("voltage-check"));
    assert(runner.summary().value(QStringLiteral("totalSteps")).toInt()==3);
    assert(writes>0);
    // Unsupported schema, missing handlers, duplicate IDs, and incorrect
    // timer fields must never replace the previously loaded definition.
    assert(!js(engine,"var bad=JSON.parse(JSON.stringify(doc));bad.schemaVersion=2;TestRunner.loadDefinition(bad,fns);").toBool());
    assert(!js(engine,"var bad=JSON.parse(JSON.stringify(doc));bad.steps[0].handler='missing';TestRunner.loadDefinition(bad,fns);").toBool());
    assert(!js(engine,"var bad=JSON.parse(JSON.stringify(doc));bad.steps[1].id='check';TestRunner.loadDefinition(bad,fns);").toBool());
    assert(!js(engine,"var bad=JSON.parse(JSON.stringify(doc));bad.steps[0].intervalMs=1;TestRunner.loadDefinition(bad,fns);").toBool());
    assert(runner.definition().value(QStringLiteral("id")).toString()==QStringLiteral("voltage-check"));
    assert(runner.summary().value(QStringLiteral("totalSteps")).toInt()==3);
    assert(js(engine,"TestRunner.run();").toBool());
    const QVariantMap record=runner.reportData();
    assert(record.value(QStringLiteral("state")).toString()==QStringLiteral("passed"));
    assert(record.value(QStringLiteral("definitionId")).toString()==QStringLiteral("voltage-check"));
    assert(record.value(QStringLiteral("measurements")).toList().size()==1);
    const QVariantList steps=record.value(QStringLiteral("steps")).toList();
    assert(steps.at(0).toMap().value(QStringLiteral("id")).toString()==QStringLiteral("init"));
    assert(steps.at(1).toMap().value(QStringLiteral("id")).toString()==QStringLiteral("check"));
    assert(steps.at(2).toMap().value(QStringLiteral("id")).toString()==QStringLiteral("tidy"));
    // Invalid replacement must preserve even a completed report.
    assert(!js(engine,"var bad=JSON.parse(JSON.stringify(doc));bad.steps[2].handler='missing';TestRunner.loadDefinition(bad,fns);").toBool());
    assert(runner.reportData().value(QStringLiteral("executionId"))==record.value(QStringLiteral("executionId")));
    QTemporaryDir temporary;
    assert(temporary.isValid());
    const QString path=temporary.filePath(QStringLiteral("controller.wftest.json"));
    assert(runner.saveDefinition(path));
    QFile file(path);
    assert(file.open(QIODevice::ReadOnly));
    const QJsonDocument saved=QJsonDocument::fromJson(file.readAll());
    assert(saved.isObject());
    assert(saved.object().value(QStringLiteral("steps")).toArray().size()==3);
    file.close();
    assert(runner.clear());
    assert(runner.definition().isEmpty());
    assert(runner.loadDefinitionFile(path,engine.globalObject().property(QStringLiteral("fns"))));
    assert(runner.summary().value(QStringLiteral("totalSteps")).toInt()==3);
    assert(js(engine,"TestRunner.run();").toBool());
    assert(runner.state()==QStringLiteral("passed"));
    assert(!runner.saveDefinition(temporary.filePath(QStringLiteral("incorrect.json"))));
    runner.shutdown();
    return 0;
}
