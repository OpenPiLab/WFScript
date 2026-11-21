#pragma once
// WFScript Native Binary Lab RC1.3: bounded, reusable binary values.
// All objects run on the owning PanelRuntime/QJSEngine worker thread.
// Buffer slices share native bytes until a write detaches (copy-on-write).
#include <QObject>
#include <QByteArray>
#include <QJSValue>
#include <QPointer>
#include <QElapsedTimer>
#include <QVariantMap>
#include <QVector>

class QJSEngine;
class BinaryBridge;

class BinarySamples final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int length READ length)
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit BinarySamples(QVector<double> values);
    int length() const {return m_values.size();}
    const QVector<double>& nativeValues() const {return m_values;} // internal, never JS arrays
    QString lastError() const {return m_error;}
    Q_INVOKABLE QJSValue stats();
    Q_INVOKABLE QJSValue minMaxBins(int bins); // bounded 1..2048, for plot downsampling
    Q_INVOKABLE QJSValue toArray(int offset,int count); // max 4096 values
    Q_INVOKABLE QJSValue rangeStats(int offset,int count); // bounded C++ computation; arbitrary view
    Q_INVOKABLE double at(int index); // convenience only; prefer bulk methods
    Q_INVOKABLE void release();
private:
    QVector<double> m_values;
    QString m_error;
    friend class BinaryBuffer;
};

class BinaryBuffer final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int length READ length)
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit BinaryBuffer(QByteArray storage,int offset=0,int length=-1);
    int length() const {return m_length;}
    const char* dataPointer() const {return m_storage.constData()+m_offset;}
    QByteArray bytesCopy() const {return m_storage.mid(m_offset,m_length);}
    QString lastError() const {return m_error;}
    bool isReleased() const {return m_released;} // native ownership guard; not a JS API
    Q_INVOKABLE QObject* slice(int offset,int count);
    Q_INVOKABLE QObject* ieeePayload();
    Q_INVOKABLE int readU8(int offset);
    Q_INVOKABLE int readI8(int offset);
    Q_INVOKABLE int readU16LE(int offset);
    Q_INVOKABLE int readU16BE(int offset);
    Q_INVOKABLE int readI16LE(int offset);
    Q_INVOKABLE int readI16BE(int offset);
    Q_INVOKABLE double readU32LE(int offset);
    Q_INVOKABLE double readU32BE(int offset);
    Q_INVOKABLE double readI32LE(int offset);
    Q_INVOKABLE double readI32BE(int offset);
    Q_INVOKABLE double readF32LE(int offset);
    Q_INVOKABLE double readF32BE(int offset);
    Q_INVOKABLE double readF64LE(int offset);
    Q_INVOKABLE double readF64BE(int offset);
    // Decimal strings prevent loss of 64-bit precision in QJSEngine Numbers.
    Q_INVOKABLE QString readU64LE(int offset);
    Q_INVOKABLE QString readU64BE(int offset);
    Q_INVOKABLE QString readI64LE(int offset);
    Q_INVOKABLE QString readI64BE(int offset);
    Q_INVOKABLE bool writeU8(int offset,double value);
    Q_INVOKABLE bool writeI8(int offset,double value);
    Q_INVOKABLE bool writeU16LE(int offset,double value);
    Q_INVOKABLE bool writeU16BE(int offset,double value);
    Q_INVOKABLE bool writeI16LE(int offset,double value);
    Q_INVOKABLE bool writeI16BE(int offset,double value);
    Q_INVOKABLE bool writeU32LE(int offset,double value);
    Q_INVOKABLE bool writeU32BE(int offset,double value);
    Q_INVOKABLE bool writeI32LE(int offset,double value);
    Q_INVOKABLE bool writeI32BE(int offset,double value);
    Q_INVOKABLE bool writeF32LE(int offset,double value);
    Q_INVOKABLE bool writeF32BE(int offset,double value);
    Q_INVOKABLE bool writeF64LE(int offset,double value);
    Q_INVOKABLE bool writeF64BE(int offset,double value);
    Q_INVOKABLE bool writeU64LE(int offset,const QString& decimal);
    Q_INVOKABLE bool writeU64BE(int offset,const QString& decimal);
    Q_INVOKABLE bool writeI64LE(int offset,const QString& decimal);
    Q_INVOKABLE bool writeI64BE(int offset,const QString& decimal);
    Q_INVOKABLE bool fill(int value,int offset,int count);
    Q_INVOKABLE bool copyFrom(QObject* source,int destOffset,int sourceOffset,int count);
    Q_INVOKABLE bool writeUtf8(int offset,const QString& text);
    Q_INVOKABLE QString toUtf8(int offset,int count);
    Q_INVOKABLE QString toBase64();
    Q_INVOKABLE double sum32();
    Q_INVOKABLE double sum32Range(int offset,int count);
    Q_INVOKABLE int crc16(); // Modbus CRC-16, polynomial 0xA001, seed 0xFFFF
    Q_INVOKABLE int crc16Range(int offset,int count);
    Q_INVOKABLE int xor8(int offset,int count);
    Q_INVOKABLE double crc32(); // CRC-32/ISO-HDLC (standard ZIP/Ethernet)
    Q_INVOKABLE double crc32Range(int offset,int count);
    Q_INVOKABLE int indexOf(QObject* needle,int start);
    Q_INVOKABLE bool equals(QObject* other);

    // CAN DBC Intel/Motorola sawtooth numbering, 1..53 exact bits.
    Q_INVOKABLE double readBits(int startBit,int bitLength,const QString& order,bool isSigned);
    Q_INVOKABLE bool writeBits(int startBit,int bitLength,const QString& order,bool isSigned,double raw);
    Q_INVOKABLE QString readBits64(int startBit,int bitLength,const QString& order,bool isSigned);
    Q_INVOKABLE bool writeBits64(int startBit,int bitLength,const QString& order,bool isSigned,const QString& decimal);

    Q_INVOKABLE double readSignal(int startBit,int bitLength,const QString& order,bool isSigned,double factor,double bias);
    Q_INVOKABLE bool writeSignal(int startBit,int bitLength,const QString& order,bool isSigned,double factor,double bias,double physical);
    Q_INVOKABLE QJSValue statsU16LE(int byteOffset,int count);
    Q_INVOKABLE QJSValue statsNumeric(const QString& type,int byteOffset,int count,double scale,double bias);
    Q_INVOKABLE QObject* decodeSamples(const QString& type,int byteOffset,int count,double scale,double bias);
    Q_INVOKABLE QString toHex(int offset,int count);
    Q_INVOKABLE QJSValue toArray(int offset,int count);
    Q_INVOKABLE void release();
private:
    QByteArray m_storage;
    int m_offset=0;
    int m_length=0;
    QString m_error;
    bool m_released=false;
    bool available(int pos,int n);
    bool checkedNumber(double value,double min,double max);
    bool writeInteger(int offset,quint64 raw,int n,bool little);
    bool writeFloat(int offset,double value,int n,bool little);
    bool readInteger(int offset,int n,bool little,quint64* result);
    double readFloat(int offset,int n,bool little);
    int bitIndex(int startBit,int step,const QString& order) const;
    bool validateBits(int startBit,int width,const QString& order,int maxWidth=53);
    friend class BinaryStream;
    friend class BinaryBridge;
};

class BinaryStream final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int pending READ pending)
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit BinaryStream(QJSEngine* engine,int limit);
    int pending() const {return m_rx.size();}
    QString lastError() const {return m_error;}
    Q_INVOKABLE bool append(QObject* value);
    Q_INVOKABLE QJSValue readLine();
    Q_INVOKABLE QObject* readIEEE();
    Q_INVOKABLE QObject* readFixed(int count);
    Q_INVOKABLE QObject* readUntil(QObject* delimiter,bool includeDelimiter);
    Q_INVOKABLE QObject* readLengthPrefixed(int prefixBytes,const QString& order,bool includePrefix);
    Q_INVOKABLE void clear();
private:
    QByteArray m_rx;
    QJSEngine* m_engine=nullptr;
    int m_limit;
    QString m_error;
    bool m_optionalSuffix=false;
    void skipOptionalSuffix();
};

class BinaryBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit BinaryBridge(QJSEngine* engine,QObject* parent=nullptr);
    QString lastError() const {return m_error;}
    Q_INVOKABLE QObject* create(int size);
    Q_INVOKABLE QObject* pattern(int bytes);
    Q_INVOKABLE QObject* wordPattern(int samples);
    Q_INVOKABLE QObject* fromArray(const QJSValue& bytes);
    Q_INVOKABLE QObject* fromHex(const QString& text);
    Q_INVOKABLE QObject* fromUtf8(const QString& text);
    Q_INVOKABLE QObject* fromBase64(const QString& text);
    Q_INVOKABLE QObject* concat(const QJSValue& buffers);
    Q_INVOKABLE QObject* wrapIEEE(QObject* payload);
    Q_INVOKABLE QObject* stream(int maxBytes);
    Q_INVOKABLE double clockUs() const;
    BinaryBuffer* fromNativeBytes(const QByteArray& data);
    void shutdown();
private:
    QJSEngine* m_engine;
    QElapsedTimer m_clock;
    QString m_error;
    QVector<QPointer<QObject>> m_objects;
    QObject* takeObject(QObject* obj);
    bool checkSize(qint64 size);
};
