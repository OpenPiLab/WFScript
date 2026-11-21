#pragma once
#include <QObject>
#include <QHash>
#include <QJSValue>
#include <QVariantMap>
#include <QJSValue>
#include <QSet>
#include <QString>
class QJSEngine;
class BinaryBridge;

// Panel-owned scripting utilities. No GUI widgets are accessed on this thread.
// Explicit user-selected paths are granted for this Panel run only.
class ScriptUtilityBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit ScriptUtilityBridge(QJSEngine* engine, const QString& projectPath, QObject* parent=nullptr);
    QString lastError() const { return m_error; }
    Q_INVOKABLE QString dataDirectory() const { return m_root; }
    Q_INVOKABLE QString dataPath(const QString& fileName) const;
    Q_INVOKABLE bool exists(const QString& path);
    Q_INVOKABLE QString readText(const QString& path);
    Q_INVOKABLE bool writeText(const QString& path, const QString& data);
    Q_INVOKABLE bool appendText(const QString& path, const QString& data);
    Q_INVOKABLE QVariant readJson(const QString& path);
    Q_INVOKABLE bool writeJson(const QString& path, const QJSValue& data);
    void setBinaryBridge(BinaryBridge* bridge) {m_binary=bridge;}
    Q_INVOKABLE QObject* readBuffer(const QString& path);
    Q_INVOKABLE bool writeBuffer(const QString& path,QObject* buffer);
    Q_INVOKABLE bool appendBuffer(const QString& path,QObject* buffer);
    Q_INVOKABLE QVariantList readBytes(const QString& path);
    Q_INVOKABLE bool writeBytes(const QString& path, const QVariantList& bytes);
    Q_INVOKABLE QVariantList readCsv(const QString& path);
    Q_INVOKABLE QJSValue readCsvArray(const QString& path); // genuine nested JS arrays
    Q_INVOKABLE bool writeCsv(const QString& path, const QVariantList& rows);
    Q_INVOKABLE int request(const QString& kind, const QVariantMap& options, const QJSValue& callback);
    void complete(int id, const QVariantMap& result);
    void shutdown();
signals:
    void dialogRequested(int id, const QString& kind, const QVariantMap& options);
    void callbackStarted();
    void callbackFinished();
    void callbackFailed(const QString& context, const QJSValue& exception);
private:
    bool permitted(const QString& path, bool write);
    QString readString(const QString& path);
    bool writeString(const QString& path, const QByteArray& data, bool append);
    QString m_root;
    QString m_error;
    QJSEngine* m_engine=nullptr;
    BinaryBridge* m_binary=nullptr; // borrowed; PanelRuntime shuts it down after utilities
    QSet<QString> m_grantedRead;
    QSet<QString> m_grantedWrite;
    QHash<int,QJSValue> m_pending;
    int m_nextId=1;
    bool m_stopped=false;
    static constexpr qint64 kMaxFileBytes=2*1024*1024;
};
