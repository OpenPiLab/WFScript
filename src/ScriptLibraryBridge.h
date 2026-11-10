#pragma once
#include <QObject>
#include <QJSEngine>
#include <QVariantMap>
#include <QStringList>
#include <QSet>
#include <QTemporaryDir>
#include <memory>

// Runs on the owning QJSEngine thread. Only explicit local library roots
// are searched; loaded code is trusted application code, not a sandbox.
class ScriptLibraryBridge final : public QObject {
    Q_OBJECT
public:
    ScriptLibraryBridge(QJSEngine* engine, const QString& entryFile,
                        const QVariantMap& embedded = {}, QObject* parent=nullptr);
    static QString userLibraryDirectory();
    static bool validLibraryName(const QString& name);
    static QString bootstrapSource();
    QString displayNameForPath(const QString& physicalPath) const;
    QString displayStack(const QString& stack) const;
    bool valid() const { return m_error.isEmpty(); }
    QString error() const { return m_error; }
    Q_INVOKABLE QVariantMap readInclude(const QString& path);
    // Evaluate a classic .js include as a named, global script. The JS caller
    // receives original QJSEngine exceptions with the included file/line.
    Q_INVOKABLE void includeFile(const QString& path);
    Q_INVOKABLE QJSValue loadModule(const QString& path);
    Q_INVOKABLE QString userFolder() const { return userLibraryDirectory(); }
private:
    QString resolve(const QString& path, QString* error) const;
    QJSEngine* m_engine;
    QStringList m_roots;
    QString m_error;
    std::unique_ptr<QTemporaryDir> m_embeddedDirectory;
    QSet<QString> m_includedPaths;
    QSet<QString> m_loadingPaths;
};
