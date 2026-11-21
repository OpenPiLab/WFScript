// WFScript v0.2-dev: native QObject protocol scripting simulation smoke.
// Zero hardware activity. Physically validated ABI/AD3 tests remain manual.
#include "ProtocolBridge.h"
#include "CompletionCatalog.h"
#include "ApiKnowledge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QDebug>

static bool inventory(QObject* object,const QString& path) {
    const auto* meta=object->metaObject();
    const auto catalog=CompletionCatalog::entries(path);
    const auto contains=[&](const QString& name){
        for(const auto& entry:catalog)if(entry.name==name)return true;
        return false;
    };
    for(int i=meta->propertyOffset();i<meta->propertyCount();++i)
        if(!contains(QString::fromLatin1(meta->property(i).name()))) {
            qCritical()<<"Missing documented property"<<path<<meta->property(i).name();return false;
        }
    for(int i=meta->methodOffset();i<meta->methodCount();++i) {
        const auto method=meta->method(i);
        if(method.methodType()!=QMetaMethod::Signal && !contains(QString::fromLatin1(method.name()))) {
            qCritical()<<"Missing documented method"<<path<<method.name();return false;
        }
    }
    return true;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    ProtocolBridge protocol(nullptr);
    if(!inventory(&protocol,QStringLiteral("Protocol1")))return 1;
    const QPair<QObject*,QString> ports[] = {
        {protocol.uart(),QStringLiteral("Protocol1.UART")},
        {protocol.spi(),QStringLiteral("Protocol1.SPI")},
        {protocol.i2c(),QStringLiteral("Protocol1.I2C")},
        {protocol.can(),QStringLiteral("Protocol1.CAN")},
        {protocol.swd(),QStringLiteral("Protocol1.SWD")}
    };
    QJSEngine engine;
    QJSEngine::setObjectOwnership(&protocol,QJSEngine::CppOwnership);
    for(const auto& port:ports) {
        if(!inventory(port.first,port.second))return 1;
        QJSEngine::setObjectOwnership(port.first,QJSEngine::CppOwnership);
    }
    const QJSValue root=engine.newQObject(&protocol);
    engine.globalObject().setProperty(QStringLiteral("Protocol1"),root);
    engine.globalObject().setProperty(QStringLiteral("Protocol"),root);
    const QJSValue answer=engine.evaluate(QStringLiteral(R"JS(
        function check(v,message){if(!v)throw Error(message);}
        check(Protocol===Protocol1 && Protocol1.simulation,'alias and Simulation');
        check(Protocol1.UART.configure({tx:0,rx:1,baudRate:115200}), 'UART config');
        check(Protocol1.UART.startRx(), 'UART RX');
        check(Protocol1.UART.write([0,255,65]).ok, 'binary TX');
        var u=Protocol1.UART.read(16);
        check(u.simulated && u.count===0 && u.data.length===0,'UART no invented input');
        check(Protocol1.SPI.configure({clock:0,mosi:1,miso:2,select:3}), 'SPI config');
        var s=Protocol1.SPI.transfer([0x9f,0,0,0]);
        check(s.ok && s.simulated && s.rx.length===4,'SPI buffer shape');
        check(Protocol1.SPI.command(8,0x9f,0,[],3).rx.length===3,'SPI command');
        check(Protocol1.I2C.configure({scl:0,sda:1}),'I2C config');
        var i=Protocol1.I2C.writeRead(0x48,[0],2);
        check(i.ok && i.simulated && !i.acknowledged && i.nackIndex===-1,'no fake I2C ACK');
        check(Protocol1.I2C.Receiver() && Protocol1.I2C.spyPoll().data.length===0,'I2C Spy');
        check(Protocol1.CAN.configure({tx:0,rx:1}), 'CAN config');
        check(Protocol1.CAN.startRx(),'CAN RX start');
        check(Protocol1.CAN.send(0x123,false,false,[1,2,3]).simulated,'CAN send');
        check(!Protocol1.CAN.receive().received,'no fake CAN frames');
        check(Protocol1.SWD.configure({clock:0,data:1}), 'SWD config');
        check(Protocol1.SWD.clear(),'SWD line reset');
        var d=Protocol1.SWD.readRegister(false,0);
        check(d.simulated && d.ack===0,'no fake SWD ACK');
        check(Protocol1.stop(),'stop');
        check(!Protocol1.SWD.configured && !Protocol1.UART.configured,'state reset');
        check(!Protocol1.SPI.transfer([1]).ok,'unconfigured port reports error');
        'PASS';
    )JS"),QStringLiteral("protocol_bridge_smoke.js"));
    if(answer.isError() || answer.toString()!=QStringLiteral("PASS")) {
        qCritical().noquote()<<"Protocol JS smoke failed"<<answer.toString()<<answer.property(QStringLiteral("stack")).toString();
        return 1;
    }
    for(const QString& path : {QStringLiteral("Protocol1.I2C.writeRead"),QStringLiteral("Protocol1.UART.read"),
                               QStringLiteral("Protocol1.SPI.command"),QStringLiteral("Protocol1.CAN.receive"),
                               QStringLiteral("Protocol1.SWD.readRegister")}) {
        if(!ApiKnowledge::isDocumentedSymbol(path)) {
            qCritical()<<"Missing API Explorer help"<<path;return 1;
        }
    }
    qInfo()<<"PASS: native protocol QObject methods, QJSEngine simulation, result shapes and API catalog";
    return 0;
}
