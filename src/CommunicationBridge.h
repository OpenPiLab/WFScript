#pragma once
// WFScript v0.2-dev.6: opt-in native, nonblocking PC serial/TCP/UDP.
// Lives on the PanelRuntime/QJSEngine worker event loop. No device or socket
// is opened simply by constructing the bridge or loading a .wfpanel.
#include <QObject>
#include <QHash>
#include <QJSValue>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QByteArray>
#include <QString>
#include <QList>

class QJSEngine;
class QIODevice;
class QTcpSocket;
class QTcpServer;
class QUdpSocket;
class QSerialPort;
class BinaryBridge;
class BinaryBuffer;

class CommunicationBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit CommunicationBridge(QJSEngine* engine, BinaryBridge* binary=nullptr,QObject* parent=nullptr);
    ~CommunicationBridge() override;
    Q_INVOKABLE QJSValue ports() const;
    Q_INVOKABLE int createSerial(const QJSValue& options);
    Q_INVOKABLE int createTcp(const QJSValue& options);
    Q_INVOKABLE int createServer(const QJSValue& options);
    Q_INVOKABLE int createUdp(const QJSValue& options);
    Q_INVOKABLE bool on(int id,const QString& event,const QJSValue& callback);
    Q_INVOKABLE bool off(int id,const QString& event);
    Q_INVOKABLE bool start(int id);
    Q_INVOKABLE int write(int id,const QJSValue& value);
    Q_INVOKABLE int send(int id,const QJSValue& value,const QString& host,int port);
    Q_INVOKABLE bool close(int id);
    Q_INVOKABLE QJSValue info(int id) const;
    QString lastError() const { return m_lastError; }
    void shutdown();
    static QString bootstrapSource();
signals:
    void callbackStarted();
    void callbackFinished();
    void callbackFailed(const QString& context,const QJSValue& error);
private:
    enum class Kind { Serial, Tcp, Server, Udp };
    struct Entry {
        int id=0;
        Kind kind=Kind::Tcp;
        QObject* object=nullptr;
        QVariantMap options;
        QHash<QString,QList<QJSValue>> listeners;
        QByteArray lineBuffer;
        qint64 received=0, sent=0;
        qint64 legacyDispatchUs=0, nativeDispatchUs=0;
        qint64 legacyBytes=0, nativeBytes=0;
        qint64 legacyEvents=0, nativeEvents=0;
        bool closing=false;
        bool closeEmitted=false;
    };
    QJSEngine* m_engine=nullptr; // borrowed, same thread only
    BinaryBridge* m_binary=nullptr; // borrowed, same worker thread
    QHash<int,Entry*> m_entries;
    int m_next=1;
    QString m_lastError;
    bool m_stopped=false;
    int create(Kind kind,const QJSValue& options);
    void watchStream(Entry* entry,QIODevice* device);
    void watchTcp(Entry* entry,QTcpSocket* socket);
    void onStreamData(int id);
    void onUdpData(int id);
    void dispatch(int id,const QString& event,const QVariantList& arguments={});
    void fail(int id,const QString& message);
    void dispatchNative(int id,const QString& event,BinaryBuffer* buffer, const QVariantMap* remote=nullptr);
    bool reject(const QString& message);
    static bool bytes(const QJSValue& value,QByteArray* out,QString* error);
    static QVariantMap opts(const QJSValue& value);
    static QString kindName(Kind kind);
};
