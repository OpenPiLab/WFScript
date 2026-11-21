#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QJSValue>
#include <QTimer>
#include <QByteArray>

class DwfRuntime;
class ProtocolBridge;

// Protocol transactions execute on WFScript's existing script worker thread.
// The native SDK owns the actual signaling timing; no GUI-thread SDK access.
class ProtocolPortBase : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool simulation READ simulation CONSTANT)
    Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)
public:
    ProtocolPortBase(const QString& kind,DwfRuntime* runtime,QObject* parent=nullptr);
    QString lastError() const {return m_error;}
    bool simulation() const {return m_runtime==nullptr;}
    bool configured() const {return m_configured;}
    virtual void invalidate();
    bool stopPort();
signals:
    void lastErrorChanged();
    void configuredChanged();
protected:
    bool configurePort(const QVariantMap& settings);
    QVariantMap execute(const QString& action,const QVariantMap& args={});
    QVariantMap failed(const QString& reason);
    static QVariantMap defaultResult();
    void setError(const QString& message);
    DwfRuntime* m_runtime=nullptr;
    QString m_kind;
    QString m_error;
    bool m_configured=false;
};

class QJSEngine;
class ProtocolUartPort final : public ProtocolPortBase {
    Q_OBJECT
    Q_PROPERTY(bool autoReceiving READ autoReceiving NOTIFY autoReceivingChanged)
public:
    explicit ProtocolUartPort(DwfRuntime* runtime,QObject* parent=nullptr);
    Q_INVOKABLE bool configure(const QVariantMap& settings);
    Q_INVOKABLE bool startRx();
    Q_INVOKABLE bool Receiver() {return startRx();}
    Q_INVOKABLE QVariantMap write(const QVariantList& data);
    Q_INVOKABLE QVariantMap writeText(const QString& text);
    Q_INVOKABLE QVariantMap read(int maxBytes=4096);
    Q_INVOKABLE bool Send(const QString& text,bool appendLF=false);
    Q_INVOKABLE bool SendArray(const QVariantList& bytes,bool appendLF=false);
    Q_INVOKABLE QVariantList ReceiveArray();
    Q_INVOKABLE QString Receive();
    // Opt-in Qt event-loop polling. Manual read() remains SDK-based when off;
    // while on, read() consumes ONLY this bounded application queue.
    Q_INVOKABLE bool startAutoRx(const QVariantMap& options=QVariantMap());
    Q_INVOKABLE bool stopAutoRx(); // stop polling; Protocol1.stop() releases hardware
    Q_INVOKABLE bool onData(const QJSValue& callback);
    Q_INVOKABLE bool clearOnData();
    Q_INVOKABLE int available() const {return m_rxQueue.size();}
    Q_INVOKABLE QVariantMap stats() const;
    bool autoReceiving() const {return m_autoReceiving;}
    bool hasOnData() const {return m_onData.isCallable();}
    QJSValue dispatchOnData(QJSEngine* engine,const QVariantMap& chunk);
    void invalidate() override;
signals:
    void dataReceived(const QVariantMap& chunk); // native Qt -> GUI and script runner
    void receiveError(const QString& message);
    void autoReceivingChanged();
    void queueChanged();
private:
    friend class ProtocolUartTestAccess;
    void pollReceive();
    void ingest(const QVariantMap& result); // one SDK result; testable without AD3
    QVariantMap queuedRead(int maxBytes);
    QTimer m_rxTimer;
    QJSValue m_onData;
    QByteArray m_rxQueue;
    int m_capacity=65536;
    bool m_autoReceiving=false;
    bool m_rxStarted=false;
    bool m_sdkOverflowPending=false;
    int m_parityErrorPending=0;
    qint64 m_receivedBytes=0;
    qint64 m_readBytes=0;
    qint64 m_queueDroppedBytes=0;
    qint64 m_sdkOverflows=0;
    qint64 m_parityErrors=0;
};

class ProtocolSpiPort final : public ProtocolPortBase {
    Q_OBJECT
public:
    explicit ProtocolSpiPort(DwfRuntime* runtime,QObject* parent=nullptr);
    Q_INVOKABLE bool configure(const QVariantMap& settings);
    Q_INVOKABLE QVariantMap transfer(const QVariantList& tx,int readCount=-1,int bits=8,int lanes=1);
    Q_INVOKABLE QVariantMap command(int commandBits,int commandByte,int dummyBits,const QVariantList& tx,int readCount,int bits=8,int lanes=1);
    Q_INVOKABLE bool Start(); // manually assert configured CS
    Q_INVOKABLE bool Stop();  // manually release configured CS, not the entire Protocol engine
    Q_INVOKABLE QVariantList ReadWrite(int bits,const QVariantList& words);
    Q_INVOKABLE bool Write(int bits,const QVariantList& words);
    Q_INVOKABLE QVariantList Read(int bits,int count);
    Q_INVOKABLE QVariantList Read0(int bits,int count);
    Q_INVOKABLE bool WriteDual(int bits,const QVariantList& words);
    Q_INVOKABLE QVariantList ReadDual(int bits,int count);
    Q_INVOKABLE bool WriteQuad(int bits,const QVariantList& words);
    Q_INVOKABLE QVariantList ReadQuad(int bits,int count);
private:
    int m_select=0;
    bool m_activeLow=true;
};

class ProtocolI2cPort final : public ProtocolPortBase {
    Q_OBJECT
public:
    explicit ProtocolI2cPort(DwfRuntime* runtime,QObject* parent=nullptr);
    Q_INVOKABLE bool configure(const QVariantMap& settings);
    Q_INVOKABLE QVariantMap writeRead(int address7,const QVariantList& tx,int readCount);
    Q_INVOKABLE QVariantMap write(int address7,const QVariantList& data);
    Q_INVOKABLE QVariantMap read(int address7,int count);
    Q_INVOKABLE bool Clear();
    Q_INVOKABLE bool Receiver(); // start native I2C Spy
    Q_INVOKABLE QVariantMap spyPoll(int maxBytes=256);
    Q_INVOKABLE QVariantList Receive(); // WaveForms-style signed 9-bit entries (approximation)
    Q_INVOKABLE int Write(int address7,const QVariantList& data);
    Q_INVOKABLE QVariantList Read(int address7,int count);
    Q_INVOKABLE QVariantList ReadSub(int address7,const QVariantList& subaddress,int count);
};

class ProtocolCanPort final : public ProtocolPortBase {
    Q_OBJECT
public:
    explicit ProtocolCanPort(DwfRuntime* runtime,QObject* parent=nullptr);
    Q_INVOKABLE bool configure(const QVariantMap& settings);
    Q_INVOKABLE bool startRx();
    Q_INVOKABLE bool Receiver(){return startRx();}
    Q_INVOKABLE QVariantMap send(int id,bool extended,bool remote,const QVariantList& bytes);
    Q_INVOKABLE QVariantMap receive();
    Q_INVOKABLE bool Send(int baseId,int ide,int extensionId,int remote,int dlc,const QVariantList& bytes);
    Q_INVOKABLE QVariantList Receive();
};

class ProtocolSwdPort final : public ProtocolPortBase {
    Q_OBJECT
public:
    explicit ProtocolSwdPort(DwfRuntime* runtime,QObject* parent=nullptr);
    Q_INVOKABLE bool configure(const QVariantMap& settings);
    Q_INVOKABLE bool clear(int resetClocks=52,int trailClocks=16);
    Q_INVOKABLE QVariantMap readRegister(bool accessPort,int addressIndex);
    Q_INVOKABLE QVariantMap writeRegister(bool accessPort,int addressIndex,qulonglong value);
};

// Native object hierarchy for both the panel and standalone script engines.
// Protocol and Protocol1 are aliases pointing to this same object.
class ProtocolBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* UART READ uart CONSTANT)
    Q_PROPERTY(QObject* SPI READ spi CONSTANT)
    Q_PROPERTY(QObject* I2C READ i2c CONSTANT)
    Q_PROPERTY(QObject* CAN READ can CONSTANT)
    Q_PROPERTY(QObject* SWD READ swd CONSTANT)
    Q_PROPERTY(QString lastError READ lastError)
    Q_PROPERTY(bool simulation READ simulation CONSTANT)
public:
    explicit ProtocolBridge(DwfRuntime* runtime,QObject* parent=nullptr);
    QObject* uart(){return &m_uart;}
    QObject* spi(){return &m_spi;}
    QObject* i2c(){return &m_i2c;}
    QObject* can(){return &m_can;}
    QObject* swd(){return &m_swd;}
    QString lastError() const {return m_error;}
    bool simulation() const {return !m_runtime;}
    Q_INVOKABLE bool stop(); // global release; native high-Z attempt on all protocol pins
    Q_INVOKABLE QVariantMap status() const;
private:
    DwfRuntime* m_runtime=nullptr;
    ProtocolUartPort m_uart;
    ProtocolSpiPort m_spi;
    ProtocolI2cPort m_i2c;
    ProtocolCanPort m_can;
    ProtocolSwdPort m_swd;
    QString m_error;
};
