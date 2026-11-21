#include "CommunicationBridge.h"
#include "BinaryBridge.h"
#include "ScriptValueConversion.h"
#include <QJSEngine>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QIODevice>
#include <QTimer>
#include <QMetaType>
#include <QElapsedTimer>
#include <QtGlobal>
#include <cmath>
#include <limits>

namespace {
constexpr int kMaxConnections=32;
constexpr int kMaxBytes=64*1024;
constexpr int kMaxQueued=256*1024;
constexpr int kMaxLine=8192;
constexpr int kMaxEventListeners=8;
QVariantList bytesList(const QByteArray& bytes) {
    QVariantList r;r.reserve(bytes.size());
    for (char b:bytes)r.append(int(static_cast<unsigned char>(b)));
    return r;
}
QString optionText(const QVariantMap& p,const char* key,const QString& fallback={}) {
    return p.value(QString::fromLatin1(key),fallback).toString();
}
int optionInt(const QVariantMap& p,const char* key,int fallback) {
    return p.value(QString::fromLatin1(key),fallback).toInt();
}
QString kindNameStatic(int kind) {return kind==0?QStringLiteral("serial"):kind==1?QStringLiteral("tcp"):kind==2?QStringLiteral("server"):QStringLiteral("udp");}
}
CommunicationBridge::CommunicationBridge(QJSEngine* engine,BinaryBridge* binary,QObject* parent):QObject(parent),m_engine(engine),m_binary(binary) {}
CommunicationBridge::~CommunicationBridge(){shutdown();}
QString CommunicationBridge::kindName(Kind k){return kindNameStatic(static_cast<int>(k));}
bool CommunicationBridge::reject(const QString& message){m_lastError=message;return false;}
QVariantMap CommunicationBridge::opts(const QJSValue& value){
    if(!value.isObject() || value.isCallable() || value.isArray())return {};
    return value.toVariant(QJSValue::ConvertJSObjects).toMap();
}
QJSValue CommunicationBridge::ports() const {
    QVariantList list;
    for(const QSerialPortInfo& p:QSerialPortInfo::availablePorts()) {
        list.append(QVariantMap{{QStringLiteral("port"),p.portName()},
            {QStringLiteral("description"),p.description()},
            {QStringLiteral("manufacturer"),p.manufacturer()},
            {QStringLiteral("systemLocation"),p.systemLocation()},
            {QStringLiteral("serialNumber"),p.serialNumber()}});
    }
    return ScriptValueConversion::toJavaScript(m_engine,list);
}
int CommunicationBridge::create(Kind kind,const QJSValue& options){
    m_lastError.clear();
    if(m_stopped || !m_engine){reject(QStringLiteral("Communication service stopped"));return 0;}
    if(m_entries.size()>=kMaxConnections){reject(QStringLiteral("At most 32 active communications objects"));return 0;}
    if(!options.isObject() || options.isArray() || options.isCallable()){
        reject(QStringLiteral("create(options) requires an object"));return 0;
    }
    const QVariantMap p=opts(options);
    if(kind==Kind::Serial){
        if(optionText(p,"port").trimmed().isEmpty()){
            reject(QStringLiteral("Serial.create requires a port name (use Serial.ports())"));return 0;
        }
        const int rate=optionInt(p,"baudRate",115200), bits=optionInt(p,"dataBits",8);
        const int stop=optionInt(p,"stopBits",1);
        const QString parity=optionText(p,"parity",QStringLiteral("none")).toLower();
        const QString flow=optionText(p,"flowControl",QStringLiteral("none")).toLower();
        if(rate<50 || rate>4000000 || (bits!=5 && bits!=6 && bits!=7 && bits!=8) ||
           (stop!=1 && stop!=2) ||
           !QStringList{QStringLiteral("none"),QStringLiteral("even"),QStringLiteral("odd")}.contains(parity) ||
           !QStringList{QStringLiteral("none"),QStringLiteral("hardware"),QStringLiteral("software")}.contains(flow)) {
            reject(QStringLiteral("Invalid serial baudRate/dataBits/stopBits/parity/flowControl"));return 0;
        }
    }
    if(kind==Kind::Tcp){
        if(optionText(p,"host").trimmed().isEmpty() || optionInt(p,"port",0)<1 || optionInt(p,"port",0)>65535){
            reject(QStringLiteral("TCP.create requires host and port 1..65535"));return 0;
        }
    }
    if(kind==Kind::Udp || kind==Kind::Server) {
        if(optionInt(p,"port",0)<0 || optionInt(p,"port",0)>65535){
            reject(QStringLiteral("Local bind/listen port must be 0..65535"));return 0;
        }
        QHostAddress address;
        if(!address.setAddress(optionText(p,"host",QStringLiteral("127.0.0.1")))) {
            reject(QStringLiteral("Bind host must be a numeric local IP address"));return 0;
        }
    }
    auto* e=new Entry;
    e->kind=kind;e->options=p;e->id=m_next++;
    if(kind==Kind::Serial){
        auto* device=new QSerialPort(this);
        device->setPortName(optionText(p,"port"));
        device->setBaudRate(optionInt(p,"baudRate",115200));
        device->setDataBits(static_cast<QSerialPort::DataBits>(optionInt(p,"dataBits",8)));
        device->setStopBits(optionInt(p,"stopBits",1)==2?QSerialPort::TwoStop:QSerialPort::OneStop);
        const QString parity=optionText(p,"parity",QStringLiteral("none")).toLower();
        device->setParity(parity==QStringLiteral("even")?QSerialPort::EvenParity:parity==QStringLiteral("odd")?QSerialPort::OddParity:QSerialPort::NoParity);
        const QString flow=optionText(p,"flowControl",QStringLiteral("none")).toLower();
        device->setFlowControl(flow==QStringLiteral("hardware")?QSerialPort::HardwareControl:flow==QStringLiteral("software")?QSerialPort::SoftwareControl:QSerialPort::NoFlowControl);
        device->setReadBufferSize(kMaxBytes);
        e->object=device;
    } else if(kind==Kind::Tcp) {
        auto* socket=new QTcpSocket(this);
        socket->setReadBufferSize(kMaxBytes);
        e->object=socket;
    } else if(kind==Kind::Server) e->object=new QTcpServer(this);
    else e->object=new QUdpSocket(this);
    m_entries.insert(e->id,e);
    if(kind==Kind::Serial){
        auto* port=qobject_cast<QSerialPort*>(e->object);
        watchStream(e,port);
        connect(port,&QSerialPort::errorOccurred,this,[this,id=e->id,port](QSerialPort::SerialPortError err){
            if(err!=QSerialPort::NoError && m_entries.contains(id))fail(id,port->errorString());
        });
    } else if(kind==Kind::Tcp){watchTcp(e,qobject_cast<QTcpSocket*>(e->object));}
    else if(kind==Kind::Server){
        auto* server=qobject_cast<QTcpServer*>(e->object);
        connect(server,&QTcpServer::newConnection,this,[this,id=e->id,server](){
            while(server->hasPendingConnections()){
                QTcpSocket* socket=server->nextPendingConnection();
                if(!socket)break;
                if(m_stopped || m_entries.size()>=kMaxConnections){socket->abort();socket->deleteLater();fail(id,QStringLiteral("TCP client limit reached"));continue;}
                socket->setParent(this);socket->setReadBufferSize(kMaxBytes);
                auto* accepted=new Entry;
                accepted->id=m_next++; accepted->kind=Kind::Tcp; accepted->object=socket;
                accepted->options={{QStringLiteral("accepted"),true}};
                m_entries.insert(accepted->id,accepted);
                watchTcp(accepted,socket);
                dispatch(id,QStringLiteral("connection"),{accepted->id});
                if(!m_entries.contains(id))return; // listener may stop server
            }
        });
        connect(server,&QTcpServer::acceptError,this,[this,id=e->id,server](QAbstractSocket::SocketError){fail(id,server->errorString());});
    } else {
        auto* udp=qobject_cast<QUdpSocket*>(e->object);
        connect(udp,&QUdpSocket::readyRead,this,[this,id=e->id](){onUdpData(id);});
        connect(udp,&QUdpSocket::errorOccurred,this,[this,id=e->id,udp](QAbstractSocket::SocketError){fail(id,udp->errorString());});
    }
    return e->id;
}
int CommunicationBridge::createSerial(const QJSValue& o){return create(Kind::Serial,o);}
int CommunicationBridge::createTcp(const QJSValue& o){return create(Kind::Tcp,o);}
int CommunicationBridge::createServer(const QJSValue& o){return create(Kind::Server,o);}
int CommunicationBridge::createUdp(const QJSValue& o){return create(Kind::Udp,o);}
bool CommunicationBridge::on(int id,const QString& event,const QJSValue& cb){
    m_lastError.clear();
    Entry* e=m_entries.value(id,nullptr);
    if(!e)return reject(QStringLiteral("Invalid or closed connection handle"));
    static const QStringList events={QStringLiteral("open"),QStringLiteral("connect"),QStringLiteral("listening"),QStringLiteral("connection"),QStringLiteral("data"),QStringLiteral("binary"),QStringLiteral("line"),QStringLiteral("message"),QStringLiteral("binaryMessage"),QStringLiteral("text"),QStringLiteral("error"),QStringLiteral("close")};
    if(!events.contains(event) || !cb.isCallable())return reject(QStringLiteral("Unknown event or callback not callable"));
    if(e->listeners[event].size()>=kMaxEventListeners)return reject(QStringLiteral("Maximum 8 callbacks per event"));
    e->listeners[event].append(cb);
    return true;
}
bool CommunicationBridge::off(int id,const QString& event){
    m_lastError.clear();Entry* e=m_entries.value(id,nullptr);
    if(!e)return reject(QStringLiteral("Invalid or closed connection handle"));
    e->listeners.remove(event);return true;
}
void CommunicationBridge::dispatch(int id,const QString& event,const QVariantList& args){
    Entry* e=m_entries.value(id,nullptr);
    if(m_stopped || !m_engine || !e)return;
    const QList<QJSValue> callbacks=e->listeners.value(event);
    const QString kind=kindName(e->kind);
    QJSValueList jsargs;
    for(const QVariant& arg:args)jsargs.append(ScriptValueConversion::toJavaScript(m_engine,arg));
    for(const QJSValue& cb:callbacks){
        if(m_stopped || !m_entries.contains(id))return;
        emit callbackStarted();
        const QJSValue result=cb.call(jsargs);
        emit callbackFinished();
        if(result.isError())emit callbackFailed(QStringLiteral("Communications %1[%2] %3 callback").arg(kind).arg(id).arg(event),result);
    }
}
void CommunicationBridge::dispatchNative(int id,const QString& event,BinaryBuffer* buffer,const QVariantMap* remote){
    Entry* e=m_entries.value(id,nullptr);
    if(m_stopped || !m_engine || !e || !buffer)return;
    const QList<QJSValue> callbacks=e->listeners.value(event);
    QJSValueList args{m_engine->newQObject(buffer)};
    if(remote)args.append(ScriptValueConversion::toJavaScript(m_engine,*remote));
    for(const QJSValue& cb:callbacks){
        if(m_stopped || !m_entries.contains(id))return;
        emit callbackStarted();
        const QJSValue result=cb.call(args);
        emit callbackFinished();
        if(result.isError())emit callbackFailed(QStringLiteral("Communications native %1[%2] %3 callback")
                .arg(kindName(e->kind)).arg(id).arg(event),result);
    }
}
void CommunicationBridge::fail(int id,const QString& message){
    m_lastError=message;
    dispatch(id,QStringLiteral("error"),{message});
}
void CommunicationBridge::watchStream(Entry* e,QIODevice* dev){
    connect(dev,&QIODevice::readyRead,this,[this,id=e->id](){onStreamData(id);});
}
void CommunicationBridge::watchTcp(Entry* e,QTcpSocket* socket){
    watchStream(e,socket);
    connect(socket,&QTcpSocket::connected,this,[this,id=e->id](){dispatch(id,QStringLiteral("connect"));});
    connect(socket,&QTcpSocket::disconnected,this,[this,id=e->id](){
        Entry* entry=m_entries.value(id,nullptr);
        if(entry && !entry->closeEmitted){entry->closeEmitted=true;dispatch(id,QStringLiteral("close"));}
    });
    connect(socket,&QTcpSocket::errorOccurred,this,[this,id=e->id,socket](QAbstractSocket::SocketError){fail(id,socket->errorString());});
}
void CommunicationBridge::onStreamData(int id){
    Entry* e=m_entries.value(id,nullptr);
    if(!e)return;
    auto* dev=qobject_cast<QIODevice*>(e->object);
    if(!dev)return;
    // Reads are bounded even if a sender floods the port. Qt's socket/serial
    // read buffers are also limited (64KiB); no unbounded native queue.
    const QByteArray data=dev->read(kMaxBytes);
    if(data.isEmpty())return;
    e->received+=data.size();
    // Legacy event is preserved exactly. Avoid paying for its per-byte
    // QVariantList/QJS array conversion when only native subscribers exist.
    if(!e->listeners.value(QStringLiteral("data")).isEmpty()){
        e->legacyBytes+=data.size();++e->legacyEvents;
        QElapsedTimer timing;timing.start();
        dispatch(id,QStringLiteral("data"),{QVariant(bytesList(data))});
        if(!m_entries.contains(id))return;
        e=m_entries.value(id);
        e->legacyDispatchUs+=timing.nsecsElapsed()/1000;
    }
    if(m_binary && !e->listeners.value(QStringLiteral("binary")).isEmpty()){
        e->nativeBytes+=data.size();++e->nativeEvents;
        QElapsedTimer timing;timing.start();
        auto* chunk=m_binary->fromNativeBytes(data);
        if(!chunk){fail(id,m_binary->lastError());return;}
        dispatchNative(id,QStringLiteral("binary"),chunk);
        if(!m_entries.contains(id))return;
        e=m_entries.value(id);
        e->nativeDispatchUs+=timing.nsecsElapsed()/1000;
    }
    if(e->listeners.value(QStringLiteral("line")).isEmpty())return;
    e->lineBuffer.append(data);
    while(true){
        const int at=e->lineBuffer.indexOf('\n');
        if(at<0)break;
        if(at>kMaxLine){e->lineBuffer.clear();fail(id,QStringLiteral("Incoming line exceeds 8192 bytes"));return;}
        QByteArray line=e->lineBuffer.left(at);
        e->lineBuffer.remove(0,at+1);
        if(line.endsWith('\r'))line.chop(1);
        dispatch(id,QStringLiteral("line"),{QString::fromUtf8(line)});
        if(!m_entries.contains(id))return;
        e=m_entries.value(id);
    }
    if(e->lineBuffer.size()>kMaxLine){e->lineBuffer.clear();fail(id,QStringLiteral("Incoming line exceeds 8192 bytes"));}
}
void CommunicationBridge::onUdpData(int id){
    Entry* e=m_entries.value(id,nullptr);
    if(!e || e->kind!=Kind::Udp)return;
    auto* udp=qobject_cast<QUdpSocket*>(e->object);
    int count=0;
    while(udp->hasPendingDatagrams() && ++count<=64){
        const qint64 size=udp->pendingDatagramSize();
        if(size<0)break;
        if(size>65507){QByteArray discard;discard.resize(int(qMin<qint64>(size,65536)));
            udp->readDatagram(discard.data(),discard.size());fail(id,QStringLiteral("UDP datagram exceeds maximum"));continue;}
        QByteArray data;data.resize(int(size));
        QHostAddress peer;quint16 port=0;
        const qint64 n=udp->readDatagram(data.data(),data.size(),&peer,&port);
        if(n<0){fail(id,udp->errorString());continue;}
        data.truncate(int(n));e->received+=n;
        const QVariantMap remote={{QStringLiteral("address"),peer.toString()},{QStringLiteral("port"),int(port)}};
        if(!e->listeners.value(QStringLiteral("message")).isEmpty())
            dispatch(id,QStringLiteral("message"),{QVariant(bytesList(data)),remote});
        if(!m_entries.contains(id))return;
        e=m_entries.value(id);
        if(m_binary && !e->listeners.value(QStringLiteral("binaryMessage")).isEmpty()){
            auto* chunk=m_binary->fromNativeBytes(data);
            if(!chunk){fail(id,m_binary->lastError());return;}
            dispatchNative(id,QStringLiteral("binaryMessage"),chunk,&remote);
        }
        if(!m_entries.contains(id))return;
        e=m_entries.value(id);
        if(!e->listeners.value(QStringLiteral("text")).isEmpty())
            dispatch(id,QStringLiteral("text"),{QString::fromUtf8(data),remote});
        if(!m_entries.contains(id))return;
        e=m_entries.value(id);
    }
    // Cap processing per readiness event to keep UI/Scope responsive.
    if(m_entries.contains(id) && udp->hasPendingDatagrams())
        QTimer::singleShot(0,this,[this,id](){onUdpData(id);});
}
bool CommunicationBridge::start(int id){
    m_lastError.clear();Entry* e=m_entries.value(id,nullptr);
    if(!e)return reject(QStringLiteral("Invalid or closed connection handle"));
    if(e->kind==Kind::Serial){
        auto* p=qobject_cast<QSerialPort*>(e->object);
        if(p->isOpen())return true;
        if(!p->open(QIODevice::ReadWrite))return reject(p->errorString());
        // open callback is asynchronous so on('open',...) registered before
        // .open() is always honored without reentrant script execution.
        QTimer::singleShot(0,this,[this,id](){dispatch(id,QStringLiteral("open"));});
        return true;
    }
    if(e->kind==Kind::Tcp){
        auto* p=qobject_cast<QTcpSocket*>(e->object);
        if(p->state()==QAbstractSocket::ConnectedState || p->state()==QAbstractSocket::ConnectingState)return true;
        p->connectToHost(optionText(e->options,"host"),quint16(optionInt(e->options,"port",0)));
        return true;
    }
    const QHostAddress address(optionText(e->options,"host",QStringLiteral("127.0.0.1")));
    const quint16 port=quint16(optionInt(e->options,"port",0));
    if(e->kind==Kind::Server){
        auto* p=qobject_cast<QTcpServer*>(e->object);
        if(p->isListening())return true;
        if(!p->listen(address,port))return reject(p->errorString());
    } else {
        auto* p=qobject_cast<QUdpSocket*>(e->object);
        if(p->state()==QAbstractSocket::BoundState)return true;
        if(!p->bind(address,port))return reject(p->errorString());
    }
    QTimer::singleShot(0,this,[this,id](){dispatch(id,QStringLiteral("listening"));});
    return true;
}
bool CommunicationBridge::bytes(const QJSValue& value,QByteArray* out,QString* error){
    out->clear();
    if(value.isQObject()){
        auto* buffer=qobject_cast<BinaryBuffer*>(value.toQObject());
        if(!buffer || buffer->isReleased()){*error=QStringLiteral("Expected live BinaryBuffer");return false;}
        if(buffer->length()>kMaxBytes){*error=QStringLiteral("Write exceeds 65536 bytes");return false;}
        *out=buffer->bytesCopy();return true;
    }
    if(value.isString()){
        *out=value.toString().toUtf8();
        if(out->size()>kMaxBytes){*error=QStringLiteral("Write exceeds 65536 bytes");return false;}
        return true;
    }
    if(!value.isArray()) {*error=QStringLiteral("Payload must be UTF-8 text or an array of bytes 0..255");return false;}
    const int count=value.property(QStringLiteral("length")).toInt();
    if(count<0 || count>kMaxBytes){*error=QStringLiteral("Write exceeds 65536 bytes");return false;}
    out->reserve(count);
    for(int i=0;i<count;++i){
        QJSValue v=value.property(quint32(i));
        const double n=v.toNumber();
        if(!v.isNumber() || !std::isfinite(n) || std::floor(n)!=n || n<0 || n>255){
            *error=QStringLiteral("Binary payload has invalid byte at index %1").arg(i);out->clear();return false;
        }
        out->append(char(int(n)));
    }
    return true;
}
int CommunicationBridge::write(int id,const QJSValue& payload){
    m_lastError.clear();Entry* e=m_entries.value(id,nullptr);
    if(!e || (e->kind!=Kind::Serial && e->kind!=Kind::Tcp)){reject(QStringLiteral("write() requires an open serial or TCP connection"));return -1;}
    QIODevice* dev=qobject_cast<QIODevice*>(e->object);
    if(!dev || !dev->isWritable()){reject(QStringLiteral("Connection is not open for writing"));return -1;}
    QByteArray data;QString err;
    if(!bytes(payload,&data,&err)){reject(err);return -1;}
    if(dev->bytesToWrite()+data.size()>kMaxQueued){reject(QStringLiteral("Outbound queue exceeds 256 KiB"));return -1;}
    const qint64 n=dev->write(data);
    if(n<0){reject(dev->errorString());return -1;}
    e->sent+=n;
    return int(n);
}
int CommunicationBridge::send(int id,const QJSValue& payload,const QString& host,int port){
    m_lastError.clear();Entry* e=m_entries.value(id,nullptr);
    if(!e || e->kind!=Kind::Udp){reject(QStringLiteral("send() requires a UDP socket"));return -1;}
    if(port<1 || port>65535){reject(QStringLiteral("UDP port must be 1..65535"));return -1;}
    QHostAddress peer;
    if(!peer.setAddress(host)){reject(QStringLiteral("UDP destination must be a numeric IP address"));return -1;}
    QByteArray data;QString err;
    if(!bytes(payload,&data,&err)){reject(err);return -1;}
    if(data.size()>65507){reject(QStringLiteral("UDP datagram exceeds 65507 bytes"));return -1;}
    auto* udp=qobject_cast<QUdpSocket*>(e->object);
    // send is allowed without bind: OS chooses source port; receive uses bind.
    const qint64 n=udp->writeDatagram(data,peer,quint16(port));
    if(n<0){reject(udp->errorString());return -1;}
    e->sent+=n;return int(n);
}
bool CommunicationBridge::close(int id){
    m_lastError.clear();Entry* e=m_entries.value(id,nullptr);
    if(!e)return reject(QStringLiteral("Invalid or already closed handle"));
    if(e->closing)return true;
    e->closing=true;
    if(!e->closeEmitted && !m_stopped){e->closeEmitted=true;dispatch(id,QStringLiteral("close"));}
    // Callbacks may invoke close recursively; closing flag prevents recursion.
    m_entries.remove(id);
    if(auto* tcp=qobject_cast<QTcpSocket*>(e->object))tcp->abort();
    else if(auto* server=qobject_cast<QTcpServer*>(e->object))server->close();
    else if(auto* serial=qobject_cast<QSerialPort*>(e->object))serial->close();
    else if(auto* udp=qobject_cast<QUdpSocket*>(e->object))udp->close();
    // Defer deletion when close() is called from the object's own readyRead
    // or connected signal. Callbacks cannot observe the removed handle.
    e->object->deleteLater();
    delete e;return true;
}
QJSValue CommunicationBridge::info(int id) const {
    QVariantMap result;
    const Entry* e=m_entries.value(id,nullptr);
    if(!e)return ScriptValueConversion::toJavaScript(m_engine,QVariantMap{{QStringLiteral("valid"),false},{QStringLiteral("error"),QStringLiteral("Closed handle")}});
    bool open=false;int local=0,peer=0;
    if(auto* s=qobject_cast<QSerialPort*>(e->object))open=s->isOpen();
    else if(auto* t=qobject_cast<QTcpSocket*>(e->object)){
        open=t->state()==QAbstractSocket::ConnectedState;local=int(t->localPort());peer=int(t->peerPort());
    } else if(auto* s=qobject_cast<QTcpServer*>(e->object)){
        open=s->isListening();local=int(s->serverPort());
    } else if(auto* u=qobject_cast<QUdpSocket*>(e->object)){
        open=u->state()==QAbstractSocket::BoundState;local=int(u->localPort());
    }
    result={{QStringLiteral("valid"),true},{QStringLiteral("open"),open},
        {QStringLiteral("kind"),kindName(e->kind)},
        {QStringLiteral("localPort"),local},{QStringLiteral("peerPort"),peer},
        {QStringLiteral("bytesReceived"),e->received},{QStringLiteral("bytesSent"),e->sent},
        {QStringLiteral("legacyBytes"),e->legacyBytes},{QStringLiteral("legacyEvents"),e->legacyEvents},
        {QStringLiteral("legacyDispatchUs"),e->legacyDispatchUs},
        {QStringLiteral("nativeBytes"),e->nativeBytes},{QStringLiteral("nativeEvents"),e->nativeEvents},
        {QStringLiteral("nativeDispatchUs"),e->nativeDispatchUs}};
    return ScriptValueConversion::toJavaScript(m_engine,result);
}
void CommunicationBridge::shutdown(){
    if(m_stopped)return;
    m_stopped=true;
    const QList<int> ids=m_entries.keys();
    for(int id:ids)if(m_entries.contains(id))close(id);
    m_engine=nullptr;
}
QString CommunicationBridge::bootstrapSource(){return QStringLiteral(R"JS(
// WFScript native host communications. No Node.js runtime or implicit I/O.
function __commCheck(ok) { if(!ok)throw Error(__commHost.lastError); return true; }
function __commWrap(id,kind) {
    var obj={
        id:id,
        on:function(event,callback) {
            if(typeof callback!=='function')throw Error('Callback required');
            if(kind==='server' && event==='connection') {
                __commCheck(__commHost.on(id,event,function(clientId){callback(__commWrap(clientId,'tcp'));}));
            } else __commCheck(__commHost.on(id,event,callback));
            return obj;
        },
        off:function(event){__commCheck(__commHost.off(id,event));return obj;},
        open:function(){__commCheck(__commHost.start(id));return obj;},
        connect:function(){__commCheck(__commHost.start(id));return obj;},
        listen:function(){__commCheck(__commHost.start(id));return obj;},
        bind:function(){__commCheck(__commHost.start(id));return obj;},
        write:function(value){var n=__commHost.write(id,value);if(n<0)throw Error(__commHost.lastError);return n;},
        send:function(value,host,port){var n=__commHost.send(id,value,String(host),Number(port));if(n<0)throw Error(__commHost.lastError);return n;},
        close:function(){__commCheck(__commHost.close(id));return true;},
        info:function(){return __commHost.info(id);}
    };
    return obj;
}
var Serial={
    ports:function(){return __commHost.ports();},
    create:function(options){var id=__commHost.createSerial(options||{});if(!id)throw Error(__commHost.lastError);return __commWrap(id,'serial');}
};
var TCP={
    create:function(options){var id=__commHost.createTcp(options||{});if(!id)throw Error(__commHost.lastError);return __commWrap(id,'tcp');},
    createServer:function(options){var id=__commHost.createServer(options||{});if(!id)throw Error(__commHost.lastError);return __commWrap(id,'server');}
};
var UDP={
    create:function(options){var id=__commHost.createUdp(options||{});if(!id)throw Error(__commHost.lastError);return __commWrap(id,'udp');}
};
)JS");}
