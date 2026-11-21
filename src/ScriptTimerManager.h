#pragma once

#include <QObject>
#include <QHash>
#include <QJSValue>
#include <QString>

class QJSEngine;
class QTimer;

// Panel-scoped, event-loop timers. The manager and QJSEngine live on the SAME
// PanelRuntime thread. Callbacks never execute on the GUI thread.
class ScriptTimerManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
public:
    explicit ScriptTimerManager(QJSEngine* engine, QObject* parent = nullptr);
    ~ScriptTimerManager() override;

    Q_INVOKABLE int schedule(const QJSValue& callback, double milliseconds, bool repeating);
    Q_INVOKABLE void cancel(int id);
    Q_INVOKABLE int activeCount() const;
    QString lastError() const { return m_lastError; }
    static QString bootstrapSource(); // exact JS API installed by PanelRuntime and tested natively

    void shutdown(); // permanently disable, cancel and release all callbacks

signals:
    void callbackStarted();
    void callbackFinished();
    void callbackFailed(const QString& context, const QJSValue& exception);

private:
    struct Entry { QTimer* timer = nullptr; QJSValue callback; bool repeating = false; };
    void fire(int id);
    QJSEngine* m_engine; // borrowed; must outlive this manager
    QHash<int, Entry> m_entries;
    int m_nextId = 1;
    bool m_stopped = false;
    QString m_lastError;

    static constexpr int kMaxActive = 256;
    static constexpr int kMinDelayMs = 10;
    static constexpr int kMaxDelayMs = 86'400'000;
};
