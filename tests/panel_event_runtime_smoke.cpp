// v5.6.24: test designer-managed direct calls and source-aware exceptions.
#include "PanelRuntime.h"
#include "PanelEventCatalog.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QThread>
#include <QDebug>

struct Result { bool ready=false;bool stopped=false;bool timedOut=false;
    QStringList messages;QStringList sources;QStringList contexts;QList<int> lines; };
static Result run(const QString& code,const QString& functionName,bool enabled=true,
                  const QString& eventName=QStringLiteral("clicked"),
                  const QVariant& initialValue=QVariant(false),
                  const QVariant& eventValue=QVariant(true)) {
    Result result;
    QThread thread; QEventLoop loop;
    PanelRuntime* worker=new PanelRuntime;
    worker->moveToThread(&thread);
    QJsonObject binding{{"handler",functionName},{"enabled",enabled}};
    const QString type=eventName==QStringLiteral("selectionChanged")
            ? QStringLiteral("option_selector") : QStringLiteral("button");
    QJsonArray controls{QJsonObject{{"id","go"},{"type",type},
        {"events",QJsonObject{{eventName,binding}}}}};
    QObject::connect(&thread,&QThread::started,worker,[worker,code,controls,initialValue]{
        worker->start(code,{{"go",initialValue}},false,0,0,QString(),{},"EventTest.js",{},controls);
    },Qt::QueuedConnection);
    QObject::connect(worker,&PanelRuntime::printed,&loop,[&](const QString& message){
        result.messages<<message;
    });
    QObject::connect(worker,&PanelRuntime::scriptDiagnostic,&loop,
        [&](const QString& source,int line,int,const QString& context,const QString&,const QString&){
            result.sources<<source;result.lines<<line;result.contexts<<context;
        });
    QObject::connect(worker,&PanelRuntime::ready,&loop,[&]{
        result.ready=true;
        QMetaObject::invokeMethod(worker,[worker,eventName,eventValue]{
            worker->eventFromPanel("go",eventName,eventValue);
        },Qt::QueuedConnection);
        QTimer::singleShot(120,&loop,[worker]{
            worker->requestStop();QMetaObject::invokeMethod(worker,"stop",Qt::QueuedConnection);
        });
    });
    QObject::connect(worker,&PanelRuntime::stopped,&thread,&QThread::quit,Qt::DirectConnection);
    QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);
    QObject::connect(&thread,&QThread::finished,&loop,[&]{result.stopped=true;loop.quit();});
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog,&QTimer::timeout,&loop,[&]{result.timedOut=true;loop.quit();});
    thread.start();watchdog.start(3500);loop.exec();
    if(!thread.wait(1600))qFatal("Event runtime thread stuck");
    return result;
}
static void require(bool ok,const char* message) {if(!ok)qFatal("Event runtime: %s",message);}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    // This headless runtime test doesn't instantiate WidgetRegistry, which
    // normally registers Option Selector's event definitions in the GUI.
    // Register only the fixture's supported event, without linking Qt Widgets.
    PanelEventCatalog::registerType(QStringLiteral("option_selector"),
        {{QStringLiteral("selectionChanged"),QStringLiteral("selectionChanged(text: string)"),
          QStringLiteral("User selected an option."),QStringLiteral("text")}});
    require(PanelEventCatalog::supports(QStringLiteral("option_selector"),
                                        QStringLiteral("selectionChanged")),
            "Option Selector event registered in headless fixture");
    const auto success=run("var count=0;\nfunction onGoClicked(v){count++;print('managed='+count);}\n",
                           "onGoClicked");
    require(success.ready && success.stopped && !success.timedOut &&
            success.messages.contains("managed=1"),"managed event calls an existing function");
    const auto exception=run("function onGoClicked(v){\n  throw new Error('specific-handler-error');\n}\n", "onGoClicked");
    require(exception.ready && exception.stopped && exception.sources.contains("EventTest.js") &&
            exception.lines.contains(2),"exception retains real named script and line");
    const auto both=run("function onGoClicked(){print('managed');}\n"
                        "ui.on('go','clicked',function(){print('manual');});\n", "onGoClicked");
    require(both.ready && both.messages.contains("manual") && both.messages.contains("managed"),
            "manual ui.on and managed events coexist");
    const auto disabled=run("function onGoClicked(){print('should-not-run');}\n","onGoClicked",false);
    require(disabled.ready && !disabled.messages.contains("should-not-run"),"disabled event does not run");
    // v5.6.43.1: actual selector events must update the text returned by ui.get()
    // before dispatching the JavaScript callback. This was the Signal Lab bug:
    // the ComboBox changed, but the JS bridge continued returning "Sine".
    // Raw C++ string preserves the embedded multi-line JavaScript exactly.
    // Ordinary quoted C++ strings cannot contain unescaped physical newlines.
    const auto selection=run(R"JS(function onShapeChanged(text){
  print('payload='+text+' readback='+ui.get('go'));
}
)JS",
        "onShapeChanged",true,"selectionChanged",QStringLiteral("Sine"),QStringLiteral("Triangle"));
    if(!(selection.ready && selection.stopped &&
         selection.messages.contains("payload=Triangle readback=Triangle")))
        qCritical()<<"Selector diagnostics:"<<"ready"<<selection.ready
                   <<"stopped"<<selection.stopped<<"timedOut"<<selection.timedOut
                   <<"messages"<<selection.messages<<"contexts"<<selection.contexts;
    require(selection.ready && selection.stopped &&
            selection.messages.contains("payload=Triangle readback=Triangle"),
            "Option Selector event updates ui.get before script callback");
    qInfo()<<"PASS: direct named handler, original diagnostics, manual coexistence, disabled bindings, selector state";
    return 0;
}
