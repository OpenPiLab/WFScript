// Native Qt QJSEngine + QTimer smoke for the exact Panel timer bootstrap.
// Simulation-only, no QWidget or Digilent SDK handle.
#include "ScriptTimerManager.h"
#include "CompletionCatalog.h"
#include "ApiKnowledge.h"
#include "EditorAssist.h"
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QJSEngine>
#include <QTimer>
#include <QJSValue>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int errors = 0;
    auto check = [&errors](bool ok, const char* name) {
        if(!ok) { qCritical() << "FAIL:" << name; ++errors; }
    };
    QJSEngine engine;
    ScriptTimerManager manager(&engine);
    QJSEngine::setObjectOwnership(&manager, QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("__timerHost"), engine.newQObject(&manager));
    QJSValue boot=engine.evaluate(ScriptTimerManager::bootstrapSource(), QStringLiteral("<timer-bootstrap>"));
    check(!boot.isError(), "bootstrap syntax");
    auto eval=[&engine](const QString& js){return engine.evaluate(js,QStringLiteral("TimerSmoke.js"));};
    check(!eval(QStringLiteral(R"JS(
        var ticks=0, once=0, arg='';
        var every=setInterval(function(){ ++ticks; },10);
        var first=setTimeout(function(a,b){ once++; arg=a+b; },35, 'hello',' world');
        var cancelled=setTimeout(function(){ once=999; },40);
        clearTimeout(cancelled);
        clearInterval(0); clearTimeout(0);
    )JS")).isError(), "initial timer registration");
    check(manager.activeCount()==2, "cancelled one-shot removed");
    QEventLoop loop;
    QTimer::singleShot(105,&loop,&QEventLoop::quit);
    loop.exec();
    check(eval(QStringLiteral("ticks >= 3 && once === 1 && arg === 'hello world' && typeof every==='number'")).toBool(),"interval and once with args");
    check(manager.activeCount()==1,"one-shot removed after firing");
    check(!eval(QStringLiteral("clearInterval(every);clearTimeout(first);")).isError(),"clear after fire");
    check(manager.activeCount()==0,"cleared interval");

    const auto invalid=eval(QStringLiteral(R"JS(
        (function(){
          const cases=[()=>setInterval(3,20),()=>setTimeout(()=>{},-1),
                       ()=>setTimeout(()=>{},0),()=>setInterval(()=>{},2.5),
                       ()=>setTimeout(()=>{},Infinity),()=>setTimeout(()=>{},86400001)];
          return cases.every(function(fn){try{fn();return false;}catch(e){return e instanceof Error;}});
        })()
    )JS"));
    check(!invalid.isError() && invalid.toBool(),"invalid parameters throw");

    check(!eval(QStringLiteral("var id=setInterval(function(){clearInterval(id);},10); ")).isError(),"self-clear setup");
    QEventLoop second;
    QTimer::singleShot(40,&second,&QEventLoop::quit);
    second.exec();
    check(manager.activeCount()==0,"self-clearing interval");

    int failures=0;
    QObject::connect(&manager,&ScriptTimerManager::callbackFailed,&app,
        [&failures](const QString& context,const QJSValue& error){
            if(context.contains(QStringLiteral("timer")) && error.isError())++failures;
        });
    check(!eval(QStringLiteral("setTimeout(function timerError(){throw new Error('planned');},10); ")).isError(),"exception setup");
    QEventLoop third;
    QTimer::singleShot(35,&third,&QEventLoop::quit);
    third.exec();
    check(failures==1,"exception forwarded as QJSValue");

    const auto hold=eval(QStringLiteral("var hold=[];for(var i=0;i<256;i++)hold.push(setTimeout(function(){},60000)); hold.length;"));
    check(!hold.isError() && hold.toInt()==256,"256 independent active timers");
    check(manager.activeCount()==256,"limit count");
    const auto over=eval(QStringLiteral("(function(){try{setTimeout(function(){},60000);return false;}catch(e){return e.message.indexOf('256')>=0;}})()"));
    check(!over.isError() && over.toBool(),"257th timer rejected");
    manager.shutdown();
    check(manager.activeCount()==0,"shutdown clears all timer references");
    const auto after=eval(QStringLiteral("(function(){try{setTimeout(function(){},100);return false;}catch(e){return true;}})()"));
    check(!after.isError() && after.toBool(),"cannot schedule after shutdown");
    for(const QString path:{QStringLiteral("setInterval"),QStringLiteral("setTimeout"),
        QStringLiteral("clearTimeout"),QStringLiteral("clearInterval")}){
        check(ApiKnowledge::isDocumentedSymbol(path),"API inventory");
        check(ApiKnowledge::lookup(path).known,"API help");
        check(!EditorAssist::signature(path,0).isEmpty(),"editor signature");
    }
    if(errors)qCritical()<<errors<<"timer smoke errors";
    else qInfo()<<"PASS: Panel JS timers, callbacks, cap, errors, cleanup, API catalog";
    return errors ? 1 : 0;
}
