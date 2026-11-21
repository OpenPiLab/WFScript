#include "StaticIOBridge.h"
#include "DwfRuntime.h"
#include <QJSEngine>
#include <QTimer>
#include <QDateTime>
#include <QtGlobal>

StaticIOInput::StaticIOInput(StaticIOBridge* bridge,int index,QObject* parent)
    :QObject(parent),m_bridge(bridge),m_index(index) {}
int StaticIOInput::value() const {return m_bridge->readPin(m_index);}
StaticIOGroupInput::StaticIOGroupInput(StaticIOBridge* bridge,int start,QObject* parent)
    :QObject(parent),m_bridge(bridge),m_start(start) {}
int StaticIOGroupInput::value() const{return m_bridge->readGroup(m_start);}
StaticIOPin::StaticIOPin(StaticIOBridge* bridge,int index,QObject* parent)
    :QObject(parent),m_input(bridge,index,this) {}
QString StaticIOPin::text() const {
    if(m_state==0) return QStringLiteral("0");
    if(m_state==1) return QStringLiteral("1");
    if(m_state==2) return QStringLiteral("Z");
    return QStringLiteral("INVALID");
}
void StaticIOPin::setValue(int state) {
    if(m_state!=state){m_state=state;emit changed();}
}
void StaticIOPin::setText(const QString& state) {
    const QString normalized=state.trimmed().toUpper();
    if(normalized==QStringLiteral("Z"))setValue(2);
    else if(normalized==QStringLiteral("0"))setValue(0);
    else if(normalized==QStringLiteral("1"))setValue(1);
    else setValue(-1); // Rejected by config; never silently drive a pin.
}
StaticIOGroup::StaticIOGroup(StaticIOBridge* bridge,int start,QObject* parent)
    :QObject(parent),m_start(start),m_input(bridge,start,this),m_mode(this) {
    for(int i=0;i<8;++i)m_pins.append(new StaticIOPin(bridge,start+i,this));
}
QObject* StaticIOGroup::pin(int globalIndex) const {
    return globalIndex>=m_start && globalIndex<m_start+8 ? m_pins.at(globalIndex-m_start) : nullptr;
}
StaticIOBridge::StaticIOBridge(DwfRuntime* runtime,QObject* parent)
    :QObject(parent),m_runtime(runtime),m_ch0(this,0,this),m_ch1(this,8,this) {}
StaticIOPin* StaticIOBridge::pin(int index) const {
    if(index<0 || index>15)return nullptr;
    return static_cast<StaticIOPin*>(index<8?m_ch0.pin(index):m_ch1.pin(index));
}
QVariantMap StaticIOBridge::capabilities() {
    QVariantMap result;
    m_error.clear();
    bool available=true;
    if(m_runtime)available=m_runtime->staticIOCapabilities(&m_outputSupported,&m_inputSupported,&m_error);
    result[QStringLiteral("available")]=available;
    result[QStringLiteral("simulation")]=simulation();
    if(!available) {m_outputSupported=0;m_inputSupported=0;}
    result[QStringLiteral("outputMask")]=m_outputSupported;
    result[QStringLiteral("inputMask")]=m_inputSupported;
    result[QStringLiteral("pins")]=16;
    result[QStringLiteral("error")]=m_error;
    return result;
}
bool StaticIOBridge::config() {
    m_error.clear();
    quint32 enables=0,levels=0;
    for(int i=0;i<16;++i) {
        const int state=pin(i)->value();
        if(state<0 || state>2) {
            m_error=QStringLiteral("DIO%1: valid states are 0 (LOW), 1 (HIGH), and 2 (Z)").arg(i);
            if(m_touched) {
                const QString reason=m_error;
                const bool safe=stop();
                m_error=reason+(safe ? QStringLiteral("; outputs released to Z") :
                            QStringLiteral("; EMERGENCY HIGH-Z NOT CONFIRMED: ")+m_error);
            }
            return false;
        }
        if(state!=2) {
            enables|=quint32(1u)<<i;
            if(state==1)levels|=quint32(1u)<<i;
        }
    }
    if(m_runtime) {
        quint32 readable=0,output=0;
        if(!m_runtime->staticIOCapabilities(&output,&readable,&m_error))return false;
        m_outputSupported=output;m_inputSupported=readable;
        if((enables&~output)!=0) {
            m_error=QStringLiteral("Selected DIO output is not supported by connected hardware");
            if(m_touched) {
                const QString reason=m_error;
                const bool safe=stop();
                m_error=reason+(safe ? QStringLiteral("; outputs released to Z") :
                            QStringLiteral("; EMERGENCY HIGH-Z NOT CONFIRMED: ")+m_error);
            }
            return false;
        }
        // A failed SDK write can leave uncertain pin directions. From this
        // point, cleanup must attempt to release the digital output bank.
        m_touched=true;
        m_confirmedOff=false;
        if(!m_runtime->staticIOApply(enables,levels,&m_error)) {
            QString stopError;
            if(m_runtime->staticIOStop(&stopError)) {
                m_confirmedOff=true;
                m_outputEnabled=0;m_outputLevels=0;
                for(int j=0;j<16;++j)pin(j)->setValue(2);
            } else m_error+=QStringLiteral("; EMERGENCY HIGH-Z NOT CONFIRMED: ")+stopError;
            return false;
        }
    } else m_touched=true;
    m_outputEnabled=enables;
    m_outputLevels=levels;
    m_confirmedOff=(enables==0);
    return true;
}
bool StaticIOBridge::setPin(int index,int state) {
    m_error.clear();
    auto* entry=pin(index);
    if(!entry || state<0 || state>2) {
        m_error=QStringLiteral("Invalid StaticIO pin/state; pin must be 0..15, state 0/1/2");
        return false;
    }
    const int previous=entry->value();
    entry->setValue(state);
    if(config())return true;
    // If an emergency high-Z succeeded, keep the software values at Z too.
    if(!m_confirmedOff)entry->setValue(previous);
    return false;
}
QVariantMap StaticIOBridge::status() {
    QVariantMap result;
    m_error.clear();
    bool valid=true;
    if(m_runtime)valid=m_runtime->staticIORead(&m_inputLevels,&m_error);
    else m_inputLevels=(m_outputLevels&m_outputEnabled); // Ideal output echo; floating inputs LOW in sim.
    result[QStringLiteral("valid")]=valid;
    result[QStringLiteral("simulation")]=simulation();
    result[QStringLiteral("inputMask")]=m_inputLevels;
    result[QStringLiteral("outputEnableMask")]=m_outputEnabled;
    result[QStringLiteral("outputValueMask")]=m_outputLevels;
    // Untouched means no output writes were made by this bridge; it does not
    // establish that some earlier or external process has released the bank.
    result[QStringLiteral("touched")]=m_touched;
    result[QStringLiteral("confirmedHighZ")]=m_touched && m_confirmedOff;
    result[QStringLiteral("lastError")]=m_error;
    return result;
}
int StaticIOBridge::readPin(int index) {
    if(index<0 || index>15) {m_error=QStringLiteral("Invalid DIO pin index");return -1;}
    const auto values=status();
    if(!values.value(QStringLiteral("valid")).toBool())return -1;
    return (m_inputLevels>>index)&1u;
}
int StaticIOBridge::readGroup(int start) {
    if(start!=0 && start!=8) {m_error=QStringLiteral("Invalid StaticIO group");return -1;}
    const auto values=status();
    if(!values.value(QStringLiteral("valid")).toBool())return -1;
    return int((m_inputLevels>>start)&0xffu);
}
bool StaticIOBridge::stop() {
    m_error.clear();
    if(m_touched && m_runtime && !m_runtime->staticIOStop(&m_error)) {
        m_confirmedOff=false;
        return false;
    }
    m_confirmedOff=true;
    m_outputEnabled=0;
    m_outputLevels=0;
    for(int i=0;i<16;++i)pin(i)->setValue(2);
    return true;
}


// Every monitor snapshot is observational. No DigitalIO configure/reset/output
// functions are called by startMonitor(), pollMonitor(), or stopMonitor().
// The QTimer and QJSEngine reside on the SAME PanelRuntime worker thread.
bool StaticIOBridge::monitoring() const {
    return m_monitorTimer && m_monitorTimer->isActive();
}

bool StaticIOBridge::readMonitorSnapshot(quint32* mask) {
    if (m_runtime && !m_runtime->protocolOwner().isEmpty()) {
        m_error=QStringLiteral("Digital protocol %1 owns shared digital resources; stop Protocol1 before monitoring")
                    .arg(m_runtime->protocolOwner());
        return false;
    }
    const auto snapshot=status(); // exactly one DigitalIOStatus + InputStatus per poll
    if(!snapshot.value(QStringLiteral("valid")).toBool())return false;
    *mask=quint32(snapshot.value(QStringLiteral("inputMask")).toUInt());
    return true;
}

bool StaticIOBridge::onChange(int pin,const QJSValue& callback,const QVariantMap& options) {
    m_error.clear();
    if(m_monitorShutdown || !m_engine) {
        m_error=QStringLiteral("StaticIO input callbacks require a running Panel script");
        return false;
    }
    if(pin<0 || pin>=16 || !callback.isCallable()) {
        m_error=QStringLiteral("onChange(pin,callback): pin must be 0..15 and callback a function");
        return false;
    }
    const QString edge=options.value(QStringLiteral("edge"),QStringLiteral("change")).toString().toLower();
    const QVariant debounce=options.value(QStringLiteral("debounceMs"),30);
    bool ok=false;
    const int debounceMs=debounce.toInt(&ok);
    if((edge!=QStringLiteral("change") && edge!=QStringLiteral("rising") && edge!=QStringLiteral("falling")) ||
        !ok || debounceMs<0 || debounceMs>1000 || debounce.toDouble()!=double(debounceMs)) {
        m_error=QStringLiteral("onChange: edge must be change/rising/falling; debounceMs integer 0..1000");
        return false;
    }
    const int initial=int((m_lastMonitorMask>>pin)&1u);
    Watch w;
    w.callback=callback; w.edge=edge; w.debounceMs=debounceMs;
    w.raw=initial; w.stable=initial;
    w.rawSinceMs=m_monitorClock.isValid()?m_monitorClock.elapsed():0;
    m_watches.insert(pin,w);
    // Does not start a timer or perform an SDK read.
    return true;
}
void StaticIOBridge::clearOnChange(int pin) {m_watches.remove(pin);}
void StaticIOBridge::clearAllOnChange() {m_watches.clear();}

bool StaticIOBridge::startMonitor(const QVariantMap& options) {
    m_error.clear();
    if(m_monitorShutdown || !m_engine) {
        m_error=QStringLiteral("startMonitor requires a running Panel; standalone scripts have no persistent event loop");
        return false;
    }
    if(monitoring()) {
        m_error=QStringLiteral("StaticIO monitor is already running; call stopMonitor() first");
        return false;
    }
    const QVariant value=options.value(QStringLiteral("pollIntervalMs"),20);
    bool ok=false;
    const int pollMs=value.toInt(&ok);
    if(!ok || value.toDouble()!=double(pollMs) || pollMs<10 || pollMs>1000) {
        m_error=QStringLiteral("pollIntervalMs must be an integer between 10 and 1000 ms");
        return false;
    }
    // Baseline snapshot is required: no synthetic first-edge callbacks.
    quint32 initial=0;
    if(!readMonitorSnapshot(&initial))return false;
    m_lastMonitorMask=initial;
    m_monitorClock.start();
    m_monitorPolls=0; m_monitorChanges=0; m_monitorCallbacks=0;
    m_monitorFailures=0; m_lastPollUsec=0;
    m_pollIntervalMs=pollMs;
    for(auto it=m_watches.begin();it!=m_watches.end();++it) {
        const int level=int((initial>>it.key())&1u);
        it->raw=level;it->stable=level;it->rawSinceMs=0;
    }
    if(!m_monitorTimer) {
        m_monitorTimer=new QTimer(this);
        m_monitorTimer->setTimerType(Qt::CoarseTimer);
        connect(m_monitorTimer,&QTimer::timeout,this,&StaticIOBridge::pollMonitor);
    }
    m_monitorTimer->start(m_pollIntervalMs);
    emit monitoringChanged();
    return true;
}
void StaticIOBridge::stopMonitor() {
    if(!monitoring())return;
    m_monitorTimer->stop();
    emit monitoringChanged();
    // Watch registrations are retained for a deliberate restart; no polls occur.
}
void StaticIOBridge::shutdownMonitor() {
    stopMonitor();
    m_monitorShutdown=true;
    clearAllOnChange(); // release QJSValue callbacks before QJSEngine destruction
    m_engine=nullptr;
}

void StaticIOBridge::pollMonitor() {
    if(!monitoring() || m_monitorShutdown)return;
    QElapsedTimer callTime;callTime.start();
    quint32 input=0;
    if(!readMonitorSnapshot(&input)) {
        ++m_monitorFailures;
        const QString reason=m_error;
        stopMonitor(); // fail closed: don't invent LOW or false input edges
        emit monitorFailed(reason);
        return;
    }
    m_lastPollUsec=callTime.nsecsElapsed()/1000;
    m_lastMonitorMask=input;
    ++m_monitorPolls;
    const qint64 now=m_monitorClock.elapsed();
    const QList<int> keys=m_watches.keys();
    for(const int pin:keys) {
        auto it=m_watches.find(pin);
        if(it==m_watches.end())continue;
        const int sample=int((input>>pin)&1u);
        if(sample!=it->raw) {
            it->raw=sample;
            it->rawSinceMs=now;
        }
        if(sample==it->stable || (now-it->rawSinceMs)<it->debounceMs)continue;
        const int previous=it->stable;
        it->stable=sample;
        ++m_monitorChanges;
        const bool match=it->edge==QStringLiteral("change") ||
                 (it->edge==QStringLiteral("rising") && sample==1) ||
                 (it->edge==QStringLiteral("falling") && sample==0);
        if(!match)continue;
        // The event is host-polled, not device timestamped or hardware-triggered.
        QVariantMap event{{QStringLiteral("pin"),pin},
                          {QStringLiteral("value"),sample},
                          {QStringLiteral("previous"),previous},
                          {QStringLiteral("edge"),sample?QStringLiteral("rising"):QStringLiteral("falling")},
                          {QStringLiteral("stableMs"),int(now-it->rawSinceMs)},
                          {QStringLiteral("inputMask"),input},
                          {QStringLiteral("timestampUtc"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                          {QStringLiteral("timestampSource"),QStringLiteral("HostPollAfterSDK")},
                          {QStringLiteral("simulation"),simulation()}};
        QJSValue callback=it->callback; // retain across callback clear/stop
        emit inputChanged(pin,sample,previous,event);
        if(!callback.isCallable() || !m_engine || m_monitorShutdown)continue;
        ++m_monitorCallbacks;
        emit callbackStarted();
        const QJSValue result=callback.call(QJSValueList{
            QJSValue(sample),QJSValue(pin),m_engine->toScriptValue(event)});
        emit callbackFinished();
        if(!m_monitorShutdown && m_engine && !m_engine->isInterrupted() && result.isError())
            emit callbackFailed(QStringLiteral("StaticIO.onChange(DIO%1)").arg(pin),result);
        if(!monitoring())break; // callback may stopMonitor(); never emit more events
    }
}
QVariantMap StaticIOBridge::monitorStats() const {
    return {{QStringLiteral("monitoring"),monitoring()},
            {QStringLiteral("pollIntervalMs"),m_pollIntervalMs},
            {QStringLiteral("watchCount"),m_watches.size()},
            {QStringLiteral("polls"),qulonglong(m_monitorPolls)},
            {QStringLiteral("changes"),qulonglong(m_monitorChanges)},
            {QStringLiteral("callbacks"),qulonglong(m_monitorCallbacks)},
            {QStringLiteral("readFailures"),qulonglong(m_monitorFailures)},
            {QStringLiteral("lastPollDurationUs"),m_lastPollUsec},
            {QStringLiteral("inputMask"),m_lastMonitorMask},
            {QStringLiteral("timestampSource"),QStringLiteral("HostPollAfterSDK")},
            {QStringLiteral("simulation"),simulation()},
            {QStringLiteral("lastError"),m_error}};
}
