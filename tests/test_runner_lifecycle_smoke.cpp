// v5.6.58: Setup/Main/Cleanup, Action/Test status, result records and graceful
// termination. Uses real Qt QJSEngine without Digilent runtime or GUI.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJSEngine>
#include <QJSValue>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantMap>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static bool eval(QJSEngine& engine,const char* source) {
    const QJSValue value=engine.evaluate(QString::fromUtf8(source),QStringLiteral("<lifecycle-smoke>"));
    if(value.isError())qFatal("JS test error: %s",qPrintable(value.toString()));
    return value.toBool();
}
static bool waitFor(const std::function<bool()>& predicate,int timeoutMs=1800) {
    QElapsedTimer t;t.start();
    while(t.elapsed()<timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,20);
        if(predicate())return true;
        QThread::msleep(2);
    }
    return predicate();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stop=false;
    TestRunnerBridge runner(&engine,[](const QString&,const QVariant&){return true;},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    assert(eval(engine,R"JS(
        globalThis.cleanupCount=0;
        TestRunner.configure({title:'Lifecycle test',stopOnFailure:true}) &&
        // Deliberately register in non-execution order.
        TestRunner.add({id:'measure',name:'Limit check',group:'main',type:'test'},
            function(){return TestRunner.measure('V+',3.6,3.25,3.35,'V');}) &&
        TestRunner.add({id:'release',name:'Release fixture',group:'cleanup',type:'action'},
            function(){globalThis.cleanupCount++;return true;}) &&
        TestRunner.add({id:'after',name:'Unreached test',group:'main'},function(){throw Error('must skip');}) &&
        TestRunner.add({id:'setup',name:'Prepare fixture',group:'setup',type:'action'},function(){return true;});
    )JS"));
    assert(!eval(engine,"TestRunner.run();"));
    assert(runner.state()==QStringLiteral("failed"));
    assert(engine.globalObject().property(QStringLiteral("cleanupCount")).toInt()==1);
    QVariantMap record=runner.reportData();
    assert(!record.value(QStringLiteral("executionId")).toString().isEmpty());
    assert(!record.value(QStringLiteral("startedAtUtc")).toString().isEmpty());
    assert(!record.value(QStringLiteral("finishedAtUtc")).toString().isEmpty());
    QVariantList steps=record.value(QStringLiteral("steps")).toList();
    assert(steps.size()==4);
    assert(steps[0].toMap().value(QStringLiteral("id")).toString()==QStringLiteral("setup"));
    assert(steps[0].toMap().value(QStringLiteral("status")).toString()==QStringLiteral("done"));
    assert(steps[1].toMap().value(QStringLiteral("status")).toString()==QStringLiteral("fail"));
    assert(steps[2].toMap().value(QStringLiteral("status")).toString()==QStringLiteral("skipped"));
    assert(steps[3].toMap().value(QStringLiteral("status")).toString()==QStringLiteral("done"));
    assert(record.value(QStringLiteral("measurements")).toList().at(0).toMap()
        .value(QStringLiteral("stepId")).toString()==QStringLiteral("measure"));
    assert(runner.summary().value(QStringLiteral("skippedSteps")).toInt()==1);
    assert(runner.summary().value(QStringLiteral("completedActions")).toInt()==2);

    // A JS exception means ERROR, not UUT FAIL. Cleanup still executes.
    assert(eval(engine,R"JS(
        TestRunner.clear() &&
        TestRunner.add({id:'throw',name:'Bad driver',group:'main'},
            function(){throw Error('simulated instrument disconnect');}) &&
        TestRunner.add({id:'release2',name:'Restore',group:'cleanup',type:'action'},
            function(){globalThis.cleanupCount++;return true;});
    )JS"));
    assert(!eval(engine,"TestRunner.run();"));
    assert(runner.state()==QStringLiteral("error"));
    assert(engine.globalObject().property(QStringLiteral("cleanupCount")).toInt()==2);

    // Graceful terminate while Qt delay is pending runs Cleanup and marks aborted.
    assert(eval(engine,R"JS(
        TestRunner.clear() &&
        TestRunner.add({id:'wait',name:'Wait for relay',group:'main',type:'action',
                        mode:'delay',durationMs:250},null) &&
        TestRunner.add({id:'release3',name:'Restore',group:'cleanup',type:'action'},
            function(){globalThis.cleanupCount++;return true;}) && TestRunner.run();
    )JS"));
    assert(runner.state()==QStringLiteral("running"));
    QTimer::singleShot(25,&runner,[&]{assert(runner.terminate());});
    assert(waitFor([&]{return runner.state()!=QStringLiteral("running");}));
    assert(runner.state()==QStringLiteral("aborted"));
    assert(engine.globalObject().property(QStringLiteral("cleanupCount")).toInt()==3);

    // An emergency abort retains v5.6.57 semantics: no sequence cleanup.
    assert(eval(engine,"TestRunner.reset() && TestRunner.run();"));
    assert(runner.abort());
    assert(runner.state()==QStringLiteral("aborted"));
    assert(engine.globalObject().property(QStringLiteral("cleanupCount")).toInt()==3);

    // Timeouts also execute Cleanup; the expired total deadline does not
    // prevent an explicitly bounded cleanup action from running.
    assert(eval(engine,R"JS(
        TestRunner.clear() && TestRunner.configure({totalTimeoutMs:35}) &&
        TestRunner.add({id:'stuck',name:'Stuck',group:'main',mode:'delay',durationMs:300},null) &&
        TestRunner.add({id:'release4',name:'Restore',group:'cleanup',type:'action'},
            function(){globalThis.cleanupCount++;return true;}) && TestRunner.run();
    )JS"));
    assert(waitFor([&]{return runner.state()!=QStringLiteral("running");}));
    assert(runner.state()==QStringLiteral("error"));
    assert(engine.globalObject().property(QStringLiteral("cleanupCount")).toInt()==4);
    runner.shutdown();
    return 0;
}
