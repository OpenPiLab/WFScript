// v5.6.59 — Native QJSEngine test; no Digilent DLL or GUI.
// Verifies that the C++ executive owns UUT metadata, per-run variables and
// result snapshots, and that reset/clear do not leak identities between runs.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QVariantMap>
#include <QVariantList>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static bool runJS(QJSEngine& e,const char* src) {
    const QJSValue v=e.evaluate(QString::fromUtf8(src),QStringLiteral("context-smoke.js"));
    if(v.isError())qFatal("Unexpected JavaScript exception: %s",qPrintable(v.toString()));
    return v.toBool();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stopping=false;
    TestRunnerBridge runner(&engine,[](const QString&,const QVariant&){return true;},&stopping);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    assert(runJS(engine,R"JS(
        var expectedUutId='ABC-0042';
        TestRunner.configure({title:'UUT context smoke',stopOnFailure:true}) &&
        TestRunner.setRunInfo({uutId:'ABC-0042',operatorId:'DEMO',stationId:'WIN-BENCH'}) &&
        TestRunner.add({id:'first',name:'First test',group:'main',type:'test'},function(){
            return TestRunner.setVariable('voltage',3.301) &&
                   TestRunner.measure('V+',3.301,3.25,3.35,'V');
        }) &&
        TestRunner.add({id:'second',name:'Shared variable test',group:'main',type:'test'},function(){
            return TestRunner.variable('voltage')===3.301 &&
                   TestRunner.runInfo().uutId===expectedUutId;
        }) && TestRunner.run();
    )JS"));
    const QVariantMap report=runner.reportData();
    assert(report.value(QStringLiteral("state")).toString()==QStringLiteral("passed"));
    assert(report.value(QStringLiteral("runInfo")).toMap()
          .value(QStringLiteral("uutId")).toString()==QStringLiteral("ABC-0042"));
    assert(report.value(QStringLiteral("variables")).toMap()
          .value(QStringLiteral("voltage")).toDouble()==3.301);
    assert(report.value(QStringLiteral("steps")).toList().size()==2);
    assert(runner.summary().value(QStringLiteral("runInfo")).toMap()
          .value(QStringLiteral("operatorId")).toString()==QStringLiteral("DEMO"));
    assert(!runJS(engine,"TestRunner.setRunInfo({bad:[1,2]});"));
    assert(runner.runInfo().value(QStringLiteral("uutId")).toString()==QStringLiteral("ABC-0042"));
    assert(runJS(engine,"TestRunner.reset();"));
    assert(runner.runInfo().isEmpty());
    assert(runner.variables().isEmpty());
    assert(!runJS(engine,"TestRunner.setVariable('voltage',3.7);"));
    // Reusing the exact same step callbacks must test the NEW UUT identity.
    assert(runJS(engine,"expectedUutId='ABC-0043'; TestRunner.setRunInfo({uutId:'ABC-0043'});"));
    assert(!runJS(engine,"TestRunner.setRunInfo({bad:[1,2]});"));
    assert(runner.runInfo().value(QStringLiteral("uutId")).toString()==QStringLiteral("ABC-0043"));
    assert(runJS(engine,"TestRunner.run();"));
    assert(runner.state()==QStringLiteral("passed"));
    assert(runner.reportData().value(QStringLiteral("runInfo")).toMap()
          .value(QStringLiteral("uutId")).toString()==QStringLiteral("ABC-0043"));
    assert(runJS(engine,"TestRunner.clear();"));
    assert(runner.runInfo().isEmpty());
    runner.shutdown();
    return 0;
}
