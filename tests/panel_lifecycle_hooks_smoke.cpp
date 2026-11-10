// v5.6.22: simulation-only integration tests for optional panel lifecycle hooks.
// Uses the actual QJSEngine/PanelRuntime and QThread worker; no GUI or AD3.
#include "PanelRuntime.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QTimer>

struct Result {
    QStringList states;
    QStringList output;
    QStringList contexts;
    QStringList errors;
    bool ready=false;
    bool threadExited=false;
    bool timedOut=false;
};

static Result run(const QString& script, PanelLifecycleSettings settings,
                  const QString& mode) {
    Result result;
    QThread thread;
    QEventLoop loop;
    auto* worker=new PanelRuntime;
    worker->moveToThread(&thread);
    QObject::connect(&thread,&QThread::started,worker,[worker,script,settings]{
        worker->start(script,{{QStringLiteral("go"),false}},false,0,0,
                      QString(),{},QStringLiteral("LifecycleHooks.js"),settings);
    },Qt::QueuedConnection);
    QObject::connect(worker,&PanelRuntime::lifecycleChanged,&loop,
        [&result](const QString& state,const QString&){result.states<<state;});
    QObject::connect(worker,&PanelRuntime::printed,&loop,
        [&result,worker,mode](const QString& message){
            result.output<<message;
            if(mode==QStringLiteral("infinite-callback") && message==QStringLiteral("loop-start")) {
                worker->requestStop();
                QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
            }
        });
    QObject::connect(worker,&PanelRuntime::failed,&loop,
        [&result](const QString& text){result.errors<<text;});
    QObject::connect(worker,&PanelRuntime::scriptDiagnostic,&loop,
        [&result](const QString&,int,int,const QString& context,
                  const QString& message,const QString&){
            result.contexts<<context;
            result.errors<<message;
        });
    QObject::connect(worker,&PanelRuntime::ready,&loop,[&result,worker,mode,&loop]{
        result.ready=true;
        if(mode==QStringLiteral("infinite-callback")) {
            QMetaObject::invokeMethod(worker,[worker]{
                worker->eventFromPanel(QStringLiteral("go"),QStringLiteral("clicked"),true);
            },Qt::QueuedConnection);
        } else if(mode!=QStringLiteral("shutdown-loop")) {
            QTimer::singleShot(15,&loop,[worker]{
                worker->requestStop();
                QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
            });
        } else {
            QTimer::singleShot(15,&loop,[worker]{
                worker->requestStop();
                QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
            });
            // A second Stop must interrupt a hung onShutdown().
            const QPointer<PanelRuntime> pending=worker;
            QTimer::singleShot(250,&loop,[pending]{ if(pending)pending->requestStop(); });
        }
    });
    QObject::connect(worker,&PanelRuntime::stopped,&thread,&QThread::quit,Qt::DirectConnection);
    QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);
    QObject::connect(&thread,&QThread::finished,&loop,[&result,&loop]{
        result.threadExited=true;
        loop.quit();
    });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog,&QTimer::timeout,&loop,[&result,&loop]{
        result.timedOut=true;
        loop.quit();
    });
    thread.start();
    watchdog.start(4500);
    loop.exec();
    if(!thread.wait(1800))qFatal("Panel lifecycle hooks test: worker thread did not stop");
    return result;
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    int failures=0;
    auto check=[&](bool pass,const char* name){
        if(pass)qInfo()<<"PASS"<<name;
        else {++failures;qCritical()<<"FAIL"<<name;}
    };
    const QString healthy=QStringLiteral(
        "print('body');\n"
        "function onInitialize(){print('initialize');}\n"
        "function onShutdown(){print('shutdown');}\n"
        "ui.on('go','clicked',function(){print('clicked');});\n");
    auto normal=run(healthy,{true,true},QStringLiteral("normal"));
    check(normal.ready && normal.threadExited && !normal.timedOut,
          "healthy lifecycle reaches Running and exits thread");
    // Capture only JavaScript print() outputs; PanelRuntime also emits a
    // diagnostic "PANEL SIMULATION" banner through the same printed signal.
    auto scriptOutput=[](const QStringList& messages) {
        QStringList filtered;
        for(const QString& message:messages)
            if(message!=QStringLiteral("PANEL SIMULATION: hardware outputs disabled"))
                filtered<<message;
        return filtered;
    };
    if(scriptOutput(normal.output)!=QStringList({"body","initialize","shutdown"}))
        qCritical()<<"Lifecycle printed messages:"<<normal.output;
    check(scriptOutput(normal.output)==QStringList({"body","initialize","shutdown"}),
          "main script then onInitialize then onShutdown exactly once");
    check(normal.states.contains("Initializing") && normal.states.contains("Initialized") &&
          normal.states.contains("ShutdownHook") && normal.states.contains("ShutdownComplete"),
          "explicit success lifecycle events");
    auto disabled=run(healthy,{},QStringLiteral("normal"));
    check(disabled.ready && disabled.threadExited &&
          scriptOutput(disabled.output)==QStringList({"body"}),"disabled hooks are not called");
    auto noInit=run(QStringLiteral("print('body');"),{true,false},QStringLiteral("none"));
    check(!noInit.ready && noInit.threadExited && noInit.states.contains("Failed") &&
          noInit.errors.join(' ').contains("onInitialize()"),
          "missing enabled initialize fails safely");
    auto initError=run(QStringLiteral(
        "function onInitialize(){throw new Error('bad initialize');}\n"
        "function onShutdown(){print('should not happen');}"),
        {true,true},QStringLiteral("none"));
    check(!initError.ready && initError.threadExited && initError.states.contains("Failed") &&
          initError.contexts.contains("Panel onInitialize()") &&
          !scriptOutput(initError.output).contains(QStringLiteral("should not happen")),
          "initialization exception is navigable and skips shutdown");
    auto shutdownError=run(QStringLiteral(
        "function onInitialize(){print('init');}\n"
        "function onShutdown(){throw new Error('bad shutdown');}"),
        {true,true},QStringLiteral("normal"));
    check(shutdownError.ready && shutdownError.threadExited &&
          shutdownError.contexts.contains("Panel onShutdown()"),
          "shutdown exception reported but native cleanup still finishes");
    auto interrupted=run(QStringLiteral(
        "function onShutdown(){print('should-not-shutdown');}\n"
        "ui.on('go','clicked',function(){print('loop-start');while(true){}});"),
        {false,true},QStringLiteral("infinite-callback"));
    check(interrupted.ready && interrupted.threadExited && !interrupted.timedOut &&
          interrupted.output.contains("loop-start") &&
          !interrupted.output.contains("should-not-shutdown") &&
          interrupted.states.contains("ShutdownSkipped"),
          "interrupted infinite callback bypasses JavaScript shutdown hook");
    auto shutdownLoop=run(QStringLiteral(
        "function onShutdown(){print('shutdown-loop');while(true){}}"),
        {false,true},QStringLiteral("shutdown-loop"));
    check(shutdownLoop.ready && shutdownLoop.threadExited && !shutdownLoop.timedOut &&
          shutdownLoop.output.contains("shutdown-loop") &&
          shutdownLoop.states.contains("ShutdownInterrupted"),
          "second Stop safely interrupts an infinite shutdown hook");
    return failures?1:0;
}
