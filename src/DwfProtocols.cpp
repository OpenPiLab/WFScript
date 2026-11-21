// WFScript v0.2-dev: Digilent digital protocol backend. No Digilent headers or
// binary libraries are redistributed. Every ABI signature below comes from the
// WaveForms SDK Reference Manual (2026-03-04), Chapter 13, checked against the
// corresponding SDK Python ctypes examples.
#include "DwfRuntime.h"
#include <QSet>
#include <cmath>
#include <limits>

namespace {
using H = int;
using F0 = int (*)(H);
using FI = int (*)(H,int);
using FII = int (*)(H,int,int);
using FD = int (*)(H,double);
using FByte = int (*)(H,unsigned char);
using FIP = int (*)(H,int*);
using FUARTTx = int (*)(H,char*,int);
using FUARTRx = int (*)(H,char*,int,int*,int*);
using FSPIPin = int (*)(H,int,int);
using FSPIRw8 = int (*)(H,int,int,unsigned char*,int,unsigned char*,int);
using FSPIRw16 = int (*)(H,int,int,unsigned short*,int,unsigned short*,int);
using FSPIRw32 = int (*)(H,int,int,unsigned int*,int,unsigned int*,int);
using FSPICmd8 = int (*)(H,int,unsigned int,int,int,int,unsigned char*,int,unsigned char*,int);
using FSPICmd16 = int (*)(H,int,unsigned int,int,int,int,unsigned short*,int,unsigned short*,int);
using FSPICmd32 = int (*)(H,int,unsigned int,int,int,int,unsigned int*,int,unsigned int*,int);
using FSPIDelay = int (*)(H,int,int,int,int);
using FI2CRw = int (*)(H,unsigned char,unsigned char*,int,unsigned char*,int,int*);
using FI2CSpy = int (*)(H,int*,int*,unsigned char*,int*,int*);
using FCANTx = int (*)(H,int,int,int,int,unsigned char*);
using FCANRx = int (*)(H,int*,int*,int*,int*,unsigned char*,int,int*);
using FSWDRead = int (*)(H,int,int,int*,unsigned int*,int*);
using FSWDWrite = int (*)(H,int,int,int*,unsigned int);

bool finiteRate(const QVariantMap& args,const char* key,double lo,double hi,double* out,QString* error) {
    bool ok=false;
    const double v=args.value(QLatin1String(key)).toDouble(&ok);
    if(!ok || !std::isfinite(v) || v<lo || v>hi) {
        if(error)*error=QStringLiteral("%1 must be between %2 and %3").arg(QLatin1String(key)).arg(lo).arg(hi);
        return false;
    }
    *out=v;return true;
}
bool pinCheck(const QVariantMap& a,const QStringList& keys,QString* error) {
    QSet<int> used;
    for(const auto& name:keys) {
        if(!a.contains(name))continue;
        bool ok=false;
        const int pin=a.value(name).toInt(&ok);
        if(!ok || pin<0 || pin>15 || used.contains(pin)) {
            if(error)*error=QStringLiteral("DIO pins must be distinct integers 0..15 (%1)").arg(name);
            return false;
        }
        used.insert(pin);
    }
    return true;
}
bool bytes(const QVariant& value,QVector<unsigned char>* output,QString* error) {
    const QVariantList list=value.toList();
    if(list.size()>65536) {if(error)*error=QStringLiteral("Maximum transaction length: 65536 bytes");return false;}
    output->clear();output->reserve(list.size());
    for(const auto& item:list) {
        bool ok=false;const int b=item.toInt(&ok);
        if(!ok || b<0 || b>255) {if(error)*error=QStringLiteral("Expected byte array (integers 0..255)");return false;}
        output->append(static_cast<unsigned char>(b));
    }
    return true;
}
QVariantList byteList(const QVector<unsigned char>& data) {
    QVariantList result;result.reserve(data.size());
    for(unsigned char b:data)result.append(int(b));
    return result;
}
}

bool DwfRuntime::protocolExecute(const QString& protocol,const QString& action,
                                 const QVariantMap& a,QVariantMap* result,QString* error) {
    if(result)result->clear();
    if(!connected()) {if(error)*error=QStringLiteral("No DWF device connected");return false;}
    const bool config=action==QLatin1String("configure");
    if(config) {
        if(!m_protocolOwner.isEmpty() && m_protocolOwner!=protocol) {
            if(error)*error=QStringLiteral("Protocol %1 owns the digital engines; call Protocol1.stop() first").arg(m_protocolOwner);
            return false;
        }
        if(m_patternTouched || m_logicActive) {
            if(error)*error=QStringLiteral("Patterns or Logic currently owns a digital engine; stop it before Protocol1.configure()");
            return false;
        }
        if(m_protocolOwner.isEmpty() && fDigitalIOOutputEnableGet) {
            unsigned int pins=0;
            if(!fDigitalIOOutputEnableGet(m_handle,&pins))return fail(error,QStringLiteral("Checking StaticIO ownership"));
            if(pins) {if(error)*error=QStringLiteral("StaticIO outputs are enabled; call StaticIO.stop() first");return false;}
        }
    } else if(m_protocolOwner!=protocol) {
        if(error)*error=QStringLiteral("%1 is not configured; call Protocol1.%1.configure(settings)").arg(protocol);
        return false;
    }
    auto invoke=[&](auto proto,const char* symbol,auto... arguments)->bool {
        using F=decltype(proto);
        const auto function=reinterpret_cast<F>(m_lib.resolve(symbol));
        if(!function) {
            if(error)*error=QStringLiteral("WaveForms SDK export unavailable: %1").arg(QLatin1String(symbol));
            return false;
        }
        if(!function(m_handle,arguments...)) return fail(error,QString::fromLatin1(symbol));
        return true;
    };
    // Configuration takes resource ownership BEFORE touching any output.
    // If the SDK fails midway, protocolStop() attempts an output reset.
    if(config)m_protocolOwner=protocol;
    const auto done=[&](bool ok)->bool {
        if(!ok && config) {
            const QString cause=error?*error:QStringLiteral("Protocol configuration failed");
            QString shutdownError;
            const bool safe=protocolStop(&shutdownError);
            if(error)*error=cause+(safe?QString():QStringLiteral("; output shutdown unconfirmed: ")+shutdownError);
        }
        if(ok && result)result->insert(QStringLiteral("ok"),true);
        return ok;
    };
    if(protocol==QLatin1String("UART")) {
        if(config) {
            double rate=0,stopBits=1;
            if(!finiteRate(a,"baudRate",1,10000000,&rate,error) ||
               !finiteRate(a,"stopBits",0.5,2,&stopBits,error) ||
               !pinCheck(a,{QStringLiteral("tx"),QStringLiteral("rx")},error))return done(false);
            const int bits=a.value(QStringLiteral("dataBits"),8).toInt();
            const int parity=a.value(QStringLiteral("parity"),0).toInt();
            const int polarity=a.value(QStringLiteral("polarity"),0).toInt();
            if(bits<5 || bits>8 || parity<0 || parity>4 || polarity<0 || polarity>1) {
                if(error) {
                    *error=QStringLiteral("UART: dataBits 5..8, parity 0..4, polarity 0/1 required");
                }
                return done(false);
            }
            // The 2026 manual prints double, but official Python samples and older dwf.h use int.
            // Verify the installed 2026 dwf.h before physical testing.
            return done(invoke(F0{},"FDwfDigitalUartReset") &&
                invoke(FD{},"FDwfDigitalUartRateSet",rate) &&
                invoke(FI{},"FDwfDigitalUartBitsSet",bits) &&
                invoke(FI{},"FDwfDigitalUartParitySet",parity) &&
                invoke(FI{},"FDwfDigitalUartPolaritySet",polarity) &&
                invoke(FD{},"FDwfDigitalUartStopSet",stopBits) &&
                invoke(FI{},"FDwfDigitalUartTxSet",a.value(QStringLiteral("tx")).toInt()) &&
                invoke(FI{},"FDwfDigitalUartRxSet",a.value(QStringLiteral("rx")).toInt()) &&
                invoke(FUARTTx{},"FDwfDigitalUartTx",static_cast<char*>(nullptr),0));
        }
        if(action==QLatin1String("startRx")) {
            int count=0,parity=0;
            return done(invoke(FUARTRx{},"FDwfDigitalUartRx",static_cast<char*>(nullptr),0,&count,&parity));
        }
        if(action==QLatin1String("write")) {
            QVector<unsigned char> tx;
            if(!bytes(a.value(QStringLiteral("data")),&tx,error))return false;
            // SDK takes char*, but payload is binary and may contain zero bytes.
            return done(invoke(FUARTTx{},"FDwfDigitalUartTx",reinterpret_cast<char*>(tx.data()),tx.size()));
        }
        if(action==QLatin1String("read")) {
            const int max=a.value(QStringLiteral("maxBytes"),4096).toInt();
            if(max<1 || max>65536) {if(error)*error=QStringLiteral("UART read maxBytes must be 1..65536");return false;}
            QVector<unsigned char> rx(max);int count=0,parity=0;
            if(!invoke(FUARTRx{},"FDwfDigitalUartRx",reinterpret_cast<char*>(rx.data()),max,&count,&parity))return false;
            if(count<0 || count>max) {if(error)*error=QStringLiteral("UART SDK returned invalid received length");return false;}
            rx.resize(count);
            if(result) {result->insert(QStringLiteral("data"),byteList(rx));result->insert(QStringLiteral("count"),count);
                result->insert(QStringLiteral("parityErrorIndex"),parity>0?parity:0);
                result->insert(QStringLiteral("overflow"),parity<0);}
            return done(true);
        }
    } else if(protocol==QLatin1String("SPI")) {
        if(config) {
            double rate=0;
            if(!finiteRate(a,"frequencyHz",1,100000000,&rate,error) ||
               !pinCheck(a,{QStringLiteral("clock"),QStringLiteral("mosi"),QStringLiteral("miso"),
                           QStringLiteral("select"),QStringLiteral("dq2"),QStringLiteral("dq3")},error))return done(false);
            const int mode=a.value(QStringLiteral("mode"),0).toInt(),duty=a.value(QStringLiteral("duty"),50).toInt();
            if(mode<0 || mode>3 || !QSet<int>{10,20,25,30,33,40,50,60,66,70,75,80,90}.contains(duty)) {
                if(error) {
                    *error=QStringLiteral("SPI mode must be 0..3 and duty must be an SDK-supported setting");
                }
                return done(false);
            }
            if(!invoke(F0{},"FDwfDigitalSpiReset") || !invoke(FD{},"FDwfDigitalSpiFrequencySet",rate) ||
               !invoke(FI{},"FDwfDigitalSpiClockSet",a.value(QStringLiteral("clock")).toInt()) ||
               !invoke(FSPIPin{},"FDwfDigitalSpiDataSet",0,a.value(QStringLiteral("mosi")).toInt()) ||
               !invoke(FSPIPin{},"FDwfDigitalSpiDataSet",1,a.value(QStringLiteral("miso")).toInt()) ||
               !invoke(FI{},"FDwfDigitalSpiModeSet",mode) ||
               !invoke(FI{},"FDwfDigitalSpiOrderSet",a.value(QStringLiteral("msbFirst"),true).toBool()?1:0) ||
               !invoke(FI{},"FDwfDigitalSpiDutySet",duty) ||
               !invoke(FSPIPin{},"FDwfDigitalSpiSelectSet",a.value(QStringLiteral("select")).toInt(),
                         a.value(QStringLiteral("selectActiveLow"),true).toBool()?1:0))return done(false);
            for(int i=2;i<=3;++i) {
                const QString key=QStringLiteral("dq%1").arg(i);
                if(a.contains(key) && !invoke(FSPIPin{},"FDwfDigitalSpiDataSet",i,a.value(key).toInt()))return done(false);
            }
            const auto delays=a.value(QStringLiteral("delays")).toList();
            if(!delays.isEmpty()) {
                if(delays.size()!=4) {if(error)*error=QStringLiteral("SPI delays requires [start,command,word,stop]");return done(false);}
                for(auto d:delays)if(d.toInt()<0 || d.toInt()>1000) {if(error)*error=QStringLiteral("SPI delays must be 0..1000 cycles");return done(false);}
                if(!invoke(FSPIDelay{},"FDwfDigitalSpiDelaySet",delays[0].toInt(),delays[1].toInt(),delays[2].toInt(),delays[3].toInt()))return done(false);
            }
            return done(true);
        }
        if(action==QLatin1String("select")) {
            const int pin=a.value(QStringLiteral("pin")).toInt();
            const int level=a.value(QStringLiteral("level")).toInt();
            if(pin<0 || pin>15 || level< -1 || level>1) {if(error)*error=QStringLiteral("SPI selection requires pin 0..15, level -1/0/1");return false;}
            return done(invoke(FSPIPin{},"FDwfDigitalSpiSelect",pin,level));
        }
        if(action==QLatin1String("transfer") || action==QLatin1String("command")) {
            const int bits=a.value(QStringLiteral("bits"),8).toInt(),lanes=a.value(QStringLiteral("lanes"),1).toInt();
            const int count=a.value(QStringLiteral("readCount"),0).toInt();
            if(bits<1 || bits>32 || (lanes!=0 && lanes!=1 && lanes!=2 && lanes!=4) || count<0 || count>65536) {
                if(error) {
                    *error=QStringLiteral("SPI bits 1..32, lanes 0/1/2/4 and readCount 0..65536 required");
                }
                return false;
            }
            const QVariantList tx=a.value(QStringLiteral("tx")).toList();
            if(tx.size()>65536) {if(error)*error=QStringLiteral("SPI TX exceeds 65536 words");return false;}
            const quint64 limit=bits==32 ? 0xffffffffULL : ((1ULL<<unsigned(bits))-1ULL);
            for(const auto& word:tx) {
                bool ok=false;const qulonglong v=word.toULongLong(&ok);
                if(!ok || v>limit) {if(error)*error=QStringLiteral("SPI TX word exceeds configured %1-bit width").arg(bits);return false;}
            }
            const bool cmd=action==QLatin1String("command");
            const int commandBits=a.value(QStringLiteral("commandBits"),8).toInt();
            const int command=a.value(QStringLiteral("command"),0).toInt();
            const int dummy=a.value(QStringLiteral("dummyBits"),0).toInt();
            // Per the supplied SDK manual, the command argument is an unsigned
            // char. Do not silently truncate commands wider than eight bits.
            if(cmd && (commandBits<1 || commandBits>8 || command<0 || command >= (1<<commandBits) || dummy<0 || dummy>1024)) {
                if(error) {
                    *error=QStringLiteral("SPI command supports 1..8 command bits and 0..1024 dummy bits");
                }
                return false;
            }
            QVariantList output;
            if(bits<=8) {
                QVector<unsigned char> t,r(count);
                if(!bytes(a.value(QStringLiteral("tx")),&t,error))return false;
                const bool ok=cmd ? invoke(FSPICmd8{},"FDwfDigitalSpiCmdWriteRead",commandBits,static_cast<unsigned int>(command),dummy,lanes,bits,t.data(),t.size(),r.data(),r.size()) :
                    invoke(FSPIRw8{},"FDwfDigitalSpiWriteRead",lanes,bits,t.data(),t.size(),r.data(),r.size());
                if(!ok) {
                    return false;
                }
                output=byteList(r);
            } else if(bits<=16) {
                QVector<unsigned short> t,r(count);for(const auto& w:tx)t.append(static_cast<unsigned short>(w.toUInt()));
                const bool ok=cmd ? invoke(FSPICmd16{},"FDwfDigitalSpiCmdWriteRead16",commandBits,static_cast<unsigned int>(command),dummy,lanes,bits,t.data(),t.size(),r.data(),r.size()) :
                    invoke(FSPIRw16{},"FDwfDigitalSpiWriteRead16",lanes,bits,t.data(),t.size(),r.data(),r.size());
                if(!ok) {
                    return false;
                }
                for(auto w:r)output.append(uint(w));
            } else {
                QVector<unsigned int> t,r(count);for(const auto& w:tx)t.append(w.toUInt());
                const bool ok=cmd ? invoke(FSPICmd32{},"FDwfDigitalSpiCmdWriteRead32",commandBits,static_cast<unsigned int>(command),dummy,lanes,bits,t.data(),t.size(),r.data(),r.size()) :
                    invoke(FSPIRw32{},"FDwfDigitalSpiWriteRead32",lanes,bits,t.data(),t.size(),r.data(),r.size());
                if(!ok) {
                    return false;
                }
                for(auto w:r)output.append(qulonglong(w));
            }
            if(result){result->insert(QStringLiteral("rx"),output);result->insert(QStringLiteral("words"),output.size());}
            return done(true);
        }
    } else if(protocol==QLatin1String("I2C")) {
        if(config) {
            double rate=0,timeout=0;
            if(!finiteRate(a,"rateHz",1,10000000,&rate,error) ||
               !finiteRate(a,"timeoutSeconds",0.001,60,&timeout,error) ||
               !pinCheck(a,{QStringLiteral("scl"),QStringLiteral("sda")},error))return done(false);
            return done(invoke(F0{},"FDwfDigitalI2cReset") &&
                invoke(FD{},"FDwfDigitalI2cRateSet",rate) &&
                invoke(FD{},"FDwfDigitalI2cTimeoutSet",timeout) &&
                invoke(FI{},"FDwfDigitalI2cStretchSet",a.value(QStringLiteral("clockStretching"),true).toBool()?1:0) &&
                invoke(FI{},"FDwfDigitalI2cReadNakSet",1) &&
                invoke(FI{},"FDwfDigitalI2cSclSet",a.value(QStringLiteral("scl")).toInt()) &&
                invoke(FI{},"FDwfDigitalI2cSdaSet",a.value(QStringLiteral("sda")).toInt()));
        }
        if(action==QLatin1String("clear")) {
            int free=0;if(!invoke(FIP{},"FDwfDigitalI2cClear",&free))return false;
            if(result)result->insert(QStringLiteral("busFree"),free!=0);
            return done(true);
        }
        if(action==QLatin1String("writeRead")) {
            const int address=a.value(QStringLiteral("address")).toInt();
            const int count=a.value(QStringLiteral("count"),0).toInt();
            if(address<0 || address>127 || count<0 || count>65536) {if(error)*error=QStringLiteral("I2C requires a 7-bit address and 0..65536 read bytes");return false;}
            QVector<unsigned char> tx,rx(count);int nak=0;
            if(!bytes(a.value(QStringLiteral("tx")),&tx,error))return false;
            if(!invoke(FI2CRw{},"FDwfDigitalI2cWriteRead",static_cast<unsigned char>(address<<1),tx.data(),tx.size(),rx.data(),rx.size(),&nak))return false;
            if(result) {result->insert(QStringLiteral("data"),byteList(rx));result->insert(QStringLiteral("nackIndex"),nak);result->insert(QStringLiteral("acknowledged"),nak==0);}
            return done(true);
        }
        if(action==QLatin1String("spyStart"))return done(invoke(F0{},"FDwfDigitalI2cSpyStart"));
        if(action==QLatin1String("spyPoll")) {
            int count=a.value(QStringLiteral("maxBytes"),256).toInt();
            if(count<1 || count>65536) {if(error)*error=QStringLiteral("I2C spy maxBytes 1..65536 required");return false;}
            QVector<unsigned char> data(count);int start=0,stop=0,nak=0;
            if(!invoke(FI2CSpy{},"FDwfDigitalI2cSpyStatus",&start,&stop,data.data(),&count,&nak))return false;
            if(count<0 || count>int(data.size())) {if(error)*error=QStringLiteral("I2C spy returned invalid byte count");return false;}
            data.resize(count);
            if(result) {result->insert(QStringLiteral("data"),byteList(data));result->insert(QStringLiteral("start"),start);
                result->insert(QStringLiteral("stop"),stop!=0);result->insert(QStringLiteral("nackIndex"),nak);
                result->insert(QStringLiteral("warning"),nak<0);result->insert(QStringLiteral("timestampSource"),QStringLiteral("HostPoll"));}
            return done(true);
        }
    } else if(protocol==QLatin1String("CAN")) {
        if(config) {
            double rate=0;
            if(!finiteRate(a,"rateHz",1000,1000000,&rate,error) ||
               !pinCheck(a,{QStringLiteral("tx"),QStringLiteral("rx")},error))return done(false);
            return done(invoke(F0{},"FDwfDigitalCanReset") && invoke(FD{},"FDwfDigitalCanRateSet",rate) &&
                // 2026 PDF has a double typo; Python samples and dwf.h use int.
                invoke(FI{},"FDwfDigitalCanPolaritySet",a.value(QStringLiteral("inverted"),false).toBool()?1:0) &&
                invoke(FI{},"FDwfDigitalCanTxSet",a.value(QStringLiteral("tx")).toInt()) &&
                invoke(FI{},"FDwfDigitalCanRxSet",a.value(QStringLiteral("rx")).toInt()) &&
                invoke(FCANTx{},"FDwfDigitalCanTx",-1,0,0,0,static_cast<unsigned char*>(nullptr)));
        }
        if(action==QLatin1String("startRx")) {
            return done(invoke(FCANRx{},"FDwfDigitalCanRx",static_cast<int*>(nullptr),static_cast<int*>(nullptr),
                static_cast<int*>(nullptr),static_cast<int*>(nullptr),static_cast<unsigned char*>(nullptr),0,static_cast<int*>(nullptr)));
        }
        if(action==QLatin1String("send")) {
            const int id=a.value(QStringLiteral("id")).toInt();
            const bool ext=a.value(QStringLiteral("extended"),false).toBool();
            const bool remote=a.value(QStringLiteral("remote"),false).toBool();
            QVector<unsigned char> payload;
            if(!bytes(a.value(QStringLiteral("data")),&payload,error))return false;
            if(id<0 || (ext?id>0x1fffffff:id>0x7ff) || payload.size()>8 || (remote && !payload.isEmpty())) {
                if(error) {
                    *error=QStringLiteral("Classical CAN: ID 11/29-bit, DLC 0..8, remote frames carry no payload");
                }
                return false;
            }
            return done(invoke(FCANTx{},"FDwfDigitalCanTx",id,int(ext),int(remote),int(payload.size()),payload.data()));
        }
        if(action==QLatin1String("receive")) {
            int id=0,ext=0,remote=0,dlc=0,status=0;QVector<unsigned char> payload(8);
            if(!invoke(FCANRx{},"FDwfDigitalCanRx",&id,&ext,&remote,&dlc,payload.data(),8,&status))return false;
            if(dlc<0 || dlc>8) {if(error)*error=QStringLiteral("CAN SDK returned invalid classical DLC");return false;}
            payload.resize(dlc);
            if(result){result->insert(QStringLiteral("received"),status!=0);result->insert(QStringLiteral("id"),id);
                result->insert(QStringLiteral("extended"),ext!=0);result->insert(QStringLiteral("remote"),remote!=0);
                result->insert(QStringLiteral("dlc"),dlc);result->insert(QStringLiteral("status"),status);
                result->insert(QStringLiteral("data"),byteList(payload));result->insert(QStringLiteral("timestampSource"),QStringLiteral("HostPoll"));}
            return done(true);
        }
    } else if(protocol==QLatin1String("SWD")) {
        if(config) {
            double rate=0;
            if(!finiteRate(a,"rateHz",1000,10000000,&rate,error) ||
               !pinCheck(a,{QStringLiteral("clock"),QStringLiteral("data")},error))return done(false);
            const int turnaround=a.value(QStringLiteral("turnaround"),1).toInt();
            if(turnaround<1 || turnaround>4) {if(error)*error=QStringLiteral("SWD turnaround must be 1..4");return done(false);}
            // The Python ctypes example uses a 32-bit int DIO channel.
            // Check this against the installed dwf.h before physical validation.
            return done(invoke(F0{},"FDwfDigitalSwdReset") &&
                invoke(FD{},"FDwfDigitalSwdRateSet",rate) &&
                invoke(FI{},"FDwfDigitalSwdCkSet",a.value(QStringLiteral("clock")).toInt()) &&
                invoke(FI{},"FDwfDigitalSwdIoSet",a.value(QStringLiteral("data")).toInt()) &&
                invoke(FI{},"FDwfDigitalSwdTurnSet",turnaround));
        }
        if(action==QLatin1String("clear")) {
            const int reset=a.value(QStringLiteral("resetClocks"),52).toInt();
            const int trail=a.value(QStringLiteral("trailClocks"),16).toInt();
            if(reset<8 || reset>1024 || trail<0 || trail>1024) {if(error)*error=QStringLiteral("SWD reset/trail clocks out of range");return false;}
            return done(invoke(FII{},"FDwfDigitalSwdClear",reset,trail));
        }
        if(action==QLatin1String("read") || action==QLatin1String("write")) {
            const int ap=a.value(QStringLiteral("ap"),0).toInt();
            const int address=a.value(QStringLiteral("address"),0).toInt();
            if((ap!=0 && ap!=1) || address<0 || address>3) {if(error)*error=QStringLiteral("SWD AP/DP 0/1; address A[3:2] index 0..3");return false;}
            int ack=0,parity=0;unsigned int value=0;
            if(action==QLatin1String("read")) {
                if(!invoke(FSWDRead{},"FDwfDigitalSwdRead",ap,address,&ack,&value,&parity))return false;
                if(result){result->insert(QStringLiteral("value"),qulonglong(value));result->insert(QStringLiteral("parityError"),parity!=0);}
            } else {
                bool valid=false;const qulonglong requested=a.value(QStringLiteral("value")).toULongLong(&valid);
                if(!valid || requested>0xffffffffULL) {if(error)*error=QStringLiteral("SWD write value must fit uint32");return false;}
                if(!invoke(FSWDWrite{},"FDwfDigitalSwdWrite",ap,address,&ack,static_cast<unsigned int>(requested)))return false;
            }
            if(result){result->insert(QStringLiteral("ack"),ack);result->insert(QStringLiteral("ackName"),ack==1?QStringLiteral("OK"):(ack==2?QStringLiteral("WAIT"):(ack==4?QStringLiteral("FAULT"):QStringLiteral("UNKNOWN"))));}
            return done(true);
        }
    }
    if(error)*error=QStringLiteral("Unsupported protocol action: %1.%2").arg(protocol,action);
    return false;
}

bool DwfRuntime::protocolStop(QString* error) {
    if(m_protocolOwner.isEmpty())return true;
    if(!connected()){if(error)*error=QStringLiteral("Device disconnected before protocol output release");return false;}
    // Reset the native output engine (not the whole device and not other
    // analog instruments). It also releases I2C clock/data and UART/CAN TX.
    const auto stopOutput=reinterpret_cast<F0>(m_lib.resolve("FDwfDigitalOutReset"));
    if(!stopOutput || !stopOutput(m_handle)) {
        if(error)*error=stopOutput?QStringLiteral("FDwfDigitalOutReset: ")+lastError():
            QStringLiteral("SDK missing FDwfDigitalOutReset; output release cannot be confirmed");
        return false;
    }
    const auto stopInput=reinterpret_cast<int (*)(H,int,int)>(m_lib.resolve("FDwfDigitalInConfigure"));
    if(stopInput && !stopInput(m_handle,0,0)) {
        if(error)*error=QStringLiteral("FDwfDigitalInConfigure(stop): ")+lastError();
        return false;
    }
    m_protocolOwner.clear();
    return true;
}
