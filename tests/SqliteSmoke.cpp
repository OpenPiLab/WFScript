#include "SqliteBridge.h"
#include "ScriptValueConversion.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QDebug>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir directory;
    if(!directory.isValid()){qCritical()<<"Temporary database directory unavailable";return 1;}
    QJSEngine engine;
    // The SQL result boundary must distinguish NULL from the valid empty
    // string before callbacks/worker threads are involved.
    const QVariant nativeNull=QVariant::fromValue(nullptr);
    const QJSValue jsNull=ScriptValueConversion::toJavaScript(&engine,nativeNull);
    const QJSValue jsEmpty=ScriptValueConversion::toJavaScript(&engine,QVariant(QStringLiteral("")));
    if(!nativeNull.isValid() || !nativeNull.isNull() || !jsNull.isNull() ||
       !jsEmpty.isString() || !jsEmpty.toString().isEmpty()) {
        qCritical()<<"SQLite NULL carrier or empty-string conversion incorrect";
        return 2;
    }
    SqliteBridge service(&engine,directory.path());
    QJSEngine::setObjectOwnership(&service,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("__databaseHost"),engine.newQObject(&service));
    const QJSValue bootstrap=engine.evaluate(SqliteBridge::bootstrapSource(),QStringLiteral("<database-bootstrap>"));
    if(bootstrap.isError()){qCritical()<<"Bootstrap:"<<bootstrap.toString();return 2;}
    const char* js=R"JS(
        var done=false, failures=[], completed=0, dbHandle=0;
        function check(ok,msg){if(!ok) failures.push(msg);}
        Database.open('SmokeDB.db',function(open) {
            check(open.ok,'open: '+open.error); if(!open.ok){done=true;return;}
            dbHandle=open.db;
            Database.exec(dbHandle,'CREATE TABLE IF NOT EXISTS samples (id INTEGER PRIMARY KEY, note TEXT, value REAL)',[],function(created){
                check(created.ok,'create: '+created.error);
                Database.exec(dbHandle,'INSERT INTO samples(note,value) VALUES(?,?)',["A','; DROP TABLE samples; --",3.301],function(inserted){
                    check(inserted.ok,'insert: '+inserted.error);
                    Database.query(dbHandle,'SELECT note,value FROM samples WHERE value>?',[3],function(rows){
                        check(rows.ok,'query: '+rows.error);
                        check(Array.isArray(rows.rows) && rows.rows.length===1 && Array.isArray(rows.rows[0]),'rows not nested JS arrays');
                        check(rows.rows[0][0]==="A','; DROP TABLE samples; --",'SQL parameter content damaged');
                        check(Math.abs(rows.rows[0][1]-3.301)<1e-9,'number changed');
                        Database.query(dbHandle,'SELECT count(*) FROM samples',function(cnt){
                            check(cnt.ok && cnt.rows[0][0]===1,'table not intact');
                            Database.exec(dbHandle,"ATTACH DATABASE '/tmp/evil.db' AS evil",[],function(rejected){
                                check(!rejected.ok && rejected.error.indexOf('supports')>=0,'unsafe ATTACH allowed');
                                Database.exec(dbHandle,'INSERT INTO samples(note,value) VALUES(?,?)',[null,2.5],function(next){
                                    check(next.ok,'insert null: '+next.error);
                                    Database.query(dbHandle,'SELECT note,value FROM samples WHERE note IS NULL',[],function(nulls){
                                        var observed=(nulls.ok && nulls.rows && nulls.rows.length && nulls.rows[0])
                                            ? nulls.rows[0][0] : undefined;
                                        check(nulls.ok && nulls.rows.length===1 && observed===null,
                                            'SQL NULL round trip: observed '+String(observed)+
                                            ' (typeof '+typeof observed+', queryOk '+String(nulls.ok)+
                                            ', rowCount '+String(nulls.rowCount)+')');
                                        Database.close(dbHandle,function(closed){
                                            check(closed.ok,'close: '+closed.error);
                                            Database.open('SmokeDB.db',function(reopened){
                                                check(reopened.ok,'reopen persistent DB');
                                                if(!reopened.ok){done=true;return;}
                                                Database.query(reopened.db,'SELECT COUNT(*) FROM samples',[],function(count){
                                                    check(count.ok && count.rows[0][0]===2,'disk persistence: '+count.error);
                                                    Database.close(reopened.db,function(c){check(c.ok,'final close');done=true;});
                                                });
                                            });
                                        });
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    )JS";
    const QJSValue result=engine.evaluate(QString::fromUtf8(js),QStringLiteral("SQLiteSmoke.js"));
    if(result.isError()){qCritical()<<"JavaScript test:"<<result.toString();return 3;}
    QElapsedTimer elapsed;elapsed.start();
    while(!engine.globalObject().property(QStringLiteral("done")).toBool() && elapsed.elapsed()<8500){
        QCoreApplication::processEvents(QEventLoop::AllEvents,20);
        QThread::msleep(2);
    }
    if(!engine.globalObject().property(QStringLiteral("done")).toBool()) {qCritical()<<"Timeout awaiting asynchronous SQLite callbacks";return 4;}
    const QJSValue failures=engine.globalObject().property(QStringLiteral("failures"));
    if(failures.property(QStringLiteral("length")).toInt()!=0) {
        qCritical()<<"SQLite errors:"<<engine.evaluate(QStringLiteral("JSON.stringify(failures)")).toString();return 5;
    }
    // Request immediately before shutdown. Its callback must never run again.
    engine.evaluate(QStringLiteral("var lateCallback=false; Database.open('Late.db',function(){lateCallback=true;});"));
    service.shutdown();
    QCoreApplication::processEvents();
    if(engine.globalObject().property(QStringLiteral("lateCallback")).toBool()) {qCritical()<<"Callback fired after stop";return 6;}
    qInfo()<<"PASS: async SQLite open, prepared binding, nested JS arrays, NULL, blocked ATTACH, persistence, close, shutdown";
    return 0;
}
