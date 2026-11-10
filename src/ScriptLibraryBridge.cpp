#include "ScriptLibraryBridge.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QUrl>

QString ScriptLibraryBridge::userLibraryDirectory() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/libraries");
    QDir().mkpath(dir);
    return QDir::cleanPath(dir);
}

bool ScriptLibraryBridge::validLibraryName(const QString& name) {
    if (name.isEmpty() || name.size() > 180 || name.contains(QLatin1Char('\\'))
        || name.startsWith(QLatin1Char('/')) || name.contains(QLatin1Char(':')))
        return false;
    const QStringList parts = name.split(QLatin1Char('/'));
    for (const QString& segment : parts) {
        if (segment.isEmpty() || segment == QStringLiteral(".") || segment == QStringLiteral("..")) return false;
        static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]*$"));
        if (!safe.match(segment).hasMatch()) return false;
    }
    return name.endsWith(QStringLiteral(".js"),Qt::CaseInsensitive)
        || name.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive);
}

ScriptLibraryBridge::ScriptLibraryBridge(QJSEngine* engine,const QString& entryFile,
                                         const QVariantMap& embedded,QObject* parent)
    : QObject(parent),m_engine(engine) {
    const QString entryDir = QFileInfo(entryFile).exists() ? QFileInfo(entryFile).absolutePath() : QString();
    if (!embedded.isEmpty()) {
        m_embeddedDirectory = std::make_unique<QTemporaryDir>();
        if (!m_embeddedDirectory->isValid()) { m_error=QStringLiteral("Cannot create temporary panel library directory");return; }
        for (auto it=embedded.cbegin();it!=embedded.cend();++it) {
            if (!validLibraryName(it.key()) || it.value().toString().toUtf8().size()>1024*1024) {
                m_error=QStringLiteral("Invalid embedded library: %1").arg(it.key()); return;
            }
            const QString path=m_embeddedDirectory->path()+QLatin1Char('/')+it.key();
            if (!QDir().mkpath(QFileInfo(path).absolutePath())) {m_error=QStringLiteral("Cannot create embedded library directory");return;}
            QSaveFile f(path);
            const QByteArray bytes=it.value().toString().toUtf8();
            if(!f.open(QIODevice::WriteOnly) || f.write(bytes)!=bytes.size() || !f.commit()) {
                m_error=QStringLiteral("Cannot write embedded library %1").arg(it.key());return;
            }
        }
        m_roots << m_embeddedDirectory->path();
    }
    if (!entryDir.isEmpty()) { m_roots << entryDir+QStringLiteral("/libraries") << entryDir; }
    m_roots << userLibraryDirectory() << QCoreApplication::applicationDirPath()+QStringLiteral("/libraries");
}

QString ScriptLibraryBridge::displayNameForPath(const QString& physicalPath) const {
    const QString actual=QFileInfo(physicalPath).canonicalFilePath();
    if(actual.isEmpty()) return physicalPath;
    if (m_embeddedDirectory) {
        const QString root=QFileInfo(m_embeddedDirectory->path()).canonicalFilePath();
        if(!root.isEmpty() && actual.startsWith(root+QLatin1Char('/'),
#ifdef Q_OS_WIN
           Qt::CaseInsensitive
#else
           Qt::CaseSensitive
#endif
        )) return QDir(root).relativeFilePath(actual).replace(QLatin1Char('\\'),QLatin1Char('/'));
    }
    return actual; // Filesystem library: keep the full path for navigation.
}

QString ScriptLibraryBridge::displayStack(const QString& stack) const {
    if (!m_embeddedDirectory || stack.isEmpty()) return stack;
    // Only replace actual file URLs that point inside this panel's temporary
    // embedded-library directory. Other sources and physical library paths
    // remain unmodified for navigation and diagnostics.
    static const QRegularExpression uri(QStringLiteral(R"(file:///[^\s\)\n]+)"));
    QString display=stack;
    QVector<QPair<int,int>> positions;
    QStringList substitutions;
    auto matches=uri.globalMatch(stack);
    while(matches.hasNext()) {
        const auto m=matches.next();
        const QString raw=m.captured();
        const QRegularExpression suffix(QStringLiteral(R"((:\d+(?::\d+)?)$)"));
        const auto s=suffix.match(raw);
        const QString positionSuffix=s.hasMatch()?s.captured(1):QString();
        const QString url=raw.left(raw.size()-positionSuffix.size());
        const QString physical=QUrl(url).toLocalFile();
        if(physical.isEmpty())continue;
        const QString named=displayNameForPath(physical);
        if(named==QFileInfo(physical).canonicalFilePath())continue;
        positions.append({m.capturedStart(),m.capturedLength()});
        substitutions.append(named+positionSuffix);
    }
    for(int i=positions.size()-1;i>=0;--i)
        display.replace(positions[i].first,positions[i].second,substitutions[i]);
    return display;
}

QString ScriptLibraryBridge::resolve(const QString& path,QString* error) const {
    QString libraryName=path;
    while (libraryName.startsWith(QStringLiteral("./"))) libraryName.remove(0,2);
    if (!validLibraryName(libraryName)) {
        *error=QStringLiteral("Library name must be a relative .js or .mjs path without '..' or absolute paths: %1").arg(path);
        return {};
    }
    for (const QString& root: m_roots) {
        const QString base=QFileInfo(root).canonicalFilePath();
        if (base.isEmpty()) continue;
        const QFileInfo candidate(QDir(root).filePath(libraryName));
        const QString actual=candidate.canonicalFilePath();
        if (!candidate.isFile() || actual.isEmpty()) continue;
        // Canonical paths also prevent symlinks escaping an allowed root.
        if (!actual.startsWith(base+QLatin1Char('/'),
#ifdef Q_OS_WIN
                Qt::CaseInsensitive
#else
                Qt::CaseSensitive
#endif
            )) continue;
        if (candidate.size()>1024*1024) { *error=QStringLiteral("Library exceeds 1 MiB: %1").arg(path); return {}; }
        return actual;
    }
    *error=QStringLiteral("Library not found: %1 (search: %2)").arg(path,m_roots.join(QStringLiteral("; ")));
    return {};
}

QVariantMap ScriptLibraryBridge::readInclude(const QString& name) {
    QString error;
    if (!name.endsWith(QStringLiteral(".js"),Qt::CaseInsensitive))
        return {{"ok",false},{"error",QStringLiteral("include() requires a .js file; use modules.load() for .mjs")}};
    const QString path=resolve(name,&error);
    if (path.isEmpty()) return {{"ok",false},{"error",error}};
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {{"ok",false},{"error",f.errorString()}};
    return {{"ok",true},{"source",QString::fromUtf8(f.readAll())},{"path",path}};
}

void ScriptLibraryBridge::includeFile(const QString& name) {
    // Run classic libraries through Qt's named script evaluator, NOT JavaScript
    // indirect eval(). QJSEngine does not reliably honor //# sourceURL, and
    // indirect eval gives stack frames such as "eval code:2".
    // evaluate() runs against the engine's global object, preserving classic
    // var/function declarations (including those referenced later by events).
    if (!name.endsWith(QStringLiteral(".js"), Qt::CaseInsensitive)) {
        m_engine->throwError(QStringLiteral("include() requires a .js file; use modules.load() for .mjs"));
        return;
    }
    QString error;
    const QString path=resolve(name,&error);
    if (path.isEmpty()) { m_engine->throwError(error); return; }
    if (m_includedPaths.contains(path)) return;
    if (m_loadingPaths.contains(path)) {
        m_engine->throwError(QStringLiteral("Circular include: %1").arg(name));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_engine->throwError(QStringLiteral("Cannot open library %1: %2").arg(name,file.errorString()));
        return;
    }
    const QString source=QString::fromUtf8(file.readAll());
    m_loadingPaths.insert(path);
    QStringList frames;
    // The physical path is intentional: Qt records it in deferred functions'
    // source locations; displayNameForPath maps embedded temporary files back
    // to their logical names for the Problems panel and the editor.
    const QJSValue result=m_engine->evaluate(source,path,1,&frames);
    m_loadingPaths.remove(path);
    if (result.isError() || !frames.isEmpty()) {
        // Preserve the exception object, filename, line, and stack. It must be
        // rethrown to abort main script initialization, exactly like include()
        // previously did. Catching errors in a library remains possible in JS.
        if (result.isError()) {
            m_engine->throwError(result);
        } else {
            QJSValue exception=m_engine->newErrorObject(QJSValue::GenericError,result.toString());
            exception.setProperty(QStringLiteral("fileName"),path);
            m_engine->throwError(exception);
        }
        return;
    }
    m_includedPaths.insert(path);
}

QJSValue ScriptLibraryBridge::loadModule(const QString& name) {
    QString error;
    if (!name.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive))
        { m_engine->throwError(QStringLiteral("modules.load() requires .mjs: %1").arg(name)); return QJSValue(); }
    const QString path=resolve(name,&error);
    if (path.isEmpty()) { m_engine->throwError(error); return QJSValue(); }
    return m_engine->importModule(path);
}

QString ScriptLibraryBridge::bootstrapSource() {
    return QStringLiteral(R"JS(
        // Classic .js libraries are evaluated in the engine's global scope by
        // the C++ bridge with the actual filename. This makes both startup and
        // delayed callback exceptions navigable to the correct source file.
        var include = function(name) { __wfLibraryHost.includeFile(name); };
        var modules = {
            load: function(name) { return __wfLibraryHost.loadModule(name); },
            userFolder: function() { return __wfLibraryHost.userFolder(); }
        };
    )JS");
}
