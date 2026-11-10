// v5.6.20: real PanelRuntime/QJSEngine worker and QThread lifecycle regression.
// No AD3 connection, GUI, hardware output or external JS files are required.
#include "PanelRuntime.h"
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QStringList>

struct Outcome {
    QStringList states;
    QStringList contexts;
    QStringList messages;
    QVariant finalValue;
    bool ready=false;
    bool stopped=false;
    bool threadFinished=false;
    bool timedOut=false;
    int stoppedCount=0;
};

static Outcome scenario(const QString& script,const QString& action) {
    Outcome result;
    QEventLoop eventLoop;
    QThread thread;
    auto* worker=new PanelRuntime;
    worker->moveToThread(&thread);
    // Match MainWindow: queued delivery ensures start() executes after
    // QThread's event loop is running, so quit() cannot be lost before exec().
    QObject::connect(&thread,&QThread::started,worker,[worker,script]{
        worker->start(script,{{QStringLiteral("runDemo"),false},
                              {QStringLiteral("good"),false},
                              {QStringLiteral("result"),0}},
                      false,0,0,QString(),{},QStringLiteral("LifecycleDemo.js"));
    },Qt::QueuedConnection);
    QObject::connect(worker,&PanelRuntime::lifecycleChanged,&eventLoop,
                     [&result](const QString& state,const QString&){result.states.append(state);});
    QObject::connect(worker,&PanelRuntime::scriptDiagnostic,&eventLoop,
        [&result](const QString&,int,int,const QString& context,const QString& message,const QString&){
            result.contexts.append(context);result.messages.append(message);
        });
    QObject::connect(worker,&PanelRuntime::controlUpdate,&eventLoop,
        [&result](const QString& id,const QVariant& value){
            if(id==QStringLiteral("result"))result.finalValue=value;
        });
    QObject::connect(worker,&PanelRuntime::ready,&eventLoop,[&result,worker,action]{
        result.ready=true;
        if(action==QStringLiteral("event-loop") || action==QStringLiteral("callback-error")) {
            QMetaObject::invokeMethod(worker,[worker]{worker->eventFromPanel(
                QStringLiteral("runDemo"),QStringLiteral("clicked"),true);},Qt::QueuedConnection);
        } else if(action==QStringLiteral("plain-stop")) {
            QTimer::singleShot(15,[worker]{
                worker->requestStop();
                QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
            });
        }
    });
    QObject::connect(worker,&PanelRuntime::printed,&eventLoop,[worker,action](const QString& message){
        if(action==QStringLiteral("event-loop") && message==QStringLiteral("entered loop")) {
            worker->requestStop();
            QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
        }
    });
    QObject::connect(worker,&PanelRuntime::scriptDiagnostic,&eventLoop,
        [worker,action](const QString&,int,int,const QString& context,const QString&,const QString&){
            if(action==QStringLiteral("callback-error") && context.startsWith("Panel event")) {
                QMetaObject::invokeMethod(worker,[worker]{worker->eventFromPanel(
                    QStringLiteral("good"),QStringLiteral("clicked"),true);},Qt::QueuedConnection);
                QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
            }
        });
    QObject::connect(worker,&PanelRuntime::stopped,&eventLoop,[&result]{
        result.stopped=true;++result.stoppedCount;
    });
    QObject::connect(worker,&PanelRuntime::stopped,&thread,&QThread::quit,Qt::DirectConnection);
    QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);
    QObject::connect(&thread,&QThread::finished,&eventLoop,[&]{
        result.threadFinished=true;
        eventLoop.quit();
    });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog,&QTimer::timeout,&eventLoop,[&]{
        result.timedOut=true;
        worker->requestStop();
        QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
        eventLoop.quit();
    });
    if(action==QStringLiteral("startup-loop")) {
        QTimer::singleShot(75,[worker]{
            worker->requestStop();
            QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
        });
    }
    if(action==QStringLiteral("early-stop")) {
        // Stop requested before the thread begins its first event loop.
        worker->requestStop();
    }
    thread.start();
    watchdog.start(4000);
    eventLoop.exec();
    // Wait for worker's cleanup and quit, never destroy an active QThread.
    if(!thread.wait(1500)) {
        worker->requestStop();
        QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
        if(!thread.wait(2000)) {
            // Test-only fallback. Simulation has no physical outputs, and process
            // termination is preferable to leaving a dangling thread object.
            qFatal("Lifecycle smoke test: JS worker did not stop");
        }
    }
    return result;
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    int errors=0;
    auto check=[&](bool test,const QString& label){
        if(test)qInfo().noquote()<<"PASS"<<label;
        else { qCritical().noquote()<<"FAIL"<<label;++errors; }
    };
    auto normal=scenario(QStringLiteral("ui.on('runDemo','clicked',function(){});"),"plain-stop");
    check(normal.ready && normal.stopped && normal.stoppedCount==1 &&
          normal.threadFinished && !normal.timedOut,
          "normal start/stop releases worker and finishes QThread");
    check(normal.states.contains("Starting") && normal.states.contains("Running") &&
          normal.states.contains("Stopping") && normal.states.contains("CleanupComplete") &&
          !normal.states.contains("Stopped"),
          "worker distinguishes cleanup complete from QThread fully stopped");

    auto early=scenario(QStringLiteral("print('should not execute');"),"early-stop");
    check(!early.ready && early.stoppedCount==1 && early.threadFinished && !early.timedOut,
          "Stop before startup does not lose QThread::quit");

    auto invalid=scenario(QStringLiteral("print('x = ' x);"),"none");
    check(!invalid.ready && invalid.stopped && invalid.stoppedCount==1 &&
          invalid.threadFinished && !invalid.timedOut,
          "syntax error prevents ready and cleans up");
    check(invalid.states.contains("Failed") && !invalid.contexts.isEmpty(),
          "startup syntax failure reports Failed and diagnostic");

    auto looping=scenario(QStringLiteral("while(true) {}"),"startup-loop");
    check(!looping.ready && looping.stopped && looping.threadFinished && !looping.timedOut,
          "cross-thread interruption ends CPU-bound startup loop");
    check(!looping.states.contains("Failed"),"user cancellation is not a syntax failure");

    auto callback=scenario(QStringLiteral(
       "ui.on('runDemo','clicked',function(){print('entered loop');while(true) {}});"),
       "event-loop");
    check(callback.ready && callback.stopped && callback.threadFinished && !callback.timedOut,
          "cross-thread interruption ends infinite button callback");
    check(callback.contexts.isEmpty(),"interrupt doesn't invent callback error");

    auto recover=scenario(QStringLiteral(
       "ui.on('runDemo','clicked',function(){throw new Error('expected test error');});"
       "ui.on('good','clicked',function(){ui.set('result',42);});"),
       "callback-error");
    check(recover.ready && recover.stopped && recover.threadFinished &&
          !recover.contexts.isEmpty(),
          "event exception is reported without crashing runtime");
    check(recover.finalValue.toInt()==42,
          "later independent callback still executes after handled exception");

    // A number of quick stop / restart cycles catches missed quit requests and
    // lingering QThreads. Each uses a fresh runtime just like Run Panel does.
    bool repeated=true;
    for(int i=0;i<8;++i){
        auto run=scenario(QStringLiteral("ui.on('runDemo','clicked',function(){});"),
                          "plain-stop");
        if(!run.ready || run.stoppedCount!=1 || !run.threadFinished || run.timedOut){
            repeated=false;
            break;
        }
    }
    check(repeated,"8 consecutive start/stop cycles complete without hanging");
    return errors?1:0;
}
