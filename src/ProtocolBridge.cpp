#include "ProtocolBridge.h"
#include "DwfRuntime.h"
#include <QByteArray>
#include <QDateTime>
#include <QSet>
#include <QHash>
#include <QJSEngine>
#include "ScriptValueConversion.h"
#include <limits>

namespace {
QVariantList textBytes(const QString& value) {
    QVariantList data;
    const QByteArray bytes=value.toUtf8();data.reserve(bytes.size());
    for(char b:bytes)data.append(int(static_cast<unsigned char>(b)));
    return data;
}
QString bytesText(const QVariantList& data) {
    QByteArray bytes;bytes.reserve(data.size());
    for(const auto& x:data)bytes.append(char(x.toInt()&0xff));
    return QString::fromUtf8(bytes);
}
QVariantMap withDefaults(QVariantMap result) {
    result.insert(QStringLiteral("simulated"),true);
    result.insert(QStringLiteral("ok"),true);
    return result;
}
}
ProtocolPortBase::ProtocolPortBase(const QString& kind,DwfRuntime* runtime,QObject* parent)
    :QObject(parent),m_runtime(runtime),m_kind(kind){}
void ProtocolPortBase::setError(const QString& value) {
    if(m_error==value)return;
    m_error=value;emit lastErrorChanged();
}
QVariantMap ProtocolPortBase::defaultResult() {return {{QStringLiteral("ok"),true}};}
QVariantMap ProtocolPortBase::failed(const QString& reason) {
    setError(reason);return {{QStringLiteral("ok"),false},{QStringLiteral("error"),reason}};
}
void ProtocolPortBase::invalidate() {
    if(m_configured){m_configured=false;emit configuredChanged();}
}
bool ProtocolPortBase::stopPort() {
    if(!m_runtime) {invalidate();setError({});return true;}
    if(m_runtime->protocolOwner()!=m_kind) {invalidate();return true;}
    QString error;
    const bool ok=m_runtime->protocolStop(&error);
    if(ok) {invalidate();setError({});}
    else setError(error);
    return ok;
}
bool ProtocolPortBase::configurePort(const QVariantMap& settings) {
    setError({});
    // One protocol at a time, enforced by the DWF backend in hardware mode.
    if(!m_runtime) {if(!m_configured){m_configured=true;emit configuredChanged();}return true;}
    QVariantMap result;QString error;
    const bool ok=m_runtime->protocolExecute(m_kind,QStringLiteral("configure"),settings,&result,&error);
    if(!ok){setError(error);invalidate();return false;}
    if(!m_configured){m_configured=true;emit configuredChanged();}
    return true;
}
QVariantMap ProtocolPortBase::execute(const QString& action,const QVariantMap& arguments) {
    if(!m_configured)return failed(m_kind+QStringLiteral(" is not configured; call configure(settings) first"));
    if(!m_runtime) {
        // Simulated transfer is clearly labeled and never presented as a real
        // measurement, ACK, or device response. Only TX echoes are returned.
        QVariantMap result=withDefaults({});
        if(action==QLatin1String("read") || action==QLatin1String("receive") ||
           action==QLatin1String("spyPoll")) {
            result.insert(QStringLiteral("data"),QVariantList{});
            result.insert(QStringLiteral("received"),false);
            result.insert(QStringLiteral("count"),0);
            if(m_kind==QLatin1String("SWD")) {
                result.insert(QStringLiteral("ack"),0);
                result.insert(QStringLiteral("ackName"),QStringLiteral("SIMULATION — NO TARGET"));
                result.insert(QStringLiteral("parityError"),false);
            }
            if(m_kind==QLatin1String("CAN")) result.insert(QStringLiteral("status"),0);
            if(m_kind==QLatin1String("I2C")) {
                result.insert(QStringLiteral("start"),0);
                result.insert(QStringLiteral("stop"),false);
                result.insert(QStringLiteral("nackIndex"),-1);
            }
        } else if(action==QLatin1String("transfer") || action==QLatin1String("command")) {
            QVariantList receive;
            const int count=qBound(0,arguments.value(QStringLiteral("readCount")).toInt(),65536);
            for(int i=0;i<count;++i)receive.append(0);
            result.insert(QStringLiteral("rx"),receive);
            result.insert(QStringLiteral("words"),receive.size());
        } else if(action==QLatin1String("writeRead")) {
            QVariantList receive;
            const int count=qBound(0,arguments.value(QStringLiteral("count")).toInt(),65536);
            for(int i=0;i<count;++i)receive.append(0);
            result.insert(QStringLiteral("data"),receive);
            result.insert(QStringLiteral("nackIndex"),-1);
            result.insert(QStringLiteral("acknowledged"),false);
        }
        if(action==QLatin1String("clear") && m_kind==QLatin1String("I2C"))result.insert(QStringLiteral("busFree"),false);
        if(action==QLatin1String("writeRead") && m_kind==QLatin1String("I2C"))result.insert(QStringLiteral("acknowledged"),false);
        return result;
    }
    // A stale port must NEVER stop a newer protocol's digital output engine.
    // This matters with timer-driven UART reads if ownership changes.
    if(m_runtime->protocolOwner()!=m_kind){
        const QString owner=m_runtime->protocolOwner();
        invalidate();
        return failed(QStringLiteral("%1 lost digital ownership; active protocol: %2")
            .arg(m_kind,owner.isEmpty()?QStringLiteral("none"):owner));
    }
    QVariantMap result;QString error;
    if(!m_runtime->protocolExecute(m_kind,action,arguments,&result,&error)) {
        // Do not silently leave physical protocol outputs active after a failed
        // transaction. A failed shutdown is included in the diagnostic.
        const QString cause=error;
        QString shutdown;
        const bool safe=m_runtime->protocolStop(&shutdown);
        invalidate();
        return failed(safe?cause:(cause+QStringLiteral("; DIGITAL OUTPUT SHUTDOWN UNCONFIRMED: ")+shutdown));
    }
    setError({});return result;
}

ProtocolUartPort::ProtocolUartPort(DwfRuntime* r,QObject* p):ProtocolPortBase(QStringLiteral("UART"),r,p){
    m_rxTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_rxTimer,&QTimer::timeout,this,&ProtocolUartPort::pollReceive);
}
void ProtocolUartPort::invalidate() {
    stopAutoRx();
    m_rxStarted=false;
    m_onData=QJSValue(); // callbacks must not outlive the configured session
    m_rxQueue.clear();
    m_sdkOverflowPending=false;m_parityErrorPending=0;
    emit queueChanged();
    ProtocolPortBase::invalidate();
}

bool ProtocolUartPort::configure(const QVariantMap& settings) {
    stopAutoRx();m_rxStarted=false;m_rxQueue.clear();m_sdkOverflowPending=false;m_parityErrorPending=0;
    m_receivedBytes=0;m_readBytes=0;m_queueDroppedBytes=0;m_sdkOverflows=0;m_parityErrors=0;
    emit queueChanged();
    QVariantMap s=settings;
    s.insert(QStringLiteral("baudRate"),settings.value(QStringLiteral("baudRate"),9600));
    s.insert(QStringLiteral("dataBits"),settings.value(QStringLiteral("dataBits"),8));
    s.insert(QStringLiteral("stopBits"),settings.value(QStringLiteral("stopBits"),1));
    QString parity=settings.value(QStringLiteral("parity"),QStringLiteral("none")).toString().toLower();
    const QHash<QString,int> parityModes={{QStringLiteral("none"),0},{QStringLiteral("even"),1},
        {QStringLiteral("odd"),2},{QStringLiteral("mark"),3},{QStringLiteral("space"),4}};
    if(!parityModes.contains(parity)) {setError(QStringLiteral("UART parity: none/even/odd/mark/space"));return false;}
    s.insert(QStringLiteral("parity"),parityModes.value(parity));
    s.insert(QStringLiteral("polarity"),settings.value(QStringLiteral("inverted"),false).toBool()?1:0);
    if(!s.contains(QStringLiteral("tx")) || !s.contains(QStringLiteral("rx"))) {setError(QStringLiteral("UART configure requires tx and rx DIO pins"));return false;}
    return configurePort(s);
}
bool ProtocolUartPort::startRx(){
    if(m_autoReceiving){setError(QStringLiteral("Stop automatic reception before restarting the SDK receiver"));return false;}
    const bool ok=execute(QStringLiteral("startRx")).value(QStringLiteral("ok")).toBool();
    if(ok)m_rxStarted=true;
    return ok;
}
QVariantMap ProtocolUartPort::write(const QVariantList& data) {return execute(QStringLiteral("write"),{{QStringLiteral("data"),data}});}
QVariantMap ProtocolUartPort::writeText(const QString& data){return write(textBytes(data));}
QVariantMap ProtocolUartPort::read(int maxBytes){
    // No parallel SDK reads while the Qt poller owns reception; the two modes
    // cannot steal bytes from one another.
    if(m_autoReceiving || !m_rxQueue.isEmpty())return queuedRead(maxBytes);
    return execute(QStringLiteral("read"),{{QStringLiteral("maxBytes"),maxBytes}});
}
bool ProtocolUartPort::Send(const QString& value,bool ending){return writeText(value+(ending?QStringLiteral("\n"):QString())).value(QStringLiteral("ok")).toBool();}
bool ProtocolUartPort::SendArray(const QVariantList& value,bool ending) {
    QVariantList bytes=value;if(ending)bytes.append(10);
    return write(bytes).value(QStringLiteral("ok")).toBool();
}
QVariantList ProtocolUartPort::ReceiveArray(){return read().value(QStringLiteral("data")).toList();}
QString ProtocolUartPort::Receive(){return bytesText(ReceiveArray());}

bool ProtocolUartPort::startAutoRx(const QVariantMap& options) {
    if(!configured()){setError(QStringLiteral("UART must be configured before startAutoRx"));return false;}
    const QSet<QString> allowed={QStringLiteral("pollIntervalMs"),QStringLiteral("bufferSize")};
    for(auto it=options.cbegin();it!=options.cend();++it)
        if(!allowed.contains(it.key())){setError(QStringLiteral("Unknown UART autoRx option: ")+it.key());return false;}
    bool intervalOk=true, capacityOk=true;
    const int interval=options.value(QStringLiteral("pollIntervalMs"),10).toInt(&intervalOk);
    const int capacity=options.value(QStringLiteral("bufferSize"),65536).toInt(&capacityOk);
    if(!intervalOk || interval<2 || interval>1000 || !capacityOk || capacity<1024 || capacity>1048576){
        setError(QStringLiteral("autoRx requires pollIntervalMs 2..1000, bufferSize 1024..1048576"));return false;
    }
    if(m_autoReceiving){setError(QStringLiteral("UART automatic receiver is already active"));return false;}
    if(!m_rxStarted && !startRx())return false;
    m_capacity=capacity;
    if(m_rxQueue.size()>m_capacity){
        m_queueDroppedBytes+=m_rxQueue.size()-m_capacity;
        m_rxQueue.remove(0,m_rxQueue.size()-m_capacity);
    }
    m_autoReceiving=true;
    m_rxTimer.start(interval);
    setError({});emit autoReceivingChanged();emit queueChanged();
    return true;
}
bool ProtocolUartPort::stopAutoRx() {
    if(!m_autoReceiving)return true;
    m_rxTimer.stop();m_autoReceiving=false;emit autoReceivingChanged();
    // SDK remains configured. Manual read() can resume; Protocol1.stop()
    // releases hardware and clears all in-memory state.
    return true;
}
bool ProtocolUartPort::onData(const QJSValue& callback) {
    if(!callback.isCallable()){setError(QStringLiteral("UART.onData requires a callable JavaScript function"));return false;}
    m_onData=callback;setError({});return true;
}
bool ProtocolUartPort::clearOnData(){m_onData=QJSValue();return true;}
QJSValue ProtocolUartPort::dispatchOnData(QJSEngine* engine,const QVariantMap& chunk) {
    if(!engine || !m_onData.isCallable())return QJSValue();
    return m_onData.call({ScriptValueConversion::toJavaScript(engine,chunk)});
}
QVariantMap ProtocolUartPort::stats() const {
    return {{QStringLiteral("autoReceiving"),m_autoReceiving},
        {QStringLiteral("pollIntervalMs"),m_autoReceiving?m_rxTimer.interval():0},
        {QStringLiteral("bufferCapacity"),m_capacity},
        {QStringLiteral("available"),m_rxQueue.size()},
        {QStringLiteral("receivedBytes"),m_receivedBytes},
        {QStringLiteral("readBytes"),m_readBytes},
        {QStringLiteral("queueDroppedBytes"),m_queueDroppedBytes},
        {QStringLiteral("sdkOverflows"),m_sdkOverflows},
        {QStringLiteral("parityErrors"),m_parityErrors},
        {QStringLiteral("simulated"),simulation()}};
}
QVariantMap ProtocolUartPort::queuedRead(int maxBytes) {
    if(maxBytes<1 || maxBytes>65536)return failed(QStringLiteral("UART read maxBytes must be 1..65536"));
    if(!configured())return failed(QStringLiteral("UART is not configured"));
    const QByteArray bytes=m_rxQueue.left(maxBytes);
    m_rxQueue.remove(0,bytes.size());
    m_readBytes+=bytes.size();
    QVariantList data;data.reserve(bytes.size());
    for(unsigned char b:bytes)data.append(int(b));
    const bool overflow=m_sdkOverflowPending;
    const int parity=m_parityErrorPending;
    m_sdkOverflowPending=false;m_parityErrorPending=0;
    emit queueChanged();
    QVariantMap result={{QStringLiteral("ok"),true},{QStringLiteral("data"),data},
        {QStringLiteral("count"),bytes.size()},{QStringLiteral("parityErrorIndex"),parity},
        {QStringLiteral("overflow"),overflow},{QStringLiteral("queued"),true}};
    if(simulation())result.insert(QStringLiteral("simulated"),true);
    return result;
}
void ProtocolUartPort::ingest(const QVariantMap& result) {
    if(!m_autoReceiving || !result.value(QStringLiteral("ok")).toBool())return;
    const QVariantList values=result.value(QStringLiteral("data")).toList();
    const bool sdkOverflow=result.value(QStringLiteral("overflow")).toBool();
    const int parityIndex=result.value(QStringLiteral("parityErrorIndex")).toInt();
    if(sdkOverflow){++m_sdkOverflows;m_sdkOverflowPending=true;}
    if(parityIndex>0){++m_parityErrors;m_parityErrorPending=parityIndex;}
    QByteArray bytes;bytes.reserve(values.size());
    for(const QVariant& v:values)bytes.append(static_cast<char>(v.toInt()&0xff));
    if(bytes.isEmpty() && !sdkOverflow && parityIndex==0)return;
    m_receivedBytes+=bytes.size();
    m_rxQueue+=bytes;
    if(m_rxQueue.size()>m_capacity){
        const qsizetype extra=m_rxQueue.size()-m_capacity;
        m_rxQueue.remove(0,extra);
        m_queueDroppedBytes+=extra;
    }
    QVariantMap chunk={{QStringLiteral("data"),values},{QStringLiteral("text"),QString::fromUtf8(bytes)},
        {QStringLiteral("count"),bytes.size()},{QStringLiteral("overflow"),sdkOverflow},
        {QStringLiteral("parityErrorIndex"),parityIndex},
        {QStringLiteral("queueDroppedBytes"),m_queueDroppedBytes},
        {QStringLiteral("timestamp"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("timestampSource"),QStringLiteral("HostPoll")}};
    if(simulation())chunk.insert(QStringLiteral("simulated"),true);
    emit queueChanged();
    emit dataReceived(chunk);
}
void ProtocolUartPort::pollReceive() {
    if(!m_autoReceiving)return;
    // Poll once per timer tick. A read occurs on this same worker thread as
    // other SDK actions and the QJSEngine, avoiding concurrent handle access.
    const QVariantMap result=execute(QStringLiteral("read"),{{QStringLiteral("maxBytes"),4096}});
    if(!result.value(QStringLiteral("ok")).toBool()){
        // execute() may already have invalidated us on an SDK failure.
        stopAutoRx();emit receiveError(result.value(QStringLiteral("error")).toString());return;
    }
    ingest(result);
}

ProtocolSpiPort::ProtocolSpiPort(DwfRuntime* r,QObject* p):ProtocolPortBase(QStringLiteral("SPI"),r,p){}
bool ProtocolSpiPort::configure(const QVariantMap& settings) {
    if(!settings.contains(QStringLiteral("clock")) || !settings.contains(QStringLiteral("mosi")) ||
       !settings.contains(QStringLiteral("miso")) || !settings.contains(QStringLiteral("select"))) {
        setError(QStringLiteral("SPI configure requires clock, mosi, miso, select pins"));return false;
    }
    QVariantMap s=settings;
    s.insert(QStringLiteral("frequencyHz"),settings.value(QStringLiteral("frequencyHz"),1000000));
    const bool ok=configurePort(s);
    if(ok){m_select=settings.value(QStringLiteral("select")).toInt();m_activeLow=settings.value(QStringLiteral("selectActiveLow"),true).toBool();}
    return ok;
}
QVariantMap ProtocolSpiPort::transfer(const QVariantList& tx,int count,int bits,int lanes) {
    if(count<0)count=tx.size();
    return execute(QStringLiteral("transfer"),{{QStringLiteral("tx"),tx},{QStringLiteral("readCount"),count},
        {QStringLiteral("bits"),bits},{QStringLiteral("lanes"),lanes}});
}
QVariantMap ProtocolSpiPort::command(int commandBits,int commandByte,int dummyBits,const QVariantList& tx,int count,int bits,int lanes) {
    return execute(QStringLiteral("command"),{{QStringLiteral("commandBits"),commandBits},
        {QStringLiteral("command"),commandByte},{QStringLiteral("dummyBits"),dummyBits},
        {QStringLiteral("tx"),tx},{QStringLiteral("readCount"),count},{QStringLiteral("bits"),bits},
        {QStringLiteral("lanes"),lanes}});
}
bool ProtocolSpiPort::Start(){return execute(QStringLiteral("select"),{{QStringLiteral("pin"),m_select},{QStringLiteral("level"),m_activeLow?0:1}}).value(QStringLiteral("ok")).toBool();}
bool ProtocolSpiPort::Stop(){return execute(QStringLiteral("select"),{{QStringLiteral("pin"),m_select},{QStringLiteral("level"),m_activeLow?1:0}}).value(QStringLiteral("ok")).toBool();}
QVariantList ProtocolSpiPort::ReadWrite(int bits,const QVariantList& tx){return transfer(tx,tx.size(),bits,1).value(QStringLiteral("rx")).toList();}
bool ProtocolSpiPort::Write(int bits,const QVariantList& tx){return transfer(tx,0,bits,1).value(QStringLiteral("ok")).toBool();}
QVariantList ProtocolSpiPort::Read(int bits,int count){return transfer({},count,bits,1).value(QStringLiteral("rx")).toList();}
QVariantList ProtocolSpiPort::Read0(int bits,int count){return transfer({},count,bits,0).value(QStringLiteral("rx")).toList();}
bool ProtocolSpiPort::WriteDual(int bits,const QVariantList& tx){return transfer(tx,0,bits,2).value(QStringLiteral("ok")).toBool();}
QVariantList ProtocolSpiPort::ReadDual(int bits,int count){return transfer({},count,bits,2).value(QStringLiteral("rx")).toList();}
bool ProtocolSpiPort::WriteQuad(int bits,const QVariantList& tx){return transfer(tx,0,bits,4).value(QStringLiteral("ok")).toBool();}
QVariantList ProtocolSpiPort::ReadQuad(int bits,int count){return transfer({},count,bits,4).value(QStringLiteral("rx")).toList();}

ProtocolI2cPort::ProtocolI2cPort(DwfRuntime* r,QObject* p):ProtocolPortBase(QStringLiteral("I2C"),r,p){}
bool ProtocolI2cPort::configure(const QVariantMap& settings) {
    if(!settings.contains(QStringLiteral("scl")) || !settings.contains(QStringLiteral("sda"))) {
        setError(QStringLiteral("I2C configure requires scl and sda DIO pins"));return false;
    }
    QVariantMap s=settings;
    s.insert(QStringLiteral("rateHz"),settings.value(QStringLiteral("rateHz"),100000));
    s.insert(QStringLiteral("timeoutSeconds"),settings.value(QStringLiteral("timeoutSeconds"),1.0));
    return configurePort(s);
}
QVariantMap ProtocolI2cPort::writeRead(int address,const QVariantList& tx,int count) {
    return execute(QStringLiteral("writeRead"),{{QStringLiteral("address"),address},{QStringLiteral("tx"),tx},{QStringLiteral("count"),count}});
}
QVariantMap ProtocolI2cPort::write(int address,const QVariantList& data){return writeRead(address,data,0);}
QVariantMap ProtocolI2cPort::read(int address,int count){return writeRead(address,{},count);}
bool ProtocolI2cPort::Clear(){return execute(QStringLiteral("clear")).value(QStringLiteral("busFree")).toBool();}
bool ProtocolI2cPort::Receiver(){return execute(QStringLiteral("spyStart")).value(QStringLiteral("ok")).toBool();}
QVariantMap ProtocolI2cPort::spyPoll(int count){return execute(QStringLiteral("spyPoll"),{{QStringLiteral("maxBytes"),count}});}
QVariantList ProtocolI2cPort::Receive() {
    // Simplified WaveForms-style markers: -1 START, -2 RESTART, -3 STOP.
    // The SDK returns raw bytes, not complete per-byte 9-bit ACK words.
    // Prefer spyPoll() for structured bytes + NACK index; not byte-exact
    // WaveForms Protocol.I2C.Receive() parity.
    const QVariantMap result=spyPoll();if(!result.value(QStringLiteral("ok")).toBool())return {};
    QVariantList list;
    const int start=result.value(QStringLiteral("start")).toInt();
    if(start)list.append(start==2?-2:-1);
    const int nack=result.value(QStringLiteral("nackIndex")).toInt();
    const QVariantList data=result.value(QStringLiteral("data")).toList();
    for(const auto& byte:data)list.append(byte.toInt());
    if(result.value(QStringLiteral("stop")).toBool())list.append(-3);
    if(nack<0)list.append(nack);
    return list;
}
int ProtocolI2cPort::Write(int addr,const QVariantList& bytes) {
    const auto result=write(addr,bytes);
    if(!result.value(QStringLiteral("ok")).toBool())return -2;
    return result.value(QStringLiteral("nackIndex")).toInt();
}
QVariantList ProtocolI2cPort::Read(int addr,int count){return read(addr,count).value(QStringLiteral("data")).toList();}
QVariantList ProtocolI2cPort::ReadSub(int addr,const QVariantList& sub,int count){return writeRead(addr,sub,count).value(QStringLiteral("data")).toList();}

ProtocolCanPort::ProtocolCanPort(DwfRuntime* r,QObject* p):ProtocolPortBase(QStringLiteral("CAN"),r,p){}
bool ProtocolCanPort::configure(const QVariantMap& settings) {
    if(!settings.contains(QStringLiteral("tx")) || !settings.contains(QStringLiteral("rx"))) {
        setError(QStringLiteral("CAN configure requires tx and rx DIO pins"));return false;
    }
    QVariantMap s=settings;s.insert(QStringLiteral("rateHz"),settings.value(QStringLiteral("rateHz"),500000));
    return configurePort(s);
}
bool ProtocolCanPort::startRx(){return execute(QStringLiteral("startRx")).value(QStringLiteral("ok")).toBool();}
QVariantMap ProtocolCanPort::send(int id,bool ext,bool remote,const QVariantList& data) {
    return execute(QStringLiteral("send"),{{QStringLiteral("id"),id},{QStringLiteral("extended"),ext},
        {QStringLiteral("remote"),remote},{QStringLiteral("data"),data}});
}
QVariantMap ProtocolCanPort::receive(){return execute(QStringLiteral("receive"));}
bool ProtocolCanPort::Send(int baseId,int ide,int extensionId,int remote,int dlc,const QVariantList& data) {
    if(baseId<0 || baseId>0x7ff || extensionId<0 || extensionId>0x3ffff || (ide!=0 && ide!=1) ||
       (remote!=0 && remote!=1) || dlc<0 || dlc>8 || (remote==0 && dlc!=data.size())) {
        setError(QStringLiteral("CAN.Send needs BI=11bit, IDE=0/1, IE=18bit, RTR=0/1, DLC=0..8"));return false;
    }
    if(remote && !data.isEmpty()) {setError(QStringLiteral("CAN remote frame must not carry data"));return false;}
    // The SDK's classical CAN remote interface exposes only DLC via payload
    // count, so remote DLC without data is supported only as DLC=0 here.
    if(remote && dlc) {setError(QStringLiteral("CAN remote DLC>0 is not supported by this wrapper"));return false;}
    return send(ide?((baseId<<18)|extensionId):baseId,ide!=0,remote!=0,data).value(QStringLiteral("ok")).toBool();
}
QVariantList ProtocolCanPort::Receive() {
    const QVariantMap r=receive();
    if(!r.value(QStringLiteral("ok")).toBool() || !r.value(QStringLiteral("received")).toBool())return {};
    const int id=r.value(QStringLiteral("id")).toInt();const bool extended=r.value(QStringLiteral("extended")).toBool();
    QVariantList output={extended?(id>>18):id,0,extended?1:0,extended?(id&0x3ffff):0,
        r.value(QStringLiteral("remote")).toBool()?1:0,0,0,0,0,r.value(QStringLiteral("dlc")).toInt(),
        QVariant(),QVariant(),QVariant()};
    for(const auto& b:r.value(QStringLiteral("data")).toList())output.append(b);
    // Unlike WaveForms' CAN decoder, SDK does not supply CRC and ACK values.
    return output;
}

ProtocolSwdPort::ProtocolSwdPort(DwfRuntime* r,QObject* p):ProtocolPortBase(QStringLiteral("SWD"),r,p){}
bool ProtocolSwdPort::configure(const QVariantMap& settings) {
    if(!settings.contains(QStringLiteral("clock")) || !settings.contains(QStringLiteral("data"))) {
        setError(QStringLiteral("SWD configure requires clock and data DIO pins"));return false;
    }
    QVariantMap s=settings;s.insert(QStringLiteral("rateHz"),settings.value(QStringLiteral("rateHz"),1000000));
    return configurePort(s);
}
bool ProtocolSwdPort::clear(int reset,int trail){return execute(QStringLiteral("clear"),{{QStringLiteral("resetClocks"),reset},{QStringLiteral("trailClocks"),trail}}).value(QStringLiteral("ok")).toBool();}
QVariantMap ProtocolSwdPort::readRegister(bool ap,int index){return execute(QStringLiteral("read"),{{QStringLiteral("ap"),int(ap)},{QStringLiteral("address"),index}});}
QVariantMap ProtocolSwdPort::writeRegister(bool ap,int index,qulonglong value){return execute(QStringLiteral("write"),{{QStringLiteral("ap"),int(ap)},{QStringLiteral("address"),index},{QStringLiteral("value"),value}});}

ProtocolBridge::ProtocolBridge(DwfRuntime* runtime,QObject* parent):QObject(parent),m_runtime(runtime),
    m_uart(runtime,this),m_spi(runtime,this),m_i2c(runtime,this),m_can(runtime,this),m_swd(runtime,this){}
bool ProtocolBridge::stop(){
    m_uart.stopAutoRx(); // Always suspend polling, including SDK stop failure.
    m_error.clear();
    if(m_runtime && !m_runtime->protocolStop(&m_error))return false;
    for(auto* port : {static_cast<ProtocolPortBase*>(&m_uart),static_cast<ProtocolPortBase*>(&m_spi),
                      static_cast<ProtocolPortBase*>(&m_i2c),static_cast<ProtocolPortBase*>(&m_can),
                      static_cast<ProtocolPortBase*>(&m_swd)})port->invalidate();
    return true;
}
QVariantMap ProtocolBridge::status() const {
    return {{QStringLiteral("simulation"),simulation()},
            {QStringLiteral("activeProtocol"),m_runtime?m_runtime->protocolOwner():QStringLiteral("Simulation")},
            {QStringLiteral("uartConfigured"),m_uart.configured()},
            {QStringLiteral("spiConfigured"),m_spi.configured()},
            {QStringLiteral("i2cConfigured"),m_i2c.configured()},
            {QStringLiteral("canConfigured"),m_can.configured()},
            {QStringLiteral("swdConfigured"),m_swd.configured()}};
}
