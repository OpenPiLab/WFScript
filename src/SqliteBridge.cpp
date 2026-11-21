#include "SqliteBridge.h"
#include "ScriptValueConversion.h"
#include <QJSEngine>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlDriver>
#include <QRegularExpression>
#include <QStringList>
#include <QByteArray>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QMetaObject>
#include <QThread>
#include <QDateTime>
#include <utility>

namespace {
constexpr int kMaxConnections=4;
constexpr int kMaxParams=64;
constexpr int kMaxRows=250;
constexpr int kMaxColumns=16;
constexpr int kMaxCellChars=4096;
constexpr int kMaxResultChars=512*1024;
constexpr int kMaxSqlChars=8192;
QVariantMap failed(const QString& action,const QString& why) {
    return {{QStringLiteral("ok"),false},{QStringLiteral("operation"),action},{QStringLiteral("error"),why}};
}
// Conservative v1 SQL subset: deliberately NO ATTACH/DETACH/VACUUM/PRAGMA,
// triggers, multiple statements or arbitrary paths. Quoted strings and SQL
// parameters may contain semicolons or the literal text of forbidden words.
bool validateSql(const QString& sql,const QString& action,QString* why) {
    if(sql.trimmed().isEmpty() || sql.size()>kMaxSqlChars) {*why=QStringLiteral("SQL length must be 1..8192 characters");return false;}
    QStringList tokens;QString word;QChar quoted;bool ended=false;
    auto flush=[&]{if(!word.isEmpty()){tokens.append(word.toUpper());word.clear();}};
    for(int i=0;i<sql.size();++i) {
        const QChar c=sql.at(i);
        if(!quoted.isNull()) {
            if((quoted==QLatin1Char('[') && c==QLatin1Char(']')) || c==quoted) {
                if(i+1<sql.size() && sql.at(i+1)==c && quoted!=QLatin1Char('[')){++i;continue;}
                quoted=QChar();
            }
            continue;
        }
        if(c==QLatin1Char('\'') || c==QLatin1Char('"') || c==QLatin1Char('`') || c==QLatin1Char('[')) {
            flush();quoted=c;continue;
        }
        if(i+1<sql.size() && c==QLatin1Char('-') && sql.at(i+1)==QLatin1Char('-')){*why=QStringLiteral("SQL comments are disabled");return false;}
        if(i+1<sql.size() && c==QLatin1Char('/') && sql.at(i+1)==QLatin1Char('*')){*why=QStringLiteral("SQL comments are disabled");return false;}
        if(c==QLatin1Char(';')){flush();ended=true;continue;}
        if(ended && !c.isSpace()){*why=QStringLiteral("Only one SQL statement is permitted");return false;}
        if(c.isLetterOrNumber() || c==QLatin1Char('_'))word+=c;
        else flush();
    }
    flush();
    if(!quoted.isNull()){*why=QStringLiteral("Unterminated SQL string or identifier");return false;}
    if(tokens.isEmpty()){*why=QStringLiteral("Missing SQL command");return false;}
    const QString first=tokens.first();
    if(action==QStringLiteral("query")) {
        if(first!=QStringLiteral("SELECT")){*why=QStringLiteral("Database.query supports SELECT only");return false;}
    } else {
        const bool standard=(first==QStringLiteral("INSERT") || first==QStringLiteral("UPDATE") || first==QStringLiteral("DELETE"));
        const bool schema=((first==QStringLiteral("CREATE") || first==QStringLiteral("DROP") || first==QStringLiteral("ALTER")) && tokens.size()>1 &&
                           (tokens.at(1)==QStringLiteral("TABLE") || tokens.at(1)==QStringLiteral("INDEX") ||
                            (first==QStringLiteral("CREATE") && tokens.at(1)==QStringLiteral("UNIQUE") && tokens.size()>2 && tokens.at(2)==QStringLiteral("INDEX"))));
        if(!standard && !schema){*why=QStringLiteral("Database.exec supports INSERT/UPDATE/DELETE and ordinary TABLE/INDEX schema statements");return false;}
    }
    const QStringList denied={QStringLiteral("ATTACH"),QStringLiteral("DETACH"),QStringLiteral("VACUUM"),
        QStringLiteral("PRAGMA"),QStringLiteral("TRIGGER"),QStringLiteral("VIRTUAL"),
        QStringLiteral("LOAD_EXTENSION"),QStringLiteral("TEMP"),QStringLiteral("TEMPORARY")};
    for(const QString& token:tokens)if(denied.contains(token)){*why=QStringLiteral("Restricted SQL keyword: ")+token;return false;}
    return true;
}
}

SqliteWorker::SqliteWorker(QString directory):m_directory(std::move(directory)){}
QVariantMap SqliteWorker::openDatabase(const QString& name) {
    const QString action=QStringLiteral("open");
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z][A-Za-z0-9_-]{0,62}(?:\\.(?:db|sqlite))?$"));
    if(!valid.match(name).hasMatch())return failed(action,QStringLiteral("Database name must be a simple ASCII filename (letters, digits, _ or -, optional .db/.sqlite); no paths"));
    if(m_connections.size()>=kMaxConnections)return failed(action,QStringLiteral("At most four databases per Panel"));
    if(!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))return failed(action,QStringLiteral("Qt QSQLITE driver unavailable; check Qt SQL plugin deployment"));
    const QString filename=name.contains(QLatin1Char('.'))?name:name+QStringLiteral(".db");
    const QString file=QDir(m_directory).absoluteFilePath(filename);
    if(QFileInfo(file).isSymLink())return failed(action,QStringLiteral("Symbolic-link databases are not allowed"));
    if(!QDir(m_directory).exists())return failed(action,QStringLiteral("Panel data directory unavailable"));
    const int handle=m_nextHandle++;
    const QString connectionName=QStringLiteral("wfscript-panel-db-%1-%2").arg(handle).arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString error;
    {
        QSqlDatabase db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),connectionName);
        db.setDatabaseName(file);
        db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=750;QSQLITE_OPEN_NOFOLLOW"));
        if(!db.open())error=db.lastError().text();
        else {
            QSqlQuery pragmas(db);
            if(!pragmas.exec(QStringLiteral("PRAGMA foreign_keys=ON")))error=pragmas.lastError().text();
            if(error.isEmpty() && !pragmas.exec(QStringLiteral("PRAGMA journal_mode=WAL")))error=pragmas.lastError().text();
        }
        if(!error.isEmpty())db.close();
    }
    if(!error.isEmpty()){QSqlDatabase::removeDatabase(connectionName);return failed(action,error);}
    m_connections.insert(handle,connectionName);
    return {{QStringLiteral("ok"),true},{QStringLiteral("operation"),action},{QStringLiteral("db"),handle},
            {QStringLiteral("path"),file}};
}
QVariantMap SqliteWorker::runStatement(const QString& action,int handle,const QString& sql,const QVariantList& parameters) {
    const QString connectionName=m_connections.value(handle);
    if(connectionName.isEmpty())return failed(action,QStringLiteral("Invalid or closed database handle"));
    QString why;
    if(!validateSql(sql,action,&why))return failed(action,why);
    if(parameters.size()>kMaxParams)return failed(action,QStringLiteral("At most 64 SQL parameters"));
    for(const QVariant& p:parameters) {
        const int t=p.metaType().id();
        if(p.isValid() && !p.isNull() && t!=QMetaType::QString && t!=QMetaType::Int &&
           t!=QMetaType::UInt && t!=QMetaType::LongLong && t!=QMetaType::Bool &&
           t!=QMetaType::Double && t!=QMetaType::Float)
            return failed(action,QStringLiteral("SQL parameters must be string, number, boolean, or null"));
    }
    QSqlDatabase db=QSqlDatabase::database(connectionName,false);
    if(!db.isValid() || !db.isOpen())return failed(action,QStringLiteral("Database connection no longer open"));
    QSqlQuery q(db);
    q.setForwardOnly(true);
    if(!q.prepare(sql))return failed(action,q.lastError().text());
    for(const QVariant& p:parameters)q.addBindValue(p);
    if(!q.exec())return failed(action,q.lastError().text());
    if(action==QStringLiteral("exec")) {
        return {{QStringLiteral("ok"),true},{QStringLiteral("operation"),action},
                {QStringLiteral("changes"),q.numRowsAffected()},{QStringLiteral("insertId"),q.lastInsertId()},
                {QStringLiteral("db"),handle}};
    }
    const QSqlRecord fields=q.record();
    if(fields.count()>kMaxColumns)return failed(action,QStringLiteral("Query result exceeds 16 columns"));
    QStringList columns;columns.reserve(fields.count());
    for(int col=0;col<fields.count();++col)columns.append(fields.fieldName(col));
    QVariantList rows;int chars=0;
    while(q.next()) {
        if(rows.size()>=kMaxRows)return failed(action,QStringLiteral("Query exceeds 250 rows; use WHERE and LIMIT"));
        QVariantList row;
        for(int col=0;col<columns.size();++col) {
            QVariant cell=q.value(col);
            if(!cell.isNull()) {
                // Stable JSON-compatible surface; convert BLOBs to reject rather than
                // exposing binary data or unknown QVariant wrapper objects to QJSEngine.
                if(cell.metaType().id()==QMetaType::QByteArray)return failed(action,QStringLiteral("BLOB results are not supported by this API"));
                const QString asText=cell.toString();
                chars+=asText.size();
                if(asText.size()>kMaxCellChars || chars>kMaxResultChars)return failed(action,QStringLiteral("Query output exceeds size limit"));
                if(cell.metaType().id()==QMetaType::LongLong || cell.metaType().id()==QMetaType::ULongLong) {
                    // JS doubles cannot exactly represent integers >2^53; retain
                    // oversized database integers as decimal strings, not rounded.
                    const qlonglong n=cell.toLongLong();
                    if(n>9007199254740991LL || n<-9007199254740991LL)cell=asText;
                }
            } else {
                // Keep SQL NULL distinct from an empty QString, which is a
                // valid user string. A std::nullptr_t QVariant is both valid
                // and null in Qt 6, and ScriptValueConversion turns it into
                // JavaScript null explicitly (without QByteArray wrappers).
                cell=QVariant::fromValue(nullptr);
            }
            row.append(cell);
        }
        rows.append(QVariant(row));
    }
    if(q.lastError().isValid())return failed(action,q.lastError().text());
    return {{QStringLiteral("ok"),true},{QStringLiteral("operation"),action},
            {QStringLiteral("db"),handle},{QStringLiteral("columns"),columns},
            {QStringLiteral("rows"),rows},{QStringLiteral("rowCount"),rows.size()}};
}
QVariantMap SqliteWorker::closeDatabase(int handle) {
    const QString action=QStringLiteral("close");
    const QString name=m_connections.take(handle);
    if(name.isEmpty())return failed(action,QStringLiteral("Invalid or already closed database handle"));
    {QSqlDatabase db=QSqlDatabase::database(name,false);if(db.isValid())db.close();}
    QSqlDatabase::removeDatabase(name);
    return {{QStringLiteral("ok"),true},{QStringLiteral("operation"),action},{QStringLiteral("db"),handle}};
}
void SqliteWorker::execute(int requestId,const QString& action,const QVariantMap& args) {
    QVariantMap result;
    if(m_stopped)result=failed(action,QStringLiteral("Database service stopped"));
    else if(action==QStringLiteral("open"))result=openDatabase(args.value(QStringLiteral("name")).toString());
    else if(action==QStringLiteral("close"))result=closeDatabase(args.value(QStringLiteral("db")).toInt());
    else if(action==QStringLiteral("exec") || action==QStringLiteral("query"))
        result=runStatement(action,args.value(QStringLiteral("db")).toInt(),args.value(QStringLiteral("sql")).toString(),args.value(QStringLiteral("params")).toList());
    else result=failed(action,QStringLiteral("Unsupported database operation"));
    emit completed(requestId,result);
}
void SqliteWorker::shutdown() {
    if(m_stopped)return;
    m_stopped=true;
    const QList<int> handles=m_connections.keys();
    for(int handle:handles)closeDatabase(handle);
}

SqliteBridge::SqliteBridge(QJSEngine* engine,const QString& privateDirectory,QObject* parent)
    :QObject(parent),m_engine(engine),m_directory(privateDirectory) {}
void SqliteBridge::startWorker() {
    if(m_worker || m_stopped)return;
    // Zero SQLite threads, DB connections or disk access until the script
    // makes its first explicit Database.* request.
    m_worker=new SqliteWorker(m_directory);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread,&QThread::finished,m_worker,&QObject::deleteLater);
    connect(this,&SqliteBridge::requested,m_worker,&SqliteWorker::execute,Qt::QueuedConnection);
    connect(m_worker,&SqliteWorker::completed,this,&SqliteBridge::complete,Qt::QueuedConnection);
    m_thread.setObjectName(QStringLiteral("WFScript SQLite worker"));
    m_thread.start();
}
SqliteBridge::~SqliteBridge(){shutdown();}
int SqliteBridge::submit(const QString& action,const QVariantMap& args,const QJSValue& cb) {
    m_error.clear();
    if(m_stopped)m_error=QStringLiteral("Database service stopped");
    else if(!cb.isCallable())m_error=QStringLiteral("Database operations require a callback function");
    else if(m_pending.size()>=64)m_error=QStringLiteral("At most 64 pending database operations");
    if(!m_error.isEmpty())return 0;
    startWorker();
    if(!m_thread.isRunning()){m_error=QStringLiteral("SQLite worker could not start");return 0;}
    const int id=m_nextRequest++;
    m_pending.insert(id,cb);
    emit requested(id,action,args);
    return id;
}
int SqliteBridge::open(const QString& name,const QJSValue& cb) {
    return submit(QStringLiteral("open"),{{QStringLiteral("name"),name}},cb);
}
int SqliteBridge::close(int handle,const QJSValue& cb) {
    return submit(QStringLiteral("close"),{{QStringLiteral("db"),handle}},cb);
}
QVariantList SqliteBridge::sqlParameters(const QJSValue& params) {
    QVariantList result;
    const int count=params.property(QStringLiteral("length")).toInt();
    result.reserve(qMin(count,65));
    for(int i=0;i<count && i<=64;++i) {
        const QJSValue item=params.property(quint32(i));
        // Qt SQL documents a typed null QVariant (of the target SQL type)
        // for binding NULL. A null QByteArray can bind as a BLOB on some
        // SQLite driver versions; a typed null QString binds as SQL NULL.
        result.append(item.isNull()?QVariant(QMetaType::fromType<QString>()):
                      item.toVariant(QJSValue::ConvertJSObjects));
    }
    return result;
}
int SqliteBridge::exec(int handle,const QString& sql,const QJSValue& params,const QJSValue& cb) {
    if(!params.isArray()){m_error=QStringLiteral("SQL parameters must be a JavaScript array");return 0;}
    return submit(QStringLiteral("exec"),{{QStringLiteral("db"),handle},{QStringLiteral("sql"),sql},
                   {QStringLiteral("params"),sqlParameters(params)}},cb);
}
int SqliteBridge::query(int handle,const QString& sql,const QJSValue& params,const QJSValue& cb) {
    if(!params.isArray()){m_error=QStringLiteral("SQL parameters must be a JavaScript array");return 0;}
    return submit(QStringLiteral("query"),{{QStringLiteral("db"),handle},{QStringLiteral("sql"),sql},
                   {QStringLiteral("params"),sqlParameters(params)}},cb);
}
void SqliteBridge::complete(int requestId,const QVariantMap& result) {
    auto it=m_pending.find(requestId);
    if(m_stopped || !m_engine || it==m_pending.end())return;
    QJSValue callback=it.value();m_pending.erase(it);
    QVariantMap reply=result;reply.insert(QStringLiteral("requestId"),requestId);
    emit callbackStarted();
    const QJSValue outcome=callback.call({ScriptValueConversion::toJavaScript(m_engine,reply)});
    emit callbackFinished();
    if(outcome.isError())emit callbackFailed(QStringLiteral("SQLite ")+result.value(QStringLiteral("operation")).toString()+QStringLiteral(" callback"),outcome);
}
void SqliteBridge::shutdown() {
    if(m_stopped)return;
    m_stopped=true;
    m_pending.clear();
    if(m_thread.isRunning()) {
        // Finish queued SQL operations before closing connections, on the SQL
        // thread. The QJSEngine is never accessed from that thread.
        QMetaObject::invokeMethod(m_worker,"shutdown",Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
    m_worker=nullptr;
}
QString SqliteBridge::bootstrapSource() {
    return QStringLiteral(R"JS(
        var Database = {
            open: function(name,callback) {
                var id=__databaseHost.open(String(name),callback);
                if(!id)throw Error(__databaseHost.lastError);
                return id;
            },
            exec: function(db,sql,params,callback) {
                if(typeof params==='function'){callback=params;params=[];}
                var id=__databaseHost.exec(db,String(sql),params,callback);
                if(!id)throw Error(__databaseHost.lastError);
                return id;
            },
            query: function(db,sql,params,callback) {
                if(typeof params==='function'){callback=params;params=[];}
                var id=__databaseHost.query(db,String(sql),params,callback);
                if(!id)throw Error(__databaseHost.lastError);
                return id;
            },
            close: function(db,callback) {
                var id=__databaseHost.close(db,callback);
                if(!id)throw Error(__databaseHost.lastError);
                return id;
            }
        };
    )JS");
}
