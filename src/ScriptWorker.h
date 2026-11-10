#pragma once
#include <QObject>
#include <QJSEngine>
#include <QMutex>
#include <atomic>
#include "ScopeFrame.h"

class ConsoleBridge final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE void write(const QString& text) { emit printed(text); }
    Q_INVOKABLE void clear() { emit clearRequested(); }
signals:
    void printed(const QString& text);
    void clearRequested();
};

class ScriptWorker final : public QObject {
    Q_OBJECT
public:
    explicit ScriptWorker(QObject* parent = nullptr);
    void requestStop();
    void execute(const QString& source, const QString& fileName,
                 bool physicalDevice, int deviceIndex, int configIndex);
signals:
    void outputReady(const QString& text);
    void consoleClearRequested();
    void scopeFrameReady(const ScopeFrame& frame);
    void scriptError(int oneBasedLine, const QString& message); // legacy signal
    void scriptDiagnostic(const QString& source, int line, int column,
                          const QString& message, const QString& stack);
    void finished(bool success, const QString& summary);
private:
    QMutex m_mutex;
    QJSEngine* m_activeEngine = nullptr; // guarded by mutex
    std::atomic_bool m_stopRequested{false};
};
