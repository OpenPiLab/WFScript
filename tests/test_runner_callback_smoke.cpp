// RC1.5.9: genuine QJSEngine test of parameterized, token-based asynchronous
// steps. No instruments, Qt Widgets, or nested event loops are required.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJSEngine>
#include <QJSValue>
#include <QTimer>
#include <atomic>
#include <functional>
#include <cmath>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QJSValue eval(QJSEngine& engine,const QString& text) {
    QJSValue value=engine.evaluate(text,QStringLiteral("<callback-smoke>"));
    if(value.isError())qFatal("JS error: %s",qPrintable(value.toString()));
    return value;
}
static void until(const std::function<bool()>& done) {
    QElapsedTimer timer;timer.start();
    while(!done() && timer.elapsed()<3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    }
    assert(done());
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stop{false};
    TestRunnerBridge runner(&engine,TestRunnerBridge::UiWriter{},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    assert(eval(engine,QStringLiteral(R"JS(
      var calls=[]; var staleToken=''; var liveToken=''; var currentRun='';
      var routines={
        setup:function(step) {
          calls.push('setup'); staleToken=step.token;
          if(step.params.expected!==5 || step.params.metadata.channel!==1)
            throw Error('Parameters not passed to setup');
          return undefined;
        },
        measure:function(step) {
          calls.push('measure');liveToken=step.token;currentRun=step.runId;
          if(step.params.voltage!==5 || step.id!=='read')throw Error('Parameters not passed');
          return undefined;
        },
        cleanup:function(step) { calls.push('cleanup');return true; }
      };
      var doc={schemaVersion:1,id:'callback-lab',title:'Callback laboratory',
        config:{stopOnFailure:true,totalTimeoutMs:900},steps:[
          {id:'setup',name:'Setup',group:'setup',type:'action',mode:'callback',
           handler:'setup',timeoutMs:200,parameters:{expected:5,metadata:{channel:1}}},
          {id:'read',name:'Read voltage',group:'main',type:'numeric',mode:'callback',
           handler:'measure',timeoutMs:200,parameters:{voltage:5},limits:{low:4.9,high:5.1},unit:'V'},
          {id:'cleanup',name:'Cleanup',group:'cleanup',type:'action',mode:'once',handler:'cleanup'}
        ]};
      TestRunner.loadDefinition(doc,routines);
    )JS")).toBool());
    assert(runner.run());
    assert(runner.state()==QStringLiteral("running"));
    const QString first=eval(engine,QStringLiteral("staleToken")).toString();
    assert(!first.isEmpty());
    assert(runner.isActive(first));
    assert(runner.complete(first,QJSValue(true)));
    assert(!runner.complete(first,QJSValue(true))); // first reply wins
    until([&]{return eval(engine,QStringLiteral("calls.length")).toInt()>=2;});
    const QString token=eval(engine,QStringLiteral("liveToken")).toString();
    assert(!token.isEmpty() && token!=first);
    assert(!runner.complete(first,QJSValue(4.99))); // stale step cannot complete new step
    assert(!runner.fail(first,QStringLiteral("late transport failure")));
    assert(runner.isActive(token));
    assert(runner.complete(token,QJSValue(4.9998)));
    until([&]{return runner.state()!=QStringLiteral("running");});
    assert(runner.state()==QStringLiteral("passed"));
    const QVariantMap report=runner.reportData();
    assert(report.value(QStringLiteral("measurements")).toList().size()==1);
    assert(std::fabs(report.value(QStringLiteral("measurements")).toList().at(0).toMap().value(QStringLiteral("value")).toDouble()-4.9998)<0.0001);
    assert(!runner.isActive(token));
    assert(!runner.complete(token,QJSValue(5))); // stale run
    assert(eval(engine,QStringLiteral("calls.join(',')")).toString()==QStringLiteral("setup,measure,cleanup"));
    assert(!eval(engine,QStringLiteral("currentRun")).toString().isEmpty());

    // Failure and cancellation still run Cleanup, but reject late replies.
    assert(eval(engine,QStringLiteral("TestRunner.loadDefinition(doc,routines)")).toBool());
    assert(runner.run());
    const QString cancelToken=eval(engine,QStringLiteral("staleToken")).toString();
    assert(runner.terminate());
    until([&]{return runner.state()!=QStringLiteral("running");});
    assert(!runner.complete(cancelToken,QJSValue(true)));
    assert(runner.reportData().value(QStringLiteral("state")).toString()!=QStringLiteral("passed"));

    // Native deadline should advance into cleanup when a callback never arrives.
    assert(eval(engine,QStringLiteral("TestRunner.loadDefinition(doc,routines)")).toBool());
    assert(runner.run());
    const QString timeoutToken=eval(engine,QStringLiteral("staleToken")).toString();
    until([&]{return runner.state()!=QStringLiteral("running");});
    assert(!runner.complete(timeoutToken,QJSValue(true)));
    assert(runner.reportData().value(QStringLiteral("state")).toString()!=QStringLiteral("passed"));

    // A callback that reports failure must not be misreported as an action PASS.
    assert(eval(engine,QStringLiteral("TestRunner.loadDefinition(doc,routines)")).toBool());
    assert(runner.run());
    const QString failToken=eval(engine,QStringLiteral("staleToken")).toString();
    assert(runner.fail(failToken,QStringLiteral("SCPI read failed")));
    until([&]{return runner.state()!=QStringLiteral("running");});
    assert(!runner.complete(failToken,QJSValue(true)));
    assert(runner.reportData().value(QStringLiteral("state")).toString()!=QStringLiteral("passed"));
    runner.shutdown();
    return 0;
}
