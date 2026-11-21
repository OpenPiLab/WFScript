// v0.2-dev.4: real QJSEngine + native Qt input monitor in simulated DIO mode.
// No DWF SDK connection or AD3 required; physical coexistence is separate.
#include "StaticIOBridge.h"
#include "ApiKnowledge.h"
#include "CompletionCatalog.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QJSEngine>
#include <QDebug>

static bool check(bool ok,const char* message) {
    if(!ok)qCritical()<<"StaticIO monitor smoke FAILED:"<<message;
    return ok;
}
static void runEvents(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms,&loop,&QEventLoop::quit);
    loop.exec();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    StaticIOBridge io(nullptr);
    io.setScriptEngine(&engine);
    QJSEngine::setObjectOwnership(&io,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("StaticIO"),engine.newQObject(&io));
    QJSValue js=engine.evaluate(QStringLiteral(R"JS(
        var events=[];
        var registered=StaticIO.onChange(0,function(v,p,e){
            events.push({v:v,p:p,edge:e.edge,source:e.timestampSource});
        },{edge:'falling',debounceMs:20});
        if(!registered)throw new Error(StaticIO.lastError);
        if(StaticIO.monitoring)throw new Error('monitor started implicitly');
    )JS"));
    if(!check(!js.isError() && !io.monitoring(),"callback registration must never start SDK polling"))return 1;
    // Set simulated DIO0 HIGH with monitor OFF: no background callbacks.
    if(!check(io.setPin(0,1),"simulation HIGH"))return 2;
    runEvents(40);
    if(!check(engine.evaluate(QStringLiteral("events.length")).toInt()==0 &&
              io.monitorStats().value(QStringLiteral("polls")).toULongLong()==0,
              "no polling/events before explicit start"))return 3;
    if(!check(!io.startMonitor({{QStringLiteral("pollIntervalMs"),1}}) &&
              !io.monitoring(),"invalid interval rejected"))return 4;
    if(!check(!io.startMonitor({{QStringLiteral("pollIntervalMs"),21.5}}) &&
              !io.monitoring(),"fractional interval rejected"))return 4;
    if(!check(io.startMonitor({{QStringLiteral("pollIntervalMs"),10}}),"explicit start"))return 5;
    runEvents(40);
    if(!check(engine.evaluate(QStringLiteral("events.length")).toInt()==0,
              "initial snapshot must not fire synthetic edges"))return 6;
    if(!check(io.setPin(0,0),"simulation press low"))return 6;
    runEvents(65);
    if(!check(engine.evaluate(QStringLiteral("events.length")).toInt()==1 &&
              engine.evaluate(QStringLiteral("events[0].edge")).toString()==QStringLiteral("falling") &&
              engine.evaluate(QStringLiteral("events[0].source")).toString()==QStringLiteral("HostPollAfterSDK"),
              "one debounced falling edge with explicit host timestamp source"))return 7;
    if(!check(io.monitorStats().value(QStringLiteral("polls")).toULongLong()>0,
              "poll count increased"))return 8;
    io.stopMonitor();
    const auto count=io.monitorStats().value(QStringLiteral("polls")).toULongLong();
    if(!check(!io.monitoring() && io.setPin(0,1),"explicit stop"))return 9;
    runEvents(50);
    if(!check(io.monitorStats().value(QStringLiteral("polls")).toULongLong()==count &&
              engine.evaluate(QStringLiteral("events.length")).toInt()==1,
              "no background activity after explicit stop"))return 10;
    if(!check(io.startMonitor({{QStringLiteral("pollIntervalMs"),10}}),"restart using same watch"))return 11;
    if(!check(io.setPin(0,0),"second falling edge"))return 12;
    runEvents(65);
    if(!check(engine.evaluate(QStringLiteral("events.length")).toInt()==2,
              "restart preserves callback registrations"))return 13;
    io.clearOnChange(0);
    if(!check(io.monitorStats().value(QStringLiteral("watchCount")).toInt()==0,
              "clearOnChange removes callback"))return 14;
    if(!check(io.setPin(0,1),"high after callback removed"))return 15;
    runEvents(30);
    if(!check(io.setPin(0,0),"low after callback removed"))return 16;
    runEvents(40);
    if(!check(engine.evaluate(QStringLiteral("events.length")).toInt()==2,
              "unregister never dispatches callback"))return 17;
    io.shutdownMonitor();
    if(!check(!io.monitoring() && io.monitorStats().value(QStringLiteral("watchCount")).toInt()==0 &&
              !io.startMonitor(),"permanent Panel shutdown disables and clears monitor"))return 18;
    for(const auto* symbol:{"StaticIO.onChange","StaticIO.clearOnChange","StaticIO.startMonitor",
                              "StaticIO.stopMonitor","StaticIO.monitorStats","StaticIO.monitoring"})
        if(!check(ApiKnowledge::isDocumentedSymbol(QString::fromLatin1(symbol)),"API Explorer documentation"))return 19;
    qInfo()<<"PASS: opt-in input monitoring, debounce, restart, clear, shutdown, API documentation";
    return 0;
}
