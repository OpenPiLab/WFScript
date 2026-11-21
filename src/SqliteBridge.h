#pragma once
// Optional, Panel-owned SQLite service. All QSqlDatabase operations occur on the
// database thread; QJSValue and callbacks remain on the PanelRuntime thread.
#include <QObject>
#include <QHash>
#include <QJSValue>
#include <QThread>
#include <QVariantMap>
#include <QVariantList>

class QJSEngine;

class SqliteWorker final : public QObject {
    Q_OBJECT
public:
    explicit SqliteWorker(QString directory);
public slots:
    void execute(int requestId, const QString& action, const QVariantMap& args);
    void shutdown();
signals:
    void completed(int requestId, const QVariantMap& result);
private:
    QString m_directory;
    QHash<int,QString> m_connections;
    int m_nextHandle=1;
    bool m_stopped=false;
    QVariantMap openDatabase(const QString& name);
    QVariantMap runStatement(const QString& action,int handle,const QString& sql,const QVariantList& parameters);
    QVariantMap closeDatabase(int handle);
};

class SqliteBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit SqliteBridge(QJSEngine* engine,const QString& privateDirectory,QObject* parent=nullptr);
    ~SqliteBridge() override;
    QString lastError() const {return m_error;}
    Q_INVOKABLE int open(const QString& name,const QJSValue& callback);
    Q_INVOKABLE int exec(int db,const QString& sql,const QJSValue& params,const QJSValue& callback);
    Q_INVOKABLE int query(int db,const QString& sql,const QJSValue& params,const QJSValue& callback);
    Q_INVOKABLE int close(int db,const QJSValue& callback);
    void shutdown();
    static QString bootstrapSource();
signals:
    void requested(int requestId,const QString& action,const QVariantMap& args);
    void callbackStarted();
    void callbackFinished();
    void callbackFailed(const QString& context,const QJSValue& error);
private slots:
    void complete(int requestId,const QVariantMap& result);
private:
    static QVariantList sqlParameters(const QJSValue& params);
    int submit(const QString& action,const QVariantMap& args,const QJSValue& cb);
    void startWorker();
    QJSEngine* m_engine=nullptr;
    QThread m_thread;
    QString m_directory;
    SqliteWorker* m_worker=nullptr;
    QHash<int,QJSValue> m_pending;
    int m_nextRequest=1;
    bool m_stopped=false;
    QString m_error;
};
