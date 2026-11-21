#include "BinaryBridge.h"
#include <QJSEngine>
#include <QJSValue>
#include <QtEndian>
#include <QVariantList>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <climits>
#include <algorithm>
#include <QByteArrayView>

namespace {
constexpr int kMaxBinary=8*1024*1024;
constexpr int kMaxSmallExport=4096;
constexpr int kMaxTextLine=8192;
unsigned char octet(const char* p,int i){return static_cast<unsigned char>(p[i]);}
QObject* jsOwned(QObject* obj){QJSEngine::setObjectOwnership(obj,QJSEngine::JavaScriptOwnership);return obj;}
}
BinaryBuffer::BinaryBuffer(QByteArray storage,int offset,int length)
    :m_storage(std::move(storage)),m_offset(offset),m_length(length<0?m_storage.size()-offset:length) {}
bool BinaryBuffer::available(int pos,int n){
    m_error.clear();
    if(m_released){m_error=QStringLiteral("BinaryBuffer released");return false;}
    if(pos<0 || n<0 || qint64(pos)+n>m_length){m_error=QStringLiteral("BinaryBuffer index outside view");return false;}
    return true;
}
QObject* BinaryBuffer::slice(int offset,int count){
    if(!available(offset,count))return nullptr;
    return jsOwned(new BinaryBuffer(m_storage,m_offset+offset,count));
}
QObject* BinaryBuffer::ieeePayload(){
    m_error.clear();
    const char* p=dataPointer();
    if(m_length<2 || p[0]!='#' || p[1]<'1' || p[1]>'9') {
        m_error=QStringLiteral("Expected definite-length IEEE 488.2 block");return nullptr;
    }
    const int digits=p[1]-'0';
    if(m_length<2+digits){m_error=QStringLiteral("Incomplete IEEE header");return nullptr;}
    qint64 len=0;
    for(int i=0;i<digits;++i){
        const char c=p[2+i];
        if(c<'0'||c>'9'){m_error=QStringLiteral("Non-decimal IEEE block length");return nullptr;}
        len=len*10+(c-'0');
        if(len>kMaxBinary){m_error=QStringLiteral("IEEE payload exceeds 8 MiB");return nullptr;}
    }
    const qint64 end=2+digits+len;
    if(end>m_length){m_error=QStringLiteral("Truncated IEEE payload");return nullptr;}
    if(end!=m_length && !(end+1==m_length && p[end]=='\n') &&
       !(end+2==m_length && p[end]=='\r' && p[end+1]=='\n')){
        m_error=QStringLiteral("Unexpected trailing IEEE data");return nullptr;
    }
    return slice(2+digits,int(len));
}
int BinaryBuffer::readU8(int offset){return available(offset,1)?octet(dataPointer(),offset):-1;}
int BinaryBuffer::readU16LE(int offset){
    if(!available(offset,2))return -1;
    const char* p=dataPointer();return octet(p,offset)|(octet(p,offset+1)<<8);
}
int BinaryBuffer::readU16BE(int offset){
    if(!available(offset,2))return -1;
    const char* p=dataPointer();return (octet(p,offset)<<8)|octet(p,offset+1);
}
double BinaryBuffer::readU32LE(int offset){
    if(!available(offset,4))return -1;
    const char* p=dataPointer();
    const quint32 value=quint32(octet(p,offset))|(quint32(octet(p,offset+1))<<8)|
        (quint32(octet(p,offset+2))<<16)|(quint32(octet(p,offset+3))<<24);
    return double(value);
}
double BinaryBuffer::readF32LE(int offset){
    if(!available(offset,4))return std::numeric_limits<double>::quiet_NaN();
    const char* p=dataPointer();
    const quint32 bits=quint32(octet(p,offset))|(quint32(octet(p,offset+1))<<8)|
        (quint32(octet(p,offset+2))<<16)|(quint32(octet(p,offset+3))<<24);
    float value;std::memcpy(&value,&bits,sizeof(value));return double(value);
}
double BinaryBuffer::sum32(){
    m_error.clear();quint32 sum=0;const char* p=dataPointer();
    for(int i=0;i<m_length;++i)sum+=octet(p,i);
    return double(sum);
}
int BinaryBuffer::crc16(){
    m_error.clear();quint16 crc=0xffff;const char* p=dataPointer();
    for(int i=0;i<m_length;++i){
        crc^=octet(p,i);
        for(int j=0;j<8;++j)crc=(crc&1)?quint16((crc>>1)^0xA001):quint16(crc>>1);
    }
    return crc;
}
QJSValue BinaryBuffer::statsU16LE(int byteOffset,int count){
    if(count<0 && byteOffset>=0 && byteOffset<=m_length)count=(m_length-byteOffset)/2;
    if(count<=0 || count>kMaxBinary/2 || !available(byteOffset,count*2))return QJSValue(QJSValue::NullValue);
    const char* p=dataPointer()+byteOffset;
    quint16 lo=65535,hi=0;double sum=0,sum2=0;
    for(int i=0;i<count;++i){
        const quint16 v=quint16(octet(p,2*i)|(octet(p,2*i+1)<<8));
        lo=qMin(lo,v);hi=qMax(hi,v);sum+=v;sum2+=double(v)*double(v);
    }
    auto* engine=qjsEngine(this);
    if(!engine){m_error=QStringLiteral("No JavaScript engine");return QJSValue(QJSValue::NullValue);}
    QJSValue result=engine->newObject();
    result.setProperty(QStringLiteral("count"),count);
    result.setProperty(QStringLiteral("min"),int(lo));
    result.setProperty(QStringLiteral("max"),int(hi));
    result.setProperty(QStringLiteral("sum"),sum);
    result.setProperty(QStringLiteral("mean"),sum/count);
    result.setProperty(QStringLiteral("rms"),std::sqrt(sum2/count));
    return result;
}
QString BinaryBuffer::toHex(int offset,int count){
    if(count>kMaxSmallExport || !available(offset,count))return {};
    return QString::fromLatin1(QByteArray(dataPointer()+offset,count).toHex());
}
QJSValue BinaryBuffer::toArray(int offset,int count){
    if(count>kMaxSmallExport || !available(offset,count))return QJSValue(QJSValue::NullValue);
    auto* engine=qjsEngine(this);if(!engine){m_error=QStringLiteral("No JavaScript engine");return QJSValue(QJSValue::NullValue);}
    QJSValue arr=engine->newArray(quint32(count));
    for(int i=0;i<count;++i)arr.setProperty(quint32(i),octet(dataPointer(),offset+i));
    return arr;
}
void BinaryBuffer::release(){m_storage.clear();m_offset=0;m_length=0;m_released=true;m_error.clear();}
BinaryStream::BinaryStream(QJSEngine* engine,int limit):m_engine(engine),m_limit(limit){}
void BinaryStream::skipOptionalSuffix(){
    if(!m_optionalSuffix)return;
    if(m_rx.isEmpty())return;
    if(m_rx[0]=='\r'){
        if(m_rx.size()==1)return;
        if(m_rx[1]=='\n')m_rx.remove(0,2);
        m_optionalSuffix=false;return;
    }
    if(m_rx[0]=='\n')m_rx.remove(0,1);
    m_optionalSuffix=false;
}
bool BinaryStream::append(QObject* value){
    auto* chunk=qobject_cast<BinaryBuffer*>(value);
    if(!chunk || chunk->m_released){m_error=QStringLiteral("append requires a live BinaryBuffer");return false;}
    if(qint64(m_rx.size())+chunk->length()>m_limit){m_error=QStringLiteral("Stream maximum exceeded");return false;}
    m_rx.append(chunk->dataPointer(),chunk->length());m_error.clear();skipOptionalSuffix();return true;
}
QJSValue BinaryStream::readLine(){
    skipOptionalSuffix();if(m_optionalSuffix)return QJSValue(QJSValue::NullValue);
    const int nl=m_rx.indexOf('\n');
    if(nl<0){if(m_rx.size()>kMaxTextLine)m_error=QStringLiteral("Line exceeds 8192 bytes");return QJSValue(QJSValue::NullValue);}
    if(nl>kMaxTextLine){m_error=QStringLiteral("Line exceeds 8192 bytes");return QJSValue(QJSValue::NullValue);}
    QByteArray line=m_rx.left(nl);m_rx.remove(0,nl+1);
    if(line.endsWith('\r'))line.chop(1);
    m_error.clear();return QJSValue(QString::fromUtf8(line));
}
QObject* BinaryStream::readIEEE(){
    skipOptionalSuffix();if(m_optionalSuffix || m_rx.size()<2)return nullptr;
    if(m_rx[0]!='#'||m_rx[1]<'1'||m_rx[1]>'9'){
        m_error=QStringLiteral("Expected IEEE #1..#9 definite-length block at buffer start");return nullptr;
    }
    const int digits=m_rx[1]-'0';if(m_rx.size()<2+digits)return nullptr;
    qint64 len=0;
    for(int i=0;i<digits;++i){
        const char c=m_rx[2+i];
        if(c<'0'||c>'9'){m_error=QStringLiteral("Invalid IEEE length digit");return nullptr;}
        len=len*10+(c-'0');
        if(len>m_limit){m_error=QStringLiteral("IEEE block exceeds stream maximum");return nullptr;}
    }
    const qint64 total=2+digits+len;
    if(total>m_rx.size())return nullptr;
    QByteArray result=m_rx.mid(2+digits,int(len));
    m_rx.remove(0,int(total));
    m_optionalSuffix=true;skipOptionalSuffix();m_error.clear();
    return jsOwned(new BinaryBuffer(std::move(result)));
}
void BinaryStream::clear(){m_rx.clear();m_error.clear();m_optionalSuffix=false;}
BinaryBridge::BinaryBridge(QJSEngine* engine,QObject* parent):QObject(parent),m_engine(engine){m_clock.start();}
QObject* BinaryBridge::takeObject(QObject* o){
    // QJSEngine owns the wrappers. Compact dead weak references periodically
    // so long-running TCP streams do not retain unbounded pointer records.
    if(m_objects.size()>1024){
        QVector<QPointer<QObject>> alive;
        alive.reserve(m_objects.size());
        for(const auto& p:m_objects)if(p)alive.append(p);
        m_objects.swap(alive);
    }
    m_objects.append(QPointer<QObject>(o));return jsOwned(o);
}
bool BinaryBridge::checkSize(qint64 size){
    m_error.clear();if(size<0 || size>kMaxBinary){m_error=QStringLiteral("Binary allocation exceeds 8 MiB or is negative");return false;}return true;
}
BinaryBuffer* BinaryBridge::fromNativeBytes(const QByteArray& data){
    if(!checkSize(data.size()))return nullptr;
    auto* b=new BinaryBuffer(data);takeObject(b);return b;
}
QObject* BinaryBridge::create(int n){
    if(!checkSize(n))return nullptr;
    return fromNativeBytes(QByteArray(n,char(0)));
}
QObject* BinaryBridge::pattern(int n){
    if(!checkSize(n))return nullptr;
    QByteArray data(n,Qt::Uninitialized);
    for(int i=0;i<n;++i)data[i]=char((i*73+19)&255);
    return fromNativeBytes(data);
}
QObject* BinaryBridge::wordPattern(int count){
    if(!checkSize(qint64(count)*2))return nullptr;
    QByteArray data(count*2,Qt::Uninitialized);
    for(int i=0;i<count;++i){
        const quint16 v=quint16((qint64(i)*73+1519)&65535);
        data[2*i]=char(v&255);data[2*i+1]=char(v>>8);
    }
    return fromNativeBytes(data);
}
QObject* BinaryBridge::fromArray(const QJSValue& a){
    if(!a.isArray()){m_error=QStringLiteral("Expected JavaScript byte array");return nullptr;}
    const int n=a.property(QStringLiteral("length")).toInt();if(!checkSize(n))return nullptr;
    QByteArray data(n,Qt::Uninitialized);
    for(int i=0;i<n;++i){
        const QJSValue v=a.property(quint32(i));const double x=v.toNumber();
        if(!v.isNumber() || !std::isfinite(x) || x<0 || x>255 || x!=std::floor(x)){
            m_error=QStringLiteral("Invalid byte at index %1").arg(i);return nullptr;
        }
        data[i]=char(int(x));
    }
    return fromNativeBytes(data);
}
QObject* BinaryBridge::fromHex(const QString& input){
    QString clean;clean.reserve(input.size());
    for(QChar ch:input){if(ch.isSpace())continue;if(!((ch>=QLatin1Char('0')&&ch<=QLatin1Char('9')) || (ch.toLower()>=QLatin1Char('a')&&ch.toLower()<=QLatin1Char('f')))){m_error=QStringLiteral("Invalid hexadecimal character");return nullptr;}clean.append(ch);}
    if((clean.size()&1) || !checkSize(clean.size()/2)){m_error=QStringLiteral("Invalid hexadecimal length");return nullptr;}
    return fromNativeBytes(QByteArray::fromHex(clean.toLatin1()));
}
QObject* BinaryBridge::wrapIEEE(QObject* object){
    auto* src=qobject_cast<BinaryBuffer*>(object);
    if(!src || src->isReleased()){m_error=QStringLiteral("Expected live BinaryBuffer");return nullptr;}
    const int n=src->length();if(!checkSize(qint64(n)+12))return nullptr;
    const QByteArray digits=QByteArray::number(n);
    QByteArray framed;framed.reserve(n+digits.size()+3);
    framed.append('#');framed.append(char('0'+digits.size()));framed.append(digits);
    framed.append(src->dataPointer(),src->length());framed.append('\n');
    return fromNativeBytes(framed);
}
QObject* BinaryBridge::stream(int maximum){
    if(!checkSize(maximum) || maximum==0)return nullptr;
    return takeObject(new BinaryStream(m_engine,maximum));
}
double BinaryBridge::clockUs() const {return double(m_clock.nsecsElapsed())/1000.0;}
void BinaryBridge::shutdown(){
    for(auto& p:m_objects){
        if(auto* buf=qobject_cast<BinaryBuffer*>(p.data()))buf->release();
        else if(auto* stream=qobject_cast<BinaryStream*>(p.data()))stream->clear();
    }
    m_objects.clear();m_engine=nullptr;
}

// RC1.3 general binary API ----------------------------------------------------
namespace {
int numericWidth(const QString& name) {
    if(name==QStringLiteral("u8")||name==QStringLiteral("i8"))return 1;
    if(name==QStringLiteral("u16le")||name==QStringLiteral("u16be")||
       name==QStringLiteral("i16le")||name==QStringLiteral("i16be"))return 2;
    if(name==QStringLiteral("u32le")||name==QStringLiteral("u32be")||
       name==QStringLiteral("i32le")||name==QStringLiteral("i32be")||
       name==QStringLiteral("f32le")||name==QStringLiteral("f32be"))return 4;
    if(name==QStringLiteral("f64le")||name==QStringLiteral("f64be"))return 8;
    return 0;
}
bool littleOrder(const QString& order){return order==QStringLiteral("intel") || order==QStringLiteral("little") || order==QStringLiteral("le");}
bool bigOrder(const QString& order){return order==QStringLiteral("motorola") || order==QStringLiteral("big") || order==QStringLiteral("be");}
qint64 asSigned(quint64 raw,int bits) {
    if(bits==64){qint64 result=0;std::memcpy(&result,&raw,sizeof(result));return result;}
    if(raw & (quint64(1)<<(bits-1)))return qint64(raw)-qint64(quint64(1)<<bits);
    return qint64(raw);
}
}
bool BinaryBuffer::checkedNumber(double value,double low,double high){
    if(!std::isfinite(value) || value<low || value>high || std::floor(value)!=value){
        m_error=QStringLiteral("Integer value outside representable range");return false;
    }
    return true;
}
bool BinaryBuffer::readInteger(int offset,int n,bool little,quint64* out){
    if(!available(offset,n))return false;
    const char* p=dataPointer()+offset;
    quint64 val=0;
    for(int i=0;i<n;++i){val=(val<<8)|octet(p,little?n-1-i:i);}
    *out=val;return true;
}
bool BinaryBuffer::writeInteger(int offset,quint64 raw,int n,bool little){
    if(!available(offset,n))return false;
    // QByteArray detach makes view writes copy-on-write, preserving sibling views.
    char* p=m_storage.data()+m_offset+offset;
    for(int i=0;i<n;++i)p[little?i:n-1-i]=char((raw>>(8*i))&255);
    return true;
}
double BinaryBuffer::readFloat(int offset,int n,bool little){
    quint64 raw=0;
    if(!readInteger(offset,n,little,&raw))return std::numeric_limits<double>::quiet_NaN();
    if(n==4){quint32 bits=quint32(raw);float value;std::memcpy(&value,&bits,4);return double(value);}
    double value;std::memcpy(&value,&raw,8);return value;
}
bool BinaryBuffer::writeFloat(int offset,double value,int n,bool little){
    if(!std::isfinite(value)){m_error=QStringLiteral("Nonfinite floating-point input");return false;}
    if(n==4){float f=static_cast<float>(value);quint32 bits=0;std::memcpy(&bits,&f,4);return writeInteger(offset,bits,4,little);}
    quint64 bits=0;std::memcpy(&bits,&value,8);return writeInteger(offset,bits,8,little);
}
int BinaryBuffer::readI8(int o){quint64 v=0;return readInteger(o,1,true,&v)?int(asSigned(v,8)):-129;}
int BinaryBuffer::readI16LE(int o){quint64 v=0;return readInteger(o,2,true,&v)?int(asSigned(v,16)):-32769;}
int BinaryBuffer::readI16BE(int o){quint64 v=0;return readInteger(o,2,false,&v)?int(asSigned(v,16)):-32769;}
double BinaryBuffer::readU32BE(int o){quint64 v=0;return readInteger(o,4,false,&v)?double(v):-1;}
double BinaryBuffer::readI32LE(int o){quint64 v=0;return readInteger(o,4,true,&v)?double(asSigned(v,32)):std::numeric_limits<double>::quiet_NaN();}
double BinaryBuffer::readI32BE(int o){quint64 v=0;return readInteger(o,4,false,&v)?double(asSigned(v,32)):std::numeric_limits<double>::quiet_NaN();}
double BinaryBuffer::readF32BE(int o){return readFloat(o,4,false);}
double BinaryBuffer::readF64LE(int o){return readFloat(o,8,true);}
double BinaryBuffer::readF64BE(int o){return readFloat(o,8,false);}
QString BinaryBuffer::readU64LE(int o){quint64 v=0;return readInteger(o,8,true,&v)?QString::number(v):QString();}
QString BinaryBuffer::readU64BE(int o){quint64 v=0;return readInteger(o,8,false,&v)?QString::number(v):QString();}
QString BinaryBuffer::readI64LE(int o){quint64 v=0;return readInteger(o,8,true,&v)?QString::number(asSigned(v,64)):QString();}
QString BinaryBuffer::readI64BE(int o){quint64 v=0;return readInteger(o,8,false,&v)?QString::number(asSigned(v,64)):QString();}
bool BinaryBuffer::writeU8(int o,double v){if(!checkedNumber(v,0,255))return false;return writeInteger(o,quint64(v),1,true);}
bool BinaryBuffer::writeI8(int o,double v){if(!checkedNumber(v,-128,127))return false;return writeInteger(o,quint64(qint64(v)),1,true);}
bool BinaryBuffer::writeU16LE(int o,double v){if(!checkedNumber(v,0,65535))return false;return writeInteger(o,quint64(v),2,true);}
bool BinaryBuffer::writeU16BE(int o,double v){if(!checkedNumber(v,0,65535))return false;return writeInteger(o,quint64(v),2,false);}
bool BinaryBuffer::writeI16LE(int o,double v){if(!checkedNumber(v,-32768,32767))return false;return writeInteger(o,quint64(qint64(v)),2,true);}
bool BinaryBuffer::writeI16BE(int o,double v){if(!checkedNumber(v,-32768,32767))return false;return writeInteger(o,quint64(qint64(v)),2,false);}
bool BinaryBuffer::writeU32LE(int o,double v){if(!checkedNumber(v,0,4294967295.0))return false;return writeInteger(o,quint64(v),4,true);}
bool BinaryBuffer::writeU32BE(int o,double v){if(!checkedNumber(v,0,4294967295.0))return false;return writeInteger(o,quint64(v),4,false);}
bool BinaryBuffer::writeI32LE(int o,double v){if(!checkedNumber(v,-2147483648.0,2147483647.0))return false;return writeInteger(o,quint64(qint64(v)),4,true);}
bool BinaryBuffer::writeI32BE(int o,double v){if(!checkedNumber(v,-2147483648.0,2147483647.0))return false;return writeInteger(o,quint64(qint64(v)),4,false);}
bool BinaryBuffer::writeF32LE(int o,double v){return writeFloat(o,v,4,true);}
bool BinaryBuffer::writeF32BE(int o,double v){return writeFloat(o,v,4,false);}
bool BinaryBuffer::writeF64LE(int o,double v){return writeFloat(o,v,8,true);}
bool BinaryBuffer::writeF64BE(int o,double v){return writeFloat(o,v,8,false);}
static bool parseDecimal(const QString& value,bool isSigned,quint64* bits){
    if(value.isEmpty())return false;
    const int first=(isSigned && value.at(0)==QLatin1Char('-'))?1:0;
    if(first==value.size())return false;
    for(int i=first;i<value.size();++i){
        const QChar c=value.at(i);
        if(c<QLatin1Char('0') || c>QLatin1Char('9'))return false;
    }
    bool ok=false;
    if(isSigned){const qint64 v=value.toLongLong(&ok,10);if(ok)*bits=quint64(v);}
    else {const quint64 v=value.toULongLong(&ok,10);if(ok)*bits=v;}
    return ok;
}
bool BinaryBuffer::writeU64LE(int o,const QString& v){quint64 bits=0;if(!parseDecimal(v,false,&bits)){m_error=QStringLiteral("Invalid unsigned 64-bit decimal string");return false;}return writeInteger(o,bits,8,true);}
bool BinaryBuffer::writeU64BE(int o,const QString& v){quint64 bits=0;if(!parseDecimal(v,false,&bits)){m_error=QStringLiteral("Invalid unsigned 64-bit decimal string");return false;}return writeInteger(o,bits,8,false);}
bool BinaryBuffer::writeI64LE(int o,const QString& v){quint64 bits=0;if(!parseDecimal(v,true,&bits)){m_error=QStringLiteral("Invalid signed 64-bit decimal string");return false;}return writeInteger(o,bits,8,true);}
bool BinaryBuffer::writeI64BE(int o,const QString& v){quint64 bits=0;if(!parseDecimal(v,true,&bits)){m_error=QStringLiteral("Invalid signed 64-bit decimal string");return false;}return writeInteger(o,bits,8,false);}
bool BinaryBuffer::fill(int value,int offset,int count){
    if(value<0||value>255){m_error=QStringLiteral("Fill byte must be 0..255");return false;}
    if(!available(offset,count))return false;
    std::memset(m_storage.data()+m_offset+offset,value,size_t(count));return true;
}
bool BinaryBuffer::copyFrom(QObject* src,int destOffset,int sourceOffset,int count){
    auto* other=qobject_cast<BinaryBuffer*>(src);
    if(!other || !other->available(sourceOffset,count)){m_error=QStringLiteral("Invalid source BinaryBuffer range");return false;}
    if(!available(destOffset,count))return false;
    // Copy before potential detach handles src==this and overlapping subviews.
    const QByteArray temp(other->dataPointer()+sourceOffset,count);
    std::memcpy(m_storage.data()+m_offset+destOffset,temp.constData(),size_t(count));return true;
}
bool BinaryBuffer::writeUtf8(int offset,const QString& text){
    const QByteArray bytes=text.toUtf8();
    if(!available(offset,bytes.size()))return false;
    std::memcpy(m_storage.data()+m_offset+offset,bytes.constData(),size_t(bytes.size()));return true;
}
QString BinaryBuffer::toUtf8(int offset,int count){
    if(!available(offset,count))return {};
    return QString::fromUtf8(dataPointer()+offset,count);
}
QString BinaryBuffer::toBase64(){
    if(!available(0,m_length))return {};
    return QString::fromLatin1(QByteArray(dataPointer(),m_length).toBase64());
}
double BinaryBuffer::sum32Range(int offset,int count){
    if(!available(offset,count))return -1;
    quint32 sum=0;const char* p=dataPointer()+offset;
    for(int i=0;i<count;++i)sum+=static_cast<unsigned char>(p[i]);
    return double(sum);
}
int BinaryBuffer::crc16Range(int offset,int count){
    if(!available(offset,count))return -1;
    quint16 crc=0xffff;const char* p=dataPointer()+offset;
    for(int i=0;i<count;++i){crc^=static_cast<unsigned char>(p[i]);for(int j=0;j<8;++j)crc=quint16((crc&1)?((crc>>1)^0xa001):(crc>>1));}
    return int(crc);
}
int BinaryBuffer::xor8(int offset,int count){
    if(!available(offset,count))return -1;
    int result=0;const char* p=dataPointer()+offset;
    for(int i=0;i<count;++i) {
        result^=static_cast<unsigned char>(p[i]);
    }
    return result;
}
int BinaryBuffer::bitIndex(int startBit,int step,const QString& order) const {
    if(littleOrder(order))return startBit+step;
    // DBC Motorola sawtooth: bit7 of byte0 follows bit0 of byte1 after
    // the least significant bit in byte0, e.g. startBit 7 width 12 -> ABC0.
    int index=startBit;
    for(int i=0;i<step;++i)index=(index%8==0)?index+15:index-1;
    return index;
}
bool BinaryBuffer::validateBits(int startBit,int width,const QString& order,int maxWidth){
    if(!littleOrder(order)&&!bigOrder(order)){m_error=QStringLiteral("Bit order must be intel/little or motorola/big");return false;}
    if(startBit<0 || startBit/8>=m_length || width<1 || width>maxWidth){m_error=QStringLiteral("DBC bit width out of range or negative startBit");return false;}
    // Avoid O(width * 64) overrun; the largest width is 53.
    const int last=bitIndex(startBit,width-1,order);
    if(last<0 || last/8>=m_length || startBit/8>=m_length){m_error=QStringLiteral("DBC signal outside buffer");return false;}
    if(m_released){m_error=QStringLiteral("BinaryBuffer released");return false;}
    m_error.clear();return true;
}
double BinaryBuffer::readBits(int startBit,int width,const QString& order,bool isSigned){
    if(!validateBits(startBit,width,order))return std::numeric_limits<double>::quiet_NaN();
    quint64 raw=0;
    const char* p=dataPointer();
    for(int k=0;k<width;++k){
        const int b=bitIndex(startBit,k,order);
        const quint64 bit=(octet(p,b/8)>>(b%8))&1;
        if(littleOrder(order))raw|=(bit<<k);
        else raw=(raw<<1)|bit;
    }
    return isSigned?double(asSigned(raw,width)):double(raw);
}
bool BinaryBuffer::writeBits(int startBit,int width,const QString& order,bool isSigned,double value){
    if(!validateBits(startBit,width,order))return false;
    const double maxUnsigned=double((quint64(1)<<width)-1);
    const double low=isSigned?-double(quint64(1)<<(width-1)):0;
    const double high=isSigned?double((quint64(1)<<(width-1))-1):maxUnsigned;
    if(!checkedNumber(value,low,high))return false;
    const quint64 raw=(quint64(qint64(value))&((quint64(1)<<width)-1));
    char* p=m_storage.data()+m_offset;
    for(int k=0;k<width;++k){
        const int b=bitIndex(startBit,k,order);
        const quint64 bit=littleOrder(order)?((raw>>k)&1):((raw>>(width-1-k))&1);
        const unsigned char mask=static_cast<unsigned char>(1u<<(b%8));
        const unsigned char old=octet(p,b/8);
        p[b/8]=char(bit?(old|mask):(old&~mask));
    }
    return true;
}
double BinaryBuffer::readSignal(int startBit,int width,const QString& order,bool isSigned,double factor,double bias){
    if(!std::isfinite(factor)||!std::isfinite(bias)||factor==0){m_error=QStringLiteral("Invalid DBC scaling");return std::numeric_limits<double>::quiet_NaN();}
    const double raw=readBits(startBit,width,order,isSigned);
    return std::isfinite(raw)?raw*factor+bias:raw;
}
bool BinaryBuffer::writeSignal(int startBit,int width,const QString& order,bool isSigned,double factor,double bias,double physical){
    if(!std::isfinite(factor)||!std::isfinite(bias)||!std::isfinite(physical)||factor==0){m_error=QStringLiteral("Invalid DBC scaling");return false;}
    const double raw=std::round((physical-bias)/factor);
    return writeBits(startBit,width,order,isSigned,raw);
}
QJSValue BinaryBuffer::statsNumeric(const QString& type,int byteOffset,int count,double scale,double bias){
    QObject* data=decodeSamples(type,byteOffset,count,scale,bias);
    if(!data)return QJSValue(QJSValue::NullValue);
    auto* samples=qobject_cast<BinarySamples*>(data);
    auto* engine=qjsEngine(this);
    if(!engine || samples->m_values.isEmpty()){
        m_error=QStringLiteral("Cannot calculate statistics from empty data");
        delete samples;return QJSValue(QJSValue::NullValue);
    }
    const QVector<double>& values=samples->m_values;
    double mn=values[0],mx=mn,sum=0,sumsq=0;
    for(double v:values){mn=std::min(mn,v);mx=std::max(mx,v);sum+=v;sumsq+=v*v;}
    QJSValue ret=engine->newObject();
    ret.setProperty(QStringLiteral("count"),int(values.size()));
    ret.setProperty(QStringLiteral("min"),mn);ret.setProperty(QStringLiteral("max"),mx);
    ret.setProperty(QStringLiteral("sum"),sum);ret.setProperty(QStringLiteral("mean"),sum/values.size());
    ret.setProperty(QStringLiteral("rms"),std::sqrt(sumsq/values.size()));
    delete samples;
    return ret;
}
QObject* BinaryBuffer::decodeSamples(const QString& rawType,int byteOffset,int count,double scale,double bias){
    const QString type=rawType.toLower();const int width=numericWidth(type);
    if(!width){m_error=QStringLiteral("Unknown numeric sample type");return nullptr;}
    if(count<0 || count>1000000 || !std::isfinite(scale) || !std::isfinite(bias) ||
       qint64(count)*width>INT_MAX || !available(byteOffset,count*width)){
        if(m_error.isEmpty()) {
            m_error=QStringLiteral("Invalid sample count, scale or range");
        }
        return nullptr;
    }
    QVector<double> values;values.reserve(count);
    const bool le=type.endsWith(QStringLiteral("le"));
    for(int i=0;i<count;++i){
        const int offset=byteOffset+i*width;
        quint64 raw=0;double v=0;
        if(type.startsWith(QLatin1Char('f'))){v=readFloat(offset,width,le);}
        else {
            if(!readInteger(offset,width,le,&raw))return nullptr;
            const bool signedValue=type.startsWith(QLatin1Char('i'));
            v=signedValue?double(asSigned(raw,width*8)):double(raw);
        }
        v=v*scale+bias;
        if(!std::isfinite(v)){m_error=QStringLiteral("Nonfinite sample in bulk decode");return nullptr;}
        values.append(v);
    }
    m_error.clear();return jsOwned(new BinarySamples(std::move(values)));
}
BinarySamples::BinarySamples(QVector<double> values):m_values(std::move(values)){}
QJSValue BinarySamples::stats(){
    m_error.clear();auto* engine=qjsEngine(this);
    if(!engine || m_values.isEmpty()){m_error=QStringLiteral("Empty native sample buffer");return QJSValue(QJSValue::NullValue);}
    double min=m_values[0],max=min,sum=0,ss=0;
    for(double v:m_values){min=std::min(min,v);max=std::max(max,v);sum+=v;ss+=v*v;}
    QJSValue result=engine->newObject();
    result.setProperty(QStringLiteral("count"),int(m_values.size()));
    result.setProperty(QStringLiteral("min"),min);result.setProperty(QStringLiteral("max"),max);
    result.setProperty(QStringLiteral("sum"),sum);result.setProperty(QStringLiteral("mean"),sum/m_values.size());
    result.setProperty(QStringLiteral("rms"),std::sqrt(ss/m_values.size()));return result;
}
QJSValue BinarySamples::minMaxBins(int bins){
    auto* engine=qjsEngine(this);
    if(!engine || bins<1 || bins>2048 || m_values.isEmpty()){
        m_error=QStringLiteral("Bins must be 1..2048 and samples nonempty");return QJSValue(QJSValue::NullValue);
    }
    m_error.clear();const int n=m_values.size();const int limit=std::min(bins,n);
    QJSValue arr=engine->newArray(quint32(limit));
    for(int i=0;i<limit;++i){
        const int start=int((qint64(i)*n)/limit);
        const int end=int((qint64(i+1)*n)/limit);
        double mn=m_values[start],mx=mn;
        for(int j=start+1;j<end;++j){mn=std::min(mn,m_values[j]);mx=std::max(mx,m_values[j]);}
        QJSValue row=engine->newObject();row.setProperty(QStringLiteral("start"),start);
        row.setProperty(QStringLiteral("end"),end);row.setProperty(QStringLiteral("min"),mn);
        row.setProperty(QStringLiteral("max"),mx);arr.setProperty(quint32(i),row);
    }
    return arr;
}
QJSValue BinarySamples::toArray(int offset,int count){
    auto* engine=qjsEngine(this);
    if(!engine || count<0||count>kMaxSmallExport||offset<0||qint64(offset)+count>m_values.size()){
        m_error=QStringLiteral("Sample export exceeds view or 4096-item limit");return QJSValue(QJSValue::NullValue);
    }
    m_error.clear();QJSValue result=engine->newArray(quint32(count));
    for(int i=0;i<count;++i) {
        result.setProperty(quint32(i),m_values[offset+i]);
    }
    return result;
}
QJSValue BinarySamples::rangeStats(int offset,int count){
    auto* engine=qjsEngine(this);
    if(!engine || count<=0 || offset<0 || qint64(offset)+count>m_values.size()){
        m_error=QStringLiteral("Invalid native sample range");return QJSValue(QJSValue::NullValue);
    }
    double low=m_values[offset],high=low,sum=0,squared=0;
    for(int i=offset;i<offset+count;++i){
        const double v=m_values.at(i);
        low=std::min(low,v);high=std::max(high,v);sum+=v;squared+=v*v;
    }
    m_error.clear();QJSValue result=engine->newObject();
    result.setProperty(QStringLiteral("start"),offset);result.setProperty(QStringLiteral("count"),count);
    result.setProperty(QStringLiteral("min"),low);result.setProperty(QStringLiteral("max"),high);
    result.setProperty(QStringLiteral("mean"),sum/count);result.setProperty(QStringLiteral("rms"),std::sqrt(squared/count));
    return result;
}
double BinarySamples::at(int i){if(i<0||i>=m_values.size()){m_error=QStringLiteral("Sample index out of range");return std::numeric_limits<double>::quiet_NaN();}m_error.clear();return m_values[i];}
void BinarySamples::release(){m_values.clear();m_error.clear();}
QObject* BinaryStream::readFixed(int count){
    skipOptionalSuffix();m_error.clear();
    if(count<0 || count>m_limit){m_error=QStringLiteral("Invalid fixed frame length");return nullptr;}
    if(m_optionalSuffix || m_rx.size()<count)return nullptr;
    const QByteArray payload=m_rx.left(count);m_rx.remove(0,count);
    return jsOwned(new BinaryBuffer(payload));
}
QObject* BinaryStream::readLengthPrefixed(int prefixBytes,const QString& order,bool includePrefix){
    skipOptionalSuffix();m_error.clear();
    if(prefixBytes!=1 && prefixBytes!=2 && prefixBytes!=4){m_error=QStringLiteral("Length prefix must be 1,2,4 bytes");return nullptr;}
    if(!littleOrder(order)&&!bigOrder(order)){m_error=QStringLiteral("Invalid length prefix endian order");return nullptr;}
    if(m_optionalSuffix || m_rx.size()<prefixBytes)return nullptr;
    quint64 n=0;
    for(int i=0;i<prefixBytes;++i)n=(n<<8)|octet(m_rx.constData(),littleOrder(order)?prefixBytes-1-i:i);
    const quint64 total=quint64(prefixBytes)+n;
    if(total>quint64(m_limit)){m_error=QStringLiteral("Length-prefixed frame exceeds stream limit");return nullptr;}
    if(quint64(m_rx.size())<total)return nullptr;
    const QByteArray result=includePrefix?m_rx.left(int(total)):m_rx.mid(prefixBytes,int(n));
    m_rx.remove(0,int(total));return jsOwned(new BinaryBuffer(result));
}
QObject* BinaryBridge::fromUtf8(const QString& value){return fromNativeBytes(value.toUtf8());}
QObject* BinaryBridge::fromBase64(const QString& value){
    // Use Qt's strict decoder: corrupt/ambiguous base64 is rejected, not ignored.
    const QByteArray input=value.toLatin1();
    if(input.size()>qint64(kMaxBinary)*2){m_error=QStringLiteral("Base64 input too large");return nullptr;}
    const auto result=QByteArray::fromBase64Encoding(input,QByteArray::AbortOnBase64DecodingErrors);
    if(!result){m_error=QStringLiteral("Invalid Base64 input");return nullptr;}
    return fromNativeBytes(result.decoded);
}
QObject* BinaryBridge::concat(const QJSValue& array){
    if(!array.isArray()){m_error=QStringLiteral("concat requires a JavaScript array of BinaryBuffers");return nullptr;}
    const int count=array.property(QStringLiteral("length")).toInt();
    if(count<0 || count>65536){m_error=QStringLiteral("Too many buffers to concatenate");return nullptr;}
    qint64 total=0;
    for(int i=0;i<count;++i){
        auto* b=qobject_cast<BinaryBuffer*>(array.property(quint32(i)).toQObject());
        if(!b || b->m_released){m_error=QStringLiteral("concat requires live BinaryBuffers");return nullptr;}
        total+=b->length();if(!checkSize(total))return nullptr;
    }
    QByteArray data;data.reserve(int(total));
    for(int i=0;i<count;++i){
        auto* b=qobject_cast<BinaryBuffer*>(array.property(quint32(i)).toQObject());
        data.append(b->dataPointer(),b->length());
    }
    return fromNativeBytes(data);
}

double BinaryBuffer::crc32(){return crc32Range(0,m_length);}
double BinaryBuffer::crc32Range(int offset,int count){
    if(!available(offset,count))return -1;
    quint32 crc=0xFFFFFFFFu;
    const char* p=dataPointer()+offset;
    for(int i=0;i<count;++i){
        crc^=static_cast<unsigned char>(p[i]);
        for(int bit=0;bit<8;++bit)crc=(crc&1)?((crc>>1)^0xEDB88320u):(crc>>1);
    }
    return double(crc^0xFFFFFFFFu);
}
int BinaryBuffer::indexOf(QObject* needle,int start){
    auto* part=qobject_cast<BinaryBuffer*>(needle);
    if(!part || part->m_released){m_error=QStringLiteral("indexOf requires a live BinaryBuffer");return -1;}
    if(start<0 || !available(start,0))return -1;
    if(part->length()==0)return start;
    const QByteArray hay(dataPointer()+start,m_length-start);
    const int found=hay.indexOf(QByteArray(part->dataPointer(),part->length()));
    return found<0?-1:start+found;
}
bool BinaryBuffer::equals(QObject* other){
    auto* rhs=qobject_cast<BinaryBuffer*>(other);
    if(!rhs || rhs->m_released || !available(0,m_length)){
        m_error=QStringLiteral("equals requires a live BinaryBuffer");return false;
    }
    return rhs->length()==m_length &&
           std::memcmp(dataPointer(),rhs->dataPointer(),size_t(m_length))==0;
}
QString BinaryBuffer::readBits64(int startBit,int width,const QString& order,bool isSigned){
    if(!validateBits(startBit,width,order,64))return {};
    quint64 raw=0;
    const char* p=dataPointer();
    for(int k=0;k<width;++k){
        const int bitIndexValue=bitIndex(startBit,k,order);
        const quint64 bit=(octet(p,bitIndexValue/8)>>(bitIndexValue%8))&1;
        if(littleOrder(order))raw|=(bit<<k);
        else raw=(raw<<1)|bit;
    }
    return isSigned?QString::number(asSigned(raw,width)):QString::number(raw);
}
bool BinaryBuffer::writeBits64(int startBit,int width,const QString& order,bool isSigned,const QString& decimal){
    if(!validateBits(startBit,width,order,64))return false;
    bool ok=false;quint64 raw=0;
    if(isSigned){
        const qint64 value=decimal.toLongLong(&ok,10);
        if(ok && width<64){
            const qint64 low=-(qint64(1)<<(width-1));
            const qint64 high=(qint64(1)<<(width-1))-1;
            if(value<low||value>high)ok=false;
        }
        raw=quint64(value);
    }else{
        raw=decimal.toULongLong(&ok,10);
        if(ok && width<64 && raw>((quint64(1)<<width)-1))ok=false;
    }
    if(!ok){m_error=QStringLiteral("DBC 64-bit decimal value outside signal range");return false;}
    const quint64 masked=width==64?raw:(raw&((quint64(1)<<width)-1));
    char* p=m_storage.data()+m_offset;
    for(int k=0;k<width;++k){
        const int bitIndexValue=bitIndex(startBit,k,order);
        const quint64 bit=littleOrder(order)?((masked>>k)&1):((masked>>(width-1-k))&1);
        const unsigned char mask=static_cast<unsigned char>(1u<<(bitIndexValue%8));
        const unsigned char old=octet(p,bitIndexValue/8);
        p[bitIndexValue/8]=char(bit?(old|mask):(old&~mask));
    }
    return true;
}
QObject* BinaryStream::readUntil(QObject* marker,bool includeDelimiter){
    skipOptionalSuffix();m_error.clear();
    auto* delim=qobject_cast<BinaryBuffer*>(marker);
    if(!delim || delim->m_released || delim->length()<1){
        m_error=QStringLiteral("Nonempty BinaryBuffer delimiter required");return nullptr;
    }
    if(m_optionalSuffix)return nullptr;
    const QByteArray bytes(delim->dataPointer(),delim->length());
    const int index=m_rx.indexOf(bytes);
    if(index<0)return nullptr;
    const int consumed=index+bytes.size();
    const QByteArray payload=m_rx.left(includeDelimiter?consumed:index);
    m_rx.remove(0,consumed);
    return jsOwned(new BinaryBuffer(payload));
}
