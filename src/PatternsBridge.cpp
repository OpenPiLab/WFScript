#include "PatternsBridge.h"
#include "DwfRuntime.h"
#include <cmath>

PatternsBridge::PatternsBridge(DwfRuntime* runtime,QObject* parent)
    :QObject(parent),m_runtime(runtime){}

bool PatternsBridge::commit(bool start) {
    m_error.clear();
    m_encoderMode=false;
    m_sequenceMode=false;
    auto& channel=m_channels.primary();
    const double rawPin=channel.pinValue().value();
    const double hz=channel.frequencyValue().value();
    const double duty=channel.dutyValue().value();
    if(!std::isfinite(rawPin) || rawPin<0 || rawPin>15 || std::floor(rawPin)!=rawPin ||
       !std::isfinite(hz) || hz<1 || hz>1000000 ||
       !std::isfinite(duty) || duty<1 || duty>99 ||
       channel.typeText().text().compare(QStringLiteral("Clock"),Qt::CaseInsensitive)!=0 ||
       channel.outputText().text().compare(QStringLiteral("PP"),Qt::CaseInsensitive)!=0 ||
       channel.idleText().text().compare(QStringLiteral("Z"),Qt::CaseInsensitive)!=0) {
        m_error=QStringLiteral("Patterns: use one Clock channel, Pin 0..15, frequency 1..1,000,000 Hz, duty 1..99%, Output PP, Idle Z");
        if(m_running && !stop())m_error+=QStringLiteral("; PATTERN STOP NOT CONFIRMED");
        return false;
    }
    if(m_runtime) {
        double actualHz=0,actualDuty=0,clock=0;
        m_touched=true; // hardware might become configured even if the SDK call fails
        if(!m_runtime->patternClockConfigure(int(rawPin),hz,duty,start,&actualHz,&actualDuty,&clock,&m_error)) {
            const QString cause=m_error;
            QString stopError;
            if(m_runtime->patternStop(&stopError))m_running=false;
            else m_error=cause+QStringLiteral("; EMERGENCY STOP NOT CONFIRMED: ")+stopError;
            if(m_error.isEmpty())m_error=cause;
            return false;
        }
        m_actualHz=actualHz;
        m_actualDuty=actualDuty;
        m_clockHz=clock;
    } else {
        // An ideal *simulation configuration*, never a physical measurement.
        m_actualHz=hz;m_actualDuty=duty;m_clockHz=125000000;
    }
    channel.frequencyValue().setReal(m_actualHz);
    channel.dutyValue().setReal(m_actualDuty);
    m_touched=true;
    m_running=start;
    return true;
}
bool PatternsBridge::commitEncoder(int aPin,int bPin,int indexPin,int ppr,double rpm,bool reverse,bool start) {
    m_error.clear();
    if(aPin<0 || bPin<0 || indexPin<0 || aPin>15 || bPin>15 || indexPin>15 ||
       aPin==bPin || aPin==indexPin || bPin==indexPin || ppr<1 || ppr>1024 ||
       !std::isfinite(rpm) || rpm<1 || rpm>10000) {
        m_error=QStringLiteral("Encoder needs three distinct DIO pins, PPR 1..1024, RPM 1..10000");
        if(m_running && !stop())m_error+=QStringLiteral("; PATTERN STOP NOT CONFIRMED");
        return false;
    }
    double actualRpm=rpm,clock=125000000;
    if(m_runtime) {
        m_touched=true;
        if(!m_runtime->patternEncoderConfigure(aPin,bPin,indexPin,ppr,rpm,reverse,start,
                                               &actualRpm,&clock,&m_error)) {
            const QString cause=m_error; QString stopError;
            if(m_runtime->patternStop(&stopError))m_running=false;
            else m_error=cause+QStringLiteral("; EMERGENCY STOP NOT CONFIRMED: ")+stopError;
            if(m_error.isEmpty())m_error=cause;
            return false;
        }
    }
    // Only update exposed channel properties AFTER successful SDK config.
    auto& a=m_channels.primary();auto& b=m_channels.second();auto& z=m_channels.third();
    PatternChannel* channels[3]={&a,&b,&z};
    const int pins[3]={aPin,bPin,indexPin};
    for(int i=0;i<3;++i) {
        channels[i]->pinValue().setValue(pins[i]);
        channels[i]->typeText().setText(QStringLiteral("Custom"));
        channels[i]->outputText().setText(QStringLiteral("PP"));
        channels[i]->idleText().setText(QStringLiteral("Z"));
        channels[i]->frequencyValue().setValue(i==2?rpm/60.0:rpm*ppr/60.0);
        channels[i]->frequencyValue().setReal(i==2?actualRpm/60.0:actualRpm*ppr/60.0);
        channels[i]->dutyValue().setValue(i==2?25.0/double(ppr):50.0);
        channels[i]->dutyValue().setReal(i==2?25.0/double(ppr):50.0);
    }
    m_aPin=aPin;m_bPin=bPin;m_zPin=indexPin;m_ppr=ppr;
    m_rpm=rpm;m_actualRpm=actualRpm;m_reverse=reverse;
    m_actualHz=actualRpm*ppr/60.0;
    m_actualDuty=50;
    m_clockHz=clock;
    m_touched=true;m_running=start;m_encoderMode=true;m_sequenceMode=false;
    return true;
}
bool PatternsBridge::runEncoder(int aPin,int bPin,int zPin,int ppr,double rpm,bool reverse) {
    return commitEncoder(aPin,bPin,zPin,ppr,rpm,reverse,true);
}
bool PatternsBridge::configEncoder(int aPin,int bPin,int zPin,int ppr,double rpm,bool reverse) {
    return commitEncoder(aPin,bPin,zPin,ppr,rpm,reverse,false);
}

// General-purpose multi-channel custom output. This deliberately uses a
// separate API from WaveForms' channel property object model: the minimal
// first implementation needs one coherent start, identical bit-lengths and
// a common hardware bit clock. The original clock and encoder APIs remain.
bool PatternsBridge::commitSequence(const QVariantList& pinValues,
                                    const QVariantList& bitValues,double rate,bool start) {
    m_error.clear();
    // A rejected live update must not leave an older hardware pattern driving
    // outputs behind the operator's back. Retain the original error message.
    const auto reject=[&](const QString& reason) {
        if(m_running) {
            const bool released=stop();
            const QString stopError=m_error;
            m_error=reason;
            if(!released)m_error+=QStringLiteral("; OUTPUT STOP NOT CONFIRMED: ")+stopError;
        } else m_error=reason;
        return false;
    };
    if(pinValues.size()<1 || pinValues.size()>3 ||
       pinValues.size()!=bitValues.size() ||
       !std::isfinite(rate) || rate<10 || rate>1000000) {
        return reject(QStringLiteral("runSequence: 1..3 channels and bit rate 10..1,000,000 Hz required"));
    }
    QVector<int> pins; QStringList patterns; quint32 used=0;
    int length=0;
    for(qsizetype i=0;i<pinValues.size();++i) {
        bool ok=false;
        const double pinNumber=pinValues.at(i).toDouble(&ok);
        if(!ok || !std::isfinite(pinNumber) || pinNumber<0 || pinNumber>15 ||
           std::floor(pinNumber)!=pinNumber || (used&(quint32(1)<<int(pinNumber)))) {
            return reject(QStringLiteral("runSequence: distinct integer DIO pins 0..15 required"));
        }
        used|=(quint32(1)<<int(pinNumber));
        pins.append(int(pinNumber));
        const QString bitString=bitValues.at(i).toString();
        if(i==0)length=bitString.size();
        if(length<2 || length>1024 || bitString.size()!=length) {
            return reject(QStringLiteral("runSequence: every string must contain the same 2..1024 bits"));
        }
        for(QChar b:bitString) if(b!=QLatin1Char('0') && b!=QLatin1Char('1')) {
            return reject(QStringLiteral("runSequence: bit data must contain only '0' and '1'"));
        }
        patterns.append(bitString);
    }
    double actual=rate,clock=125000000;int repeatFactor=1;
    if(m_runtime) {
        // If the SDK has touched a pin, any subsequent failure must attempt
        // a safe release of ALL of this new sequence's pins.
        m_touched=true;
        if(!m_runtime->patternSequenceConfigure(pins,patterns,rate,start,
                                 &actual,&clock,&repeatFactor,&m_error)) {
            const QString cause=m_error; QString stopError;
            if(m_runtime->patternStop(&stopError))m_running=false;
            else m_error=cause+QStringLiteral("; EMERGENCY STOP NOT CONFIRMED: ")+stopError;
            if(m_error.isEmpty())m_error=cause;
            return false;
        }
    }
    const auto pinMap=[&](){QVariantList out;for(int p:pins)out.append(p);return out;};
    const auto dataMap=[&](){QVariantList out;for(const QString& p:patterns)out.append(p);return out;};
    m_sequencePins=pinMap();m_sequenceBits=dataMap();
    m_sequenceLength=length;m_sequenceRequestedBitRateHz=rate;
    m_sequenceActualBitRateHz=actual;m_sequenceRepeatFactor=repeatFactor;
    m_actualHz=actual;m_actualDuty=0;m_clockHz=clock;
    for(qsizetype i=0;i<pins.size();++i) {
        PatternChannel* ch= i==0?&m_channels.primary():i==1?&m_channels.second():&m_channels.third();
        ch->pinValue().setValue(pins.at(i));
        ch->typeText().setText(QStringLiteral("Custom"));
        ch->outputText().setText(QStringLiteral("PP"));
        ch->idleText().setText(QStringLiteral("Z"));
    }
    m_touched=true;m_running=start;m_encoderMode=false;m_sequenceMode=true;
    return true;
}
bool PatternsBridge::runSequence(const QVariantList& pins,const QVariantList& bitStrings,double bitRateHz) {
    return commitSequence(pins,bitStrings,bitRateHz,true);
}
bool PatternsBridge::configSequence(const QVariantList& pins,const QVariantList& bitStrings,double bitRateHz) {
    return commitSequence(pins,bitStrings,bitRateHz,false);
}

bool PatternsBridge::config(){return commit(false);}
bool PatternsBridge::run(){return commit(true);}
bool PatternsBridge::stop() {
    m_error.clear();
    if(!m_touched){m_running=false;return true;}
    if(m_runtime && !m_runtime->patternStop(&m_error))return false;
    m_running=false;
    return true;
}
QVariantMap PatternsBridge::status() const {
    const auto& ch=m_channels.primary();
    QVariantMap result={{QStringLiteral("simulation"),!m_runtime},
            {QStringLiteral("running"),m_running},
            {QStringLiteral("pin"),int(ch.pinValue().value())},
            {QStringLiteral("requestedFrequencyHz"),ch.frequencyValue().value()},
            {QStringLiteral("actualFrequencyHz"),m_actualHz},
            {QStringLiteral("requestedDutyPercent"),ch.dutyValue().value()},
            {QStringLiteral("actualDutyPercent"),m_actualDuty},
            {QStringLiteral("clockHz"),m_clockHz},
            {QStringLiteral("mode"),m_sequenceMode?QStringLiteral("Sequence"):(m_encoderMode?QStringLiteral("Encoder"):QStringLiteral("Clock"))},
            {QStringLiteral("sequencePins"),m_sequencePins},
            {QStringLiteral("sequenceBitStrings"),m_sequenceBits},
            {QStringLiteral("sequenceLength"),m_sequenceLength},
            {QStringLiteral("sequenceRequestedBitRateHz"),m_sequenceRequestedBitRateHz},
            {QStringLiteral("sequenceActualBitRateHz"),m_sequenceActualBitRateHz},
            {QStringLiteral("sequenceRepeatFactor"),m_sequenceRepeatFactor},
            {QStringLiteral("encoderAPin"),m_aPin},
            {QStringLiteral("encoderBPin"),m_bPin},
            {QStringLiteral("encoderIndexPin"),m_zPin},
            {QStringLiteral("encoderPpr"),m_ppr},
            {QStringLiteral("encoderReverse"),m_reverse},
            {QStringLiteral("encoderRequestedRpm"),m_rpm},
            {QStringLiteral("encoderActualRpm"),m_actualRpm},
            {QStringLiteral("lastError"),m_error}};
    // sdkStateName/sdkRunning are *actual* device readbacks; running stays
    // the original cached workflow flag for backward compatibility.
    if(m_runtime) {
        const QVariantMap native=m_runtime->patternHardwareStatus();
        for(auto it=native.constBegin();it!=native.constEnd();++it)result.insert(it.key(),it.value());
    } else {
        result.insert(QStringLiteral("sdkStatusAvailable"),false);
        result.insert(QStringLiteral("sdkStateName"),QStringLiteral("Simulation"));
        result.insert(QStringLiteral("sdkStateCode"),-1);
        result.insert(QStringLiteral("sdkRunning"),QVariant());
        result.insert(QStringLiteral("sdkRepeatAvailable"),false);
        result.insert(QStringLiteral("sdkRepeatCount"),QVariant());
    }
    return result;
}
