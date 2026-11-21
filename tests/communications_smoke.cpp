// Native Qt6/QJSEngine communications smoke: localhost client/server,
// byte-exact payloads, line framing, explicit startup and cleanup.
#include "CommunicationBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QDebug>

static int failed=0;
static void expect(bool ok,const char* label) {
    if(!ok){qCritical()<<"FAIL:"<<label;++failed;}
    else qInfo()<<"PASS:"<<label;
}
static bool eval(QJSEngine& engine,const QString& source) {
    const QJSValue result=engine.evaluate(source,QStringLiteral("<native-communications-test>"));
    if(result.isError()){qCritical()<<"Script error"<<result.toString()<<result.property("stack").toString();return false;}
    return true;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    CommunicationBridge bridge(&engine);
    QJSEngine::setObjectOwnership(&bridge,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("__commHost"),engine.newQObject(&bridge));
    expect(eval(engine,CommunicationBridge::bootstrapSource()),"load native JavaScript wrappers");
    expect(eval(engine,QStringLiteral(R"JS(
        var ports=Serial.ports();
        if(!Array.isArray(ports))throw Error('Serial ports must be a JS array');
        var badSerialRejected=false;
        try { Serial.create({port:''}); } catch(e) { badSerialRejected=true; }
        if(!badSerialRejected)throw Error('invalid serial accepted');
        var badByteRejected=false;
        var tmp=UDP.create({host:'127.0.0.1',port:0});
        try {tmp.send([0,256],'127.0.0.1',9001);}catch(e){badByteRejected=true;}
        tmp.close();
        if(!badByteRejected)throw Error('invalid byte accepted');
        var tcpOK=false,udpOK=false,udpTextOK=false,udpBinaryOK=false,errors=[];
        var server=TCP.createServer({host:'127.0.0.1',port:0});
        if(server.info().open)throw Error('server bound too early');
        server.on('error',function(e){errors.push('server: '+e);});
        server.on('connection',function(peer){
            peer.on('data',function(bytes){peer.write(bytes);});
        });
        server.on('listening',function(){
            var client=TCP.create({host:'127.0.0.1',port:server.info().localPort});
            var textOK=false,binaryOK=false;
            var bytes=[];
            client.on('error',function(e){errors.push('tcp: '+e);});
            client.on('line',function(t){
                if(t==='HELLO')textOK=true;
                if(textOK && binaryOK)tcpOK=true;
            });
            client.on('data',function(chunk){
                bytes=bytes.concat(chunk);
                if(bytes.length>=6){
                    var expected=[72,69,76,76,79,10];
                    for(var i=0;i<6;i++)if(bytes[i]!==expected[i])throw Error('TCP bytes differ');
                    // binary is sent after text, potentially coalesced.
                    if(bytes.length>=11){
                        var expectedBinary=[0,1,127,128,255];
                        for(var j=0;j<5;j++)if(bytes[j+6]!==expectedBinary[j])throw Error('TCP binary differs');
                        binaryOK=true;
                    }
                }
                if(textOK && binaryOK)tcpOK=true;
            });
            client.on('connect',function(){client.write('HELLO\n');client.write([0,1,127,128,255]);});
            client.connect();
        });
        server.listen();
        var receiver=UDP.create({host:'127.0.0.1',port:0});
        var sender=UDP.create({host:'127.0.0.1',port:0});
        receiver.on('error',function(e){errors.push('udp rx: '+e);});
        sender.on('error',function(e){errors.push('udp tx: '+e);});
        receiver.on('message',function(chunk,source){receiver.send(chunk,source.address,source.port);});
        sender.on('message',function(chunk){
            if(chunk.length!==5)return; // text datagram is checked by text event
            var expected=[0,1,127,128,255];
            for(var j=0;j<5;j++)if(chunk[j]!==expected[j])throw Error('UDP payload');
            udpBinaryOK=true;udpOK=udpBinaryOK&&udpTextOK;
        });
        sender.on('text',function(text){
            if(text==='HELLO UDP'){udpTextOK=true;udpOK=udpBinaryOK&&udpTextOK;}
        });
        receiver.bind();sender.bind();
        sender.send('HELLO UDP','127.0.0.1',receiver.info().localPort);
        sender.send([0,1,127,128,255],'127.0.0.1',receiver.info().localPort);
    )JS")),"script creation and explicit startup");
    QElapsedTimer t;t.start();
    while(t.elapsed()<5000 && (!engine.globalObject().property("tcpOK").toBool() ||
                                 !engine.globalObject().property("udpOK").toBool())) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,40);
        QThread::msleep(2);
    }
    expect(engine.globalObject().property("tcpOK").toBool(),"TCP text+binary localhost echo");
    expect(engine.globalObject().property("udpOK").toBool(),"UDP binary localhost echo");
    const QJSValue errors=engine.globalObject().property("errors");
    expect(errors.isArray() && errors.property("length").toInt()==0,"no async communication errors");
    bridge.shutdown();
    expect(!bridge.start(1),"communications disabled after shutdown");
    return failed ? 1:0;
}
