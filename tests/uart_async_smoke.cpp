// v0.2-dev.1: native asynchronous UART queue/callback + widget smoke.
// Injected samples are test fixtures, NEVER claimed to be physical UART.
#include "ProtocolBridge.h"
#include "widgets/UartTerminalWidget.h"
#include "widgets/UartTerminalWidgetFactory.h"
#include "CompletionCatalog.h"
#include "ApiKnowledge.h"
#include <QApplication>
#include <QJSEngine>
#include <QDebug>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QTextDocument>

class ProtocolUartTestAccess {
public:
    static void inject(ProtocolUartPort* uart,const QVariantMap& event){uart->ingest(event);}
};
static bool check(bool condition,const char* message){
    if(!condition)qCritical()<<"UART async smoke failure:"<<message;
    return condition;
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    ProtocolBridge root(nullptr);
    auto* uart=static_cast<ProtocolUartPort*>(root.uart());
    QJSEngine engine;
    QJSEngine::setObjectOwnership(&root,QJSEngine::CppOwnership);
    QJSEngine::setObjectOwnership(uart,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("Protocol1"),engine.newQObject(&root));
    auto js=engine.evaluate(QStringLiteral(R"JS(
       var observed=[];
       Protocol1.UART.configure({tx:0,rx:1,baudRate:115200});
       Protocol1.UART.onData(function(chunk){observed.push(chunk.text);});
       Protocol1.UART.startAutoRx({pollIntervalMs:10,bufferSize:1024});
    )JS"));
    if(!check(!js.isError() && uart->autoReceiving(),"startAutoRx in QJSEngine"))return 1;
    UartTerminalWidgetFactory factory;
    if(!check(factory.typeId()==QStringLiteral("uart_terminal"),"factory ID"))return 1;
    auto def=factory.defaultDefinition(QStringLiteral("term"));
    UartTerminalWidget widget;
    // Apply through the factory (QJsonObject-backed properties), not directly:
    // this exercises defaults and persisted property overrides used in real Panels.
    def.properties.remove(QStringLiteral("maxLines"));
    def.properties.remove(QStringLiteral("hexMode"));
    factory.applyDefinition(&widget,def);
    auto* display=widget.findChild<QPlainTextEdit*>(QStringLiteral("uartTerminalRx"));
    if(!check(display && display->document()->maximumBlockCount()==1000,
              "UART terminal missing maxLines default"))return 1;
    def.properties.insert(QStringLiteral("maxLines"),150);
    def.properties.insert(QStringLiteral("hexMode"),true);
    factory.applyDefinition(&widget,def);
    if(!check(display->document()->maximumBlockCount()==150,
              "UART terminal maxLines property override"))return 1;
    def.properties.insert(QStringLiteral("hexMode"),false);
    factory.applyDefinition(&widget,def);
    int nativeChunks=0;
    QObject::connect(uart,&ProtocolUartPort::dataReceived,&app,[&](const QVariantMap& chunk){
        ++nativeChunks;
        widget.applyUpdate(chunk);
        const auto callback=uart->dispatchOnData(&engine,chunk);
        if(callback.isError())qCritical()<<callback.toString();
    });
    ProtocolUartTestAccess::inject(uart,{{"ok",true},{"data",QVariantList{72,73,10}},{"count",3},
        {"overflow",false},{"parityErrorIndex",0}});
    if(!check(nativeChunks==1 && widget.contents()==QStringLiteral("HI\n"),"native data to widget"))return 2;
    if(!check(engine.evaluate(QStringLiteral("observed.join('|')")).toString()==QStringLiteral("HI\n"),
              "JS onData callback received copy"))return 3;
    if(!check(uart->available()==3,"onData does not consume queue"))return 4;
    auto r=uart->read(2);
    if(!check(r.value(QStringLiteral("queued")).toBool() && r.value(QStringLiteral("count")).toInt()==2 &&
              uart->available()==1,"bounded queue read"))return 5;
    uart->clearOnData();
    ProtocolUartTestAccess::inject(uart,{{"ok",true},{"data",QVariantList{33}},{"count",1},
        {"overflow",true},{"parityErrorIndex",1}});
    if(!check(uart->stats().value(QStringLiteral("sdkOverflows")).toLongLong()==1 &&
              uart->stats().value(QStringLiteral("parityErrors")).toLongLong()==1,"error counters"))return 6;
    if(!check(engine.evaluate(QStringLiteral("observed.length")).toInt()==1 &&
              widget.contents().contains(QLatin1Char('!')),
              "clearOnData does not interrupt native Terminal"))return 6;
    QVariantList large;for(int i=0;i<1300;++i)large.append(65);
    ProtocolUartTestAccess::inject(uart,{{"ok",true},{"data",large},{"count",1300}});
    if(!check(uart->available()==1024 && uart->stats().value(QStringLiteral("queueDroppedBytes")).toLongLong()>0,
              "ring buffer capped and drops accounted"))return 7;
    if(!check(uart->stopAutoRx() && !uart->autoReceiving(),"timer stopped"))return 8;
    const QVariantMap drained=uart->read(4096);
    if(!check(drained.value(QStringLiteral("count")).toInt()==1024 &&
              drained.value(QStringLiteral("overflow")).toBool() &&
              drained.value(QStringLiteral("parityErrorIndex")).toInt()==1,
              "queued bytes and SDK errors readable after stopAutoRx"))return 9;
    const QVariantMap manual=uart->read(32);
    if(!check(manual.value(QStringLiteral("ok")).toBool() &&
              !manual.value(QStringLiteral("queued")).toBool(),
              "original read SDK behavior restored once queue empty"))return 9;
    if(!check(uart->stats().value(QStringLiteral("receivedBytes")).toLongLong()==1304,
              "accumulated counts"))return 10;
    if(!check(root.stop() && uart->available()==0 && !uart->configured(),"device cleanup"))return 11;
    for(auto symbol:{"Protocol1.UART.startAutoRx","Protocol1.UART.onData","Protocol1.UART.stats"})
        if(!check(ApiKnowledge::isDocumentedSymbol(QString::fromLatin1(symbol)),"API help"))return 12;
    qInfo()<<"PASS: native UART polling state/queue/callback/widget/error handling in simulation";
    return 0;
}
