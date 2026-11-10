// WFScript v5.6.56 — direct native Qt executive smoke test, no AD3 / DWF DLL.
// This uses real QJSEngine -> QObject -> JS callback re-entry and QVariant data.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QVariantMap>
#include <QHash>
#include <QStringList>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static bool evalOk(QJSEngine& engine,const QString& code) {
    const QJSValue value=engine.evaluate(code,QStringLiteral("<runner-smoke>"));
    if(value.isError())qFatal("TestRunner JavaScript error: %s",qPrintable(value.toString()));
    return value.toBool();
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stop=false;
    QHash<QString,QVariant> output;
    QStringList events;
    QString exceptionId;
    TestRunnerBridge runner(&engine,[&](const QString& id,const QVariant& value) {
        if(id==QStringLiteral("log")) {
            const QVariantMap m=value.toMap();
            if(m.value(QStringLiteral("clear")).toBool())events.clear();
            if(m.contains(QStringLiteral("append")))
                events.push_back(m.value(QStringLiteral("append")).toMap().value(QStringLiteral("level")).toString());
        } else output[id]=value;
        return true;
    },&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    QObject::connect(&runner,&TestRunnerBridge::stepException,&runner,
        [&](const QString& id,const QJSValue& value) {
            assert(value.isError());exceptionId=id;
        },Qt::DirectConnection);
    assert(evalOk(engine,QStringLiteral(R"JS(
        TestRunner.configure({title:'Native Test',sequenceId:'sequence',tableId:'table',
                              logId:'log',summaryId:'summary',stopOnFailure:true}) &&
        TestRunner.add({id:'init',name:'Initialize'},function(){return true;}) &&
        TestRunner.add({id:'power',name:'Verify voltage'},function(){
            return TestRunner.measure('Voltage',3.301,3.25,3.35,'V','resultV');
        }) &&
        TestRunner.add({id:'pwm',name:'Check PWM'},function(){
            return TestRunner.measure('PWM',globalThis.duty,49,51,'%','resultPwm');
        }) &&
        TestRunner.add({id:'report',name:'Report'},function(){return true;});
    )JS")));
    assert(output.value(QStringLiteral("sequence")).toList().size()==4);
    assert(evalOk(engine,QStringLiteral("TestRunner.reset();")));
    assert(events.isEmpty());
    assert(evalOk(engine,QStringLiteral("TestRunner.next();")));
    assert(runner.state()==QStringLiteral("running"));
    assert(runner.summary().value(QStringLiteral("completedSteps")).toInt()==1);
    assert(evalOk(engine,QStringLiteral("globalThis.duty=50.1;TestRunner.run();")));
    assert(runner.state()==QStringLiteral("passed"));
    assert(runner.summary().value(QStringLiteral("completedSteps")).toInt()==4);
    assert(runner.summary().value(QStringLiteral("passedMeasurements")).toInt()==2);
    assert(runner.resultsData().size()==2);
    assert(output.value(QStringLiteral("table")).toList().size()==2);
    assert(output.value(QStringLiteral("resultV")).toDouble()>3.3);
    assert(events.contains(QStringLiteral("PASS")));
    assert(!evalOk(engine,QStringLiteral("globalThis.duty=47.5;TestRunner.run();")));
    assert(runner.state()==QStringLiteral("failed"));
    assert(runner.summary().value(QStringLiteral("completedSteps")).toInt()==3);
    assert(runner.summary().value(QStringLiteral("failedMeasurements")).toInt()==1);
    assert(events.contains(QStringLiteral("FAIL")));
    assert(evalOk(engine,QStringLiteral("TestRunner.reset();")));
    assert(runner.state()==QStringLiteral("idle"));
    assert(output.value(QStringLiteral("table")).toMap().value(QStringLiteral("reset")).toBool());
    assert(evalOk(engine,QStringLiteral("TestRunner.configure({stopOnFailure:false});")));
    assert(!evalOk(engine,QStringLiteral("TestRunner.run();")));
    assert(runner.state()==QStringLiteral("failed"));
    assert(runner.summary().value(QStringLiteral("completedSteps")).toInt()==4);
    assert(evalOk(engine,QStringLiteral("TestRunner.clear();")));
    assert(evalOk(engine,QStringLiteral(R"JS(
        TestRunner.add({id:'explode',name:'Exception'},function(){throw Error('test exception');});
    )JS")));
    assert(!evalOk(engine,QStringLiteral("TestRunner.run();")));
    assert(runner.state()==QStringLiteral("error"));
    assert(exceptionId==QStringLiteral("explode"));
    assert(evalOk(engine,QStringLiteral("TestRunner.clear();")));
    assert(evalOk(engine,QStringLiteral(R"JS(
        TestRunner.add({id:'cancel',name:'Abort'},function(){ TestRunner.abort(); return true; });
    )JS")));
    assert(!evalOk(engine,QStringLiteral("TestRunner.run();")));
    assert(runner.state()==QStringLiteral("aborted"));
    assert(!evalOk(engine,QStringLiteral("TestRunner.add({id:'invalid',name:'Bad'},null);")));
    assert(!runner.lastError().isEmpty());
    assert(!evalOk(engine,QStringLiteral("TestRunner.run();")));
    // The aborted callback always causes the next run to end ABORTED.
    assert(runner.state()==QStringLiteral("aborted"));
    return 0;
}
