// v0.2-dev.8.9.3.2: RC Step Lab's actual embedded JS in Qt QJSEngine.
// Node.js accepts syntax that QJSEngine rejects. This test must run with Qt.
#include "PanelProject.h"
#include "PanelSchemaTestEvents.h"
#include "ScriptLibraryBridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSEngine>
#include <QJSValue>
#include <QDebug>
static void need(bool yes,const char* message){if(!yes)qFatal("RC Step Lab QJSEngine smoke: %s",message);}
static void needOk(QJSValue value,const char* message){
    if(value.isError())qCritical().noquote()<<value.toString()<<value.property("stack").toString();
    need(!value.isError(),message);
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    registerPanelSchemaTestEvents();
    QFile file(QStringLiteral(":/wfscript/examples/RCStepResponseLaboratory.wfpanel"));
    need(file.open(QIODevice::ReadOnly),"RC example embedded as Qt resource");
    const auto doc=QJsonDocument::fromJson(file.readAll());
    need(doc.isObject(),"RC JSON parses");
    PanelProject panel; QString why;
    if(!PanelProject::fromJson(doc.object(),QStringLiteral("rc-step.wfpanel"),&panel,&why))
        qCritical().noquote()<<why;
    need(panel.pages.size()==11,"native PanelProject schema and 11 Pages accepted");
    need(panel.libraries.size()==1,"embedded JS module is present");
    QJSEngine engine;
    QVariantMap embedded;
    for(const auto& value:panel.libraries){
        const auto lib=value.toObject();
        embedded.insert(lib.value(QStringLiteral("name")).toString(),lib.value(QStringLiteral("source")).toString());
    }
    ScriptLibraryBridge host(&engine,QString(),embedded);
    QJSEngine::setObjectOwnership(&host,QJSEngine::CppOwnership);
    need(host.valid(),"embedded module materialized");
    engine.globalObject().setProperty(QStringLiteral("__wfLibraryHost"),engine.newQObject(&host));
    needOk(engine.evaluate(ScriptLibraryBridge::bootstrapSource(),QStringLiteral("ModuleBootstrap.js")),
           "native module bootstrap parses");
    // No instrument bridges are used: the test runs in MODEL mode only.
    needOk(engine.evaluate(QStringLiteral(R"JS(
        var Device={SN:'DEMO'};
        var ui={_values:{rcR:980,rcC:571,rcFreq:100,rcVpk:0.5,rcNoise:0,stepWiringVerified:false},
            get:function(id){return this._values[id];},
            set:function(id,value){this._values[id]=value;return true;}};
        var plots={clear:function(){},configure:function(){},setSeries:function(){}};
        var print=function(){};
    )JS"),QStringLiteral("RCStepNativeMocks.js")),"native mocks parse");
    // This specifically failed at RCStepResponseLaboratory.js:170 before v8.9.3.2.
    needOk(engine.evaluate(panel.script,QStringLiteral("RCStepResponseLaboratory.js")),
           "FULL native panel script parses and loads real embedded module");
    QJSValue check=engine.evaluate(QStringLiteral(R"JS(
      (function(){
        onInitialize();
        if(!state.lastResult || state.lastResult.source!=='MODEL')throw Error('MODEL not initialized');
        if(!(Math.abs(state.lastResult.fitTauS-980*571e-9)<2e-8))throw Error('bad exponential fit');
        onSyntheticClicked();
        if(state.records.length!==1 || state.records[0].source!=='MODEL')throw Error('notebook failed');
        if(state.records[0].ch1 || state.records[0].ch2 || state.records[0].edges)
            throw Error('raw sample arrays leaked into notebook');
        if(!(state.records[0].transitions.length>=2))throw Error('fit metadata lost');
        if(!(ui._values.stepNotebook.length===1))throw Error('table did not update');
        onShutdown();
        return true;
      })()
    )JS"),QStringLiteral("RCStepNativeSimulation.js"));
    needOk(check,"native initialization, model fit, and notebook execute");
    need(check.toBool(),"native simulation returned true");
    qInfo()<<"PASS RC Step Lab native QJSEngine: schema, full script, real module, simulation, notebook";
    return 0;
}
