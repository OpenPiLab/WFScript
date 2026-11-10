// v5.6.57 — native QTimer and real embedded .mjs step handlers, no AD3.
// This is a worker-thread model using the actual QJSEngine event loop.
#include "TestRunnerBridge.h"
#include "ScriptLibraryBridge.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJSEngine>
#include <QElapsedTimer>
#include <QThread>
#include <QVariantMap>
#include <QHash>
#include <QTimer>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static bool evalOk(QJSEngine& engine,const QString& source) {
    const QJSValue result=engine.evaluate(source,QStringLiteral("<timed-test-smoke>"));
    if(result.isError())qFatal("TestRunner JS error: %s",qPrintable(result.toString()));
    return result.toBool();
}
static bool spinUntil(const std::function<bool()>& done,int timeoutMs=2000) {
    QElapsedTimer clock;clock.start();
    while(clock.elapsed()<timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,20);
        if(done())return true;
        QThread::msleep(2);
    }
    return done();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stopping=false;
    QHash<QString,QVariant> values;
    TestRunnerBridge runner(&engine,[&](const QString& id,const QVariant& value){
        values[id]=value;return true;
    },&stopping);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    int callbacksActive=0;
    QObject::connect(&runner,&TestRunnerBridge::scriptCallbackActive,&runner,
        [&](bool active){callbacksActive+=active?1:-1;assert(callbacksActive>=0);},Qt::DirectConnection);

    const QVariantMap embedded{{QStringLiteral("Checks.mjs"),QStringLiteral(R"JS(
        export function check(elapsedMs) {
            globalThis.pollCounter=(globalThis.pollCounter||0)+1;
            if(elapsedMs<50)return undefined;
            return TestRunner.measure('Voltage',3.301,3.25,3.35,'V','v');
        }
        export function once(){ return true; }
        export function finish(status,elapsed){ globalThis.finishedStep=status;globalThis.finishedMs=elapsed; }
    )JS")}};
    ScriptLibraryBridge library(&engine,QString(),embedded);
    assert(library.valid());
    QJSEngine::setObjectOwnership(&library,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("__wfLibraryHost"),engine.newQObject(&library));
    assert(!engine.evaluate(ScriptLibraryBridge::bootstrapSource()).isError());
    assert(evalOk(engine,QStringLiteral(R"JS(
        var checks=modules.load('Checks.mjs');
        TestRunner.configure({title:'Timed executive',sequenceId:'sequence',tableId:'table',
                              summaryId:'summary',totalTimeoutMs:2000}) &&
        TestRunner.onFinished(function(info){globalThis.finalInfo=info;}) &&
        TestRunner.add({id:'delay',name:'Hardware settling',mode:'delay',durationMs:35,timeoutMs:250},null) &&
        TestRunner.add({id:'poll',name:'Poll measurement',mode:'poll',intervalMs:10,timeoutMs:350},
                       {poll:checks.check,finish:checks.finish}) &&
        TestRunner.add({id:'once',name:'Report'},checks.once) &&
        TestRunner.run();
    )JS")));
    assert(runner.state()==QStringLiteral("running"));
    assert(spinUntil([&]{return runner.state()!=QStringLiteral("running");}));
    assert(runner.state()==QStringLiteral("passed"));
    assert(runner.summary().value(QStringLiteral("elapsedMs")).toLongLong()>=75);
    assert(runner.summary().value(QStringLiteral("completedSteps")).toInt()==3);
    assert(runner.resultsData().size()==1);
    assert(engine.globalObject().property(QStringLiteral("pollCounter")).toInt()>=3);
    assert(engine.globalObject().property(QStringLiteral("finishedStep")).toString()==QStringLiteral("pass"));
    assert(engine.globalObject().property(QStringLiteral("finalInfo")).property(QStringLiteral("state")).toString()==QStringLiteral("passed"));
    assert(callbacksActive==0);

    // Per-step deadline stops a never-completing module poll.
    assert(evalOk(engine,QStringLiteral(R"JS(
        TestRunner.clear() &&
        TestRunner.configure({totalTimeoutMs:1000}) &&
        TestRunner.add({id:'timeout',name:'Never ready',mode:'poll',intervalMs:10,timeoutMs:45},
                       {poll:function(){return undefined;}}) &&
        TestRunner.run();
    )JS")));
    assert(spinUntil([&]{return runner.state()!=QStringLiteral("running");}));
    assert(runner.state()==QStringLiteral("error"));
    assert(runner.lastError().contains(QStringLiteral("timed out")));

    // Total-run deadline applies to pure native delays as well.
    assert(evalOk(engine,QStringLiteral(R"JS(
        TestRunner.clear() && TestRunner.configure({totalTimeoutMs:40}) &&
        TestRunner.add({id:'long',name:'Long settle',mode:'delay',durationMs:200},null) &&
        TestRunner.run();
    )JS")));
    assert(spinUntil([&]{return runner.state()!=QStringLiteral("running");}));
    assert(runner.state()==QStringLiteral("error"));
    assert(runner.lastError().contains(QStringLiteral("Total test timeout")));

    // Abort is handled while the step is pending and its cleanup handler runs.
    assert(evalOk(engine,QStringLiteral(R"JS(
        TestRunner.clear() && TestRunner.configure({totalTimeoutMs:0}) &&
        TestRunner.add({id:'abort',name:'Abortable delay',mode:'delay',durationMs:300},
                       {finish:function(status){globalThis.abortStatus=status;}}) && TestRunner.run();
    )JS")));
    QTimer::singleShot(25,&runner,[&]{assert(runner.abort());});
    assert(spinUntil([&]{return runner.state()!=QStringLiteral("running");}));
    assert(runner.state()==QStringLiteral("aborted"));
    assert(engine.globalObject().property(QStringLiteral("abortStatus")).toString()==QStringLiteral("aborted"));

    // Reset while a delay is pending must stop the timer (no stale future update).
    assert(evalOk(engine,QStringLiteral(R"JS(
        TestRunner.clear() &&
        TestRunner.add({id:'reset',name:'Reset delay',mode:'delay',durationMs:250},null) &&
        TestRunner.run() && TestRunner.reset();
    )JS")));
    assert(runner.state()==QStringLiteral("idle"));
    assert(spinUntil([&]{return runner.state()==QStringLiteral("idle");},100));
    assert(callbacksActive==0);
    runner.shutdown();
    return 0;
}
