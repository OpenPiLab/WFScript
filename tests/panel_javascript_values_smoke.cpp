// v5.6.70.2 — no GUI, SDK, or hardware; tests the actual QObject bridge and QJSEngine.
// In particular a QVariant-returning slot must NEVER leak a QVariantList wrapper
// that fails Array.isArray() into a Panel script or an event callback.
#include "PanelUiBridge.h"
#include "ScriptValueConversion.h"
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QJSValue eval(QJSEngine& engine,const char* script) {
    const QJSValue result=engine.evaluate(QString::fromUtf8(script),QStringLiteral("native-js-values-smoke.js"));
    if(result.isError())qFatal("Unexpected JS error: %s",qPrintable(result.toString()));
    return result;
}
static bool truth(QJSEngine& engine,const char* script) {
    const auto result=eval(engine,script);
    if(!result.toBool())qFatal("Failed JS contract: %s",script);
    return result.toBool();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    const QVariantList first{QStringLiteral("100"),QStringLiteral("0.50")};
    const QVariantList second{QStringLiteral("500"),QStringLiteral("1.00")};
    const QVariantList table{QVariant(first),QVariant(second)};
    QVariantMap uiValues{
        {QStringLiteral("recipeTable"),table},
        {QStringLiteral("notes"),QString()},
        {QStringLiteral("nested"),QVariantMap{
            {QStringLiteral("values"),QVariantList{1,QVariantList{2,3}}},
            {QStringLiteral("flags"),QStringList{QStringLiteral("A"),QStringLiteral("B")}},
            {QStringLiteral("enabled"),true}}},
        {QStringLiteral("number"),1000},
        {QStringLiteral("emptyTable"),QVariantList{}}
    };
    PanelUiBridge ui(&engine,uiValues);
    QJSEngine::setObjectOwnership(&ui,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("__panelHost"),engine.newQObject(&ui));
    eval(engine,R"JS(var ui = {get: function(id){return __panelHost.get(id);},
                             set: function(id,v){return __panelHost.set(id,v);}};)JS");
    truth(engine,R"JS((function(){
        const r=ui.get('recipeTable');
        return Array.isArray(r) && r.length===2 && Array.isArray(r[0]) &&
            r[0][0]==='100' && r[1][1]==='1.00' &&
            r.map(row=>Number(row[0])).reduce((a,b)=>a+b,0)===600;
    })())JS");
    truth(engine,"ui.get('notes') === '' && ui.get('number') === 1000");
    truth(engine,R"JS((function(){
        const v=ui.get('nested');
        return Object.getPrototypeOf(v)===Object.prototype &&
            Array.isArray(v.values) && Array.isArray(v.values[1]) &&
            v.values[1][1]===3 && Array.isArray(v.flags) &&
            v.flags.join(',')==='A,B' && v.enabled===true &&
            JSON.stringify(v).indexOf('"values"')>=0;
    })())JS");
    truth(engine,"Array.isArray(ui.get('emptyTable')) && ui.get('emptyTable').length===0");
    truth(engine,"typeof ui.get('unknown') === 'undefined'");
    // Returns an independent JS copy: no hidden mutation of the native model.
    truth(engine,R"JS((function(){
        const r=ui.get('recipeTable');r[0][0]='999';r.push(['1000','1']);
        return ui.get('recipeTable').length===2 && ui.get('recipeTable')[0][0]==='100';
    })())JS");
    QVariant observed;
    QObject::connect(&ui,&PanelUiBridge::updateRequested,&ui,
                     [&](const QString& id,const QVariant& value){
        if(id==QStringLiteral("recipeTable"))observed=value;
    },Qt::DirectConnection);
    truth(engine,R"JS(ui.set('recipeTable', [['250','0.75'],['750','1.25']]))JS");
    assert(observed.toList().size()==2);
    assert(observed.toList().at(1).toList().at(0).toString()==QStringLiteral("750"));
    truth(engine,"Array.isArray(ui.get('recipeTable')) && ui.get('recipeTable')[1][0]==='750'");
    // A GUI-originating table edit arrives as QVariantList; both callback
    // mechanisms (ui.on and managed widget handlers) must get genuine arrays.
    ui.receiveEvent(QStringLiteral("recipeTable"),table);
    engine.globalObject().setProperty(QStringLiteral("payload"),
        ScriptValueConversion::toJavaScript(&engine,table));
    truth(engine,"Array.isArray(payload) && Array.isArray(payload[0]) && payload[0][0]==='100'");
    truth(engine,"Array.isArray(ui.get('recipeTable')) && ui.get('recipeTable')[1][1]==='1.00'");
    // QVariantMap containing arrays and QJson collections should also stay native.
    engine.globalObject().setProperty(QStringLiteral("jsonPayload"),
        ScriptValueConversion::toJavaScript(&engine,QJsonObject{{"rows",QJsonArray{
            QJsonArray{100,500},QJsonArray{1000,2000}}}}));
    truth(engine,"Array.isArray(jsonPayload.rows) && Array.isArray(jsonPayload.rows[1]) && jsonPayload.rows[1][1]===2000");
    // Audit typed C++ result APIs: these deliberately return QVariantList and
    // QVariantMap (unlike the formerly generic PanelUiBridge::get()).
    std::atomic_bool stopping{false};
    TestRunnerBridge runner(&engine,[](const QString&,const QVariant&){return true;},&stopping);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    truth(engine,R"JS((function(){
        return TestRunner.configure({title:'native JS arrays'}) &&
               TestRunner.add({id:'check',name:'Check',type:'test',group:'main'},
                   function(){return TestRunner.measure('V',3.3,3.2,3.4,'V');}) &&
               TestRunner.run();
    })())JS");
    truth(engine,R"JS((function(){
        const results=TestRunner.results(),report=TestRunner.report();
        return globalThis===this && Array.isArray(results) && results.length===1 &&
               Array.isArray(report.steps) && Array.isArray(report.measurements) &&
               report.measurements[0].name==='V' &&
               typeof report.steps.map==='function' &&
               report.steps[0].id==='check';
    }).call(globalThis))JS");
    // A returned JS snapshot must not mutate the native Test Executive report.
    truth(engine,R"JS((function(){
        const result=TestRunner.results();
        result[0].name='mutated'; result.push({name:'unexpected'});
        const report=TestRunner.report();
        report.steps[0].id='mutated';
        return TestRunner.results().length===1 &&
               TestRunner.results()[0].name==='V' &&
               TestRunner.report().steps[0].id==='check';
    })())JS");
    runner.shutdown();
    qInfo("PASS: ui.get, ui.set, edit events, native arrays/maps, TestRunner collections");
    return 0;
}
