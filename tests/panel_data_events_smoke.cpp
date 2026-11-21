// Full QJSEngine execution of the EXACT production Panel Data subscription
// bootstrap. No QWidget, DWF SDK device or physical hardware required.
#include "PanelDataEvents.h"
#include "PanelDataSchema.h"
#include "PanelUiBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

static void require(bool ok,const char* what) {
    if(!ok)qFatal("Panel Data Events smoke failed: %s",what);
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    const QJsonArray definitions{
        QJsonObject{{"name","Motor.Speed"},{"type","number"},{"initial",0.0}},
        QJsonObject{{"name","Motor.Ready"},{"type","boolean"},{"initial",false}},
        QJsonObject{{"name","Motor.Mode"},{"type","text"},{"initial","Auto"}}
    };
    const QJsonArray controls{QJsonObject{{"id","speedInput"},{"type","numeric_stepper"},
                                        {"bindings",QJsonObject{{"value","Motor.Speed"}}}}};
    PanelUiBridge host(&engine,QVariantMap{{"speedInput",0.0}}, {},definitions,controls);
    QJSEngine::setObjectOwnership(&host,QJSEngine::CppOwnership);
    engine.globalObject().setProperty("__panelHost",engine.newQObject(&host));
    int failures=0, widgets=0;
    QObject::connect(&host,&PanelUiBridge::dataCallbackFailed,&host,
        [&](const QString& name,const QJSValue& error){
            require(name=="Motor.Speed" && error.isError(),"callback exception forwarded as QJSValue");
            ++failures;
        });
    QObject::connect(&host,&PanelUiBridge::updateRequested,&host,
        [&](const QString&,const QVariant&){++widgets;});
    QJSValue result=engine.evaluate(PanelDataEvents::bootstrapSource(),"<panel-data-events>");
    require(!result.isError(),"production JavaScript bootstrap compiles");
    result=engine.evaluate(QStringLiteral(R"JS(
        var events=[];
        var id=Data.onChange('Motor.Speed',function(value,old,name){events.push([value,old,name]);});
        Data.set('Motor.Speed',100);
        Data.set('Motor.Speed',100);   // equal write: no duplicate
        Data.set('Motor.Speed',125);
        if(events.length!==2 || events[0][0]!==100 || events[0][1]!==0 ||
           events[0][2]!=='Motor.Speed' || events[1][1]!==100)
            throw Error('value, previousValue, name or deduplication');
        if(!Data.offChange(id) || Data.offChange(id))throw Error('unsubscribe');
        Data.set('Motor.Speed',150);
        if(events.length!==2)throw Error('detached listener still called');
        var immediate=[];
        var onceInitial=Data.onChange('Motor.Speed',function(v,p,n){immediate.push([v,p,n]);}, {immediate:true});
        if(immediate.length!==1 || immediate[0][0]!==150 || immediate[0][1]!==undefined)
            throw Error('immediate initial notification');
        Data.offChange(onceInitial);
        if(!Array.isArray(Data.names()) || Data.names().length!==3)
            throw Error('Data.names regression');
        try { Data.onChange('missing',function(){}); throw Error('not rejected'); }
        catch(e) { if(String(e).includes('not rejected'))throw e; }
        try { Data.onChange('Motor.Speed',12); throw Error('not rejected'); }
        catch(e) { if(String(e).includes('not rejected'))throw e; }
        try { Data.onChange('Motor.Speed',function(){}, {immediate:'yes'}); throw Error('not rejected'); }
        catch(e) { if(String(e).includes('not rejected'))throw e; }
        true;
    )JS"),"<test-data-1>");
    require(!result.isError() && result.toBool(),"set, no-op, unsubscribe, immediate, invalid args");
    require(widgets==3,"Data.set updates bound widgets only on actual change");
    result=engine.evaluate(QStringLiteral(R"JS(
        var order=[];
        var a=Data.onChange('Motor.Speed',function(v,p,n){
            order.push('first:'+v);
            if(v===200)Data.set('Motor.Ready',true);
        });
        var b=Data.onChange('Motor.Ready',function(v,p,n){order.push('ready:'+v);});
        var c=Data.onChange('Motor.Speed',function(v){order.push('second:'+v);});
        Data.set('Motor.Speed',200);
        if(order.join('|') !== 'first:200|second:200|ready:true')throw Error('event queue order');
        Data.offChange(a);Data.offChange(b);Data.offChange(c);
        // A listener that throws must not prevent another subscriber.
        var bad=Data.onChange('Motor.Speed',function(){throw Error('deliberate exception');});
        var good=0, goodId=Data.onChange('Motor.Speed',function(){++good;});
        Data.set('Motor.Speed',250);
        Data.offChange(bad);Data.offChange(goodId);
        if(good!==1)throw Error('callback isolation');
        true;
    )JS"),"<test-data-2>");
    require(!result.isError() && result.toBool(),"nonrecursive queue and callback isolation");
    require(failures==1,"one diagnostic for thrown subscriber");
    // Simulate the same native input bridge and dispatch called by PanelRuntime.
    result=engine.evaluate(QStringLiteral(R"JS(
        var inputOld=-1, inputNew=-1, inputCount=0;
        var inputWatch=Data.onChange('Motor.Speed',function(v,p,n){
            inputOld=p;inputNew=v;++inputCount;
        });
        true;
    )JS"));
    require(!result.isError(),"input listener registered");
    const QVariant before=host.dataValue("Motor.Speed");
    require(host.setDataFromInput("speedInput","valueChanged",QVariant(275.0)),"native bound input accepted");
    const QVariant after=host.dataValue("Motor.Speed");
    require(before.toDouble()==250 && after.toDouble()==275,"native input old/new readback");
    result=engine.globalObject().property("__panelDataDispatch").call(QJSValueList{
        QJSValue("Motor.Speed"),QJSValue(after.toDouble()),QJSValue(before.toDouble())});
    require(!result.isError(),"native change dispatched to JS registry");
    result=engine.evaluate(QStringLiteral("inputOld===250 && inputNew===275 && inputCount===1 && Data.offChange(inputWatch)"));
    require(!result.isError() && result.toBool(),"native operator input reaches subscribed JS callback exactly once");
    result=engine.evaluate(QStringLiteral(R"JS(
        var feedback=Data.onChange('Motor.Speed',function(v){Data.set('Motor.Speed',v+1);});
        var loopStopped=false;
        try { Data.set('Motor.Speed',300); }
        catch(e) {loopStopped=/change cycle exceeded 256 events/.test(String(e));}
        Data.offChange(feedback);
        if(!loopStopped)throw Error('feedback loop did not stop');
        Data.set('Motor.Speed',7);
        if(Data.get('Motor.Speed')!==7)throw Error('registry not usable after loop guard');
        true;
    )JS"),"<test-data-3>");
    require(!result.isError() && result.toBool(),"bounded feedback guard and recovery");
    qInfo()<<"PASS: Data.onChange QJSEngine bootstrap, input bridge, change isolation, immediate, unsubscribe, bounded feedback";
    return 0;
}
