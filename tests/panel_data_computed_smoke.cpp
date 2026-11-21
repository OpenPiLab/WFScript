// Qt6-native integration: production JS bootstrap + real typed PanelUiBridge.
// No GUI display or physical Digilent instrument is required.
#include "PanelDataEvents.h"
#include "PanelUiBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

static void require(bool test,const char* context) {
    if(!test)qFatal("Panel Data computed smoke failed: %s",context);
}
static void eval(QJSEngine& engine,const QString& source,const char* context) {
    const QJSValue result=engine.evaluate(source,QString::fromLatin1(context));
    if(result.isError())qCritical().noquote()<<result.toString()<<result.property("stack").toString();
    require(!result.isError() && result.toBool(),context);
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    const QJsonArray definitions{
        QJsonObject{{"name","Motor.RPM"},{"type","number"},{"initial",600.0}},
        QJsonObject{{"name","Motor.RPS"},{"type","number"},{"initial",0.0}},
        QJsonObject{{"name","Motor.Period"},{"type","number"},{"initial",0.0}},
        QJsonObject{{"name","Motor.Mode"},{"type","text"},{"initial","Idle"}},
        QJsonObject{{"name","Motor.ModeLabel"},{"type","text"},{"initial",""}},
        QJsonObject{{"name","Motor.Flag"},{"type","boolean"},{"initial",false}},
        QJsonObject{{"name","Input.Target"},{"type","number"},{"initial",0.0}}
    };
    const QJsonArray controls{
        QJsonObject{{"id","speedInput"},{"type","numeric_stepper"},
                    {"bindings",QJsonObject{{"value","Motor.RPM"}}}},
        QJsonObject{{"id","rpsDisplay"},{"type","digital_display"},
                    {"bindings",QJsonObject{{"value","Motor.RPS"}}}},
        QJsonObject{{"id","periodDisplay"},{"type","numeric"},
                    {"bindings",QJsonObject{{"value","Motor.Period"}}}},
        QJsonObject{{"id","editableTarget"},{"type","numeric_stepper"},
                    {"bindings",QJsonObject{{"value","Input.Target"}}}}
    };
    PanelUiBridge host(&engine,QVariantMap{{"speedInput",600.0},{"rpsDisplay",0.0},
                                        {"periodDisplay",0.0},{"editableTarget",0.0}},
                       {},definitions,controls);
    QJSEngine::setObjectOwnership(&host,QJSEngine::CppOwnership);
    engine.globalObject().setProperty("__panelHost",engine.newQObject(&host));
    int rpsWidgetUpdates=0,reportedErrors=0;
    QObject::connect(&host,&PanelUiBridge::updateRequested,&host,
        [&](const QString& id,const QVariant&) { if(id==QStringLiteral("rpsDisplay"))++rpsWidgetUpdates; });
    QObject::connect(&host,&PanelUiBridge::dataCallbackFailed,&host,
        [&](const QString&,const QJSValue&) { ++reportedErrors; });
    QJSValue result=engine.evaluate(PanelDataEvents::bootstrapSource(),"<bootstrap>");
    require(!result.isError(),"production bootstrap compiles under QJSEngine");
    eval(engine,QStringLiteral(R"JS(
        var changes=[];
        if(Data.computed('Motor.RPS',['Motor.RPM'],function(rpm){return rpm/60;})!=='Motor.RPS')
            throw Error('registration result');
        Data.computed('Motor.Period',['Motor.RPS'],function(rps){return rps===0?0:1/rps;});
        Data.computed('Motor.ModeLabel',['Motor.Mode'],function(mode){return 'Mode: '+mode;});
        Data.computed('Motor.Flag',['Motor.RPM'],function(rpm){return rpm>2000;});
        if(!Data.isComputed('Motor.RPS') || Data.isComputed('Motor.RPM'))throw Error('computed identity');
        if(Data.get('Motor.RPS')!==10 || Data.get('Motor.Period')!==0.1 ||
           Data.get('Motor.ModeLabel')!=='Mode: Idle')throw Error('initial values');
        Data.onChange('Motor.RPM',function(v){
            changes.push([v,Data.get('Motor.RPS'),Data.get('Motor.Period')]);
        });
        Data.set('Motor.RPM',1200);
        if(Data.get('Motor.RPS')!==20 || Data.get('Motor.Period')!==0.05 ||
           changes.length!==1 || changes[0][1]!==20)throw Error('dependency ordering');
        try {Data.set('Motor.RPS',2);throw Error('writable computed');}
        catch(e){if(String(e).includes('writable computed'))throw e;}
        try {Data.computed('Motor.RPM',['Motor.RPS'],function(v){return v;});
             throw Error('circular graph accepted');}
        catch(e){if(String(e).includes('circular graph accepted') ||
                    !String(e).includes('circular'))throw e;}
        try {Data.computed('Input.Target',['Motor.RPM'],function(v){return v;});
             throw Error('editable output accepted');}
        catch(e){if(String(e).includes('editable output accepted'))throw e;}
        true;
    )JS"),"<registration>");
    require(rpsWidgetUpdates==2,"initial + computed update reach native widget binding");
    require(!host.setData("Motor.RPS",QJSValue(300.0)),"native computed write also rejected");
    require(host.dataValue("Motor.RPS").toDouble()==20.0,"native read-only target preserved");
    const QVariant previous=host.dataValue("Motor.RPM");
    require(host.setDataFromInput("speedInput","valueChanged",QVariant(3000.0)),
            "operator editing writable source");
    const QVariant current=host.dataValue("Motor.RPM");
    result=engine.globalObject().property("__panelDataDispatch").call(QJSValueList{
        QJSValue(QStringLiteral("Motor.RPM")),QJSValue(current.toDouble()),QJSValue(previous.toDouble())});
    require(!result.isError(),"native operator dispatch doesn't error");
    require(host.dataValue("Motor.RPS").toDouble()==50.0,"operator input updates computed target");
    require(host.dataValue("Motor.Period").toDouble()==0.02,"chained target propagated");
    require(host.dataValue("Motor.Flag").toBool(),"boolean derived target propagated");
    require(rpsWidgetUpdates==3,"operator input refreshes computed visual binding");
    eval(engine,QStringLiteral(R"JS(
        Data.set('Motor.Mode','Run');
        if(Data.get('Motor.ModeLabel')!=='Mode: Run')throw Error('text computed');
        Data.set('Motor.RPM',1000);
        if(Data.get('Motor.Flag')!==false)throw Error('boolean computed');
        true;
    )JS"),"<typed-derived>");
    require(reportedErrors==0,"no unexpected formula diagnostics");
    qInfo()<<"PASS: real Qt Panel Data computed values, typed outputs, read-only bindings, input propagation, chained formulas";
    return 0;
}
