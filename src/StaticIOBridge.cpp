#include "StaticIOBridge.h"
#include "DwfRuntime.h"

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
