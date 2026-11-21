// RC1.5.10 native Qt smoke: named results, JSON arrays, Once + callback,
// limit failures, missing fields, no partial output, stale callback tokens.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QJSEngine>
#include <QJSValue>
#include <atomic>
#include <functional>
#include <cmath>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QJSValue js(QJSEngine& engine,const QString& code) {
    QJSValue r=engine.evaluate(code,QStringLiteral("<structured-result-smoke>"));
    if(r.isError())qFatal("JS error: %s",qPrintable(r.toString()));
    return r;
}
static void waitUntil(const std::function<bool()>& predicate) {
    QElapsedTimer timer;timer.start();
    while(!predicate() && timer.elapsed()<3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    assert(predicate());
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool stop(false);
    TestRunnerBridge runner(&engine,TestRunnerBridge::UiWriter{},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    const auto loaded=js(engine,QStringLiteral(R"JS(
      var tokenWave='',tokenArray='',tokenLegacy='';
      var handlers={
        setup:function(){return true;},
        wave:function(step){tokenWave=step.token;},
        channels:function(step){tokenArray=step.token;},
        immediate:function(step){return {gain:step.params.gain,extra:{unit:'ratio'}};},
        legacy:function(step){tokenLegacy=step.token;},
        cleanup:function(){return true;}
      };
      var doc={schemaVersion:1,id:'structured-smoke',title:'Structured result smoke',
        config:{stopOnFailure:true,totalTimeoutMs:2000},steps:[
          {id:'setup',name:'Setup',group:'setup',type:'action',mode:'once',handler:'setup'},
          {id:'wave',name:'Waveform',group:'main',type:'numeric',mode:'callback',handler:'wave',timeoutMs:500,
           measurements:{vpp:{low:1.9,high:2.1,name:'Amplitude',unit:'Vpp'},
                         'metrics.frequency':{low:970,high:1030,name:'Frequency',unit:'Hz'},
                         shape:{low:0.32,high:0.40,name:'Shape',unit:'ratio'}}},
          {id:'channels',name:'Channels',group:'main',type:'numeric',mode:'callback',handler:'channels',timeoutMs:500,
           measurements:{'channels[0]':{low:4.9,high:5.1,name:'Channel A',unit:'V'},
                         'channels[1]':{low:3.2,high:3.4,name:'Channel B',unit:'V'},
                         'temperatures[*]':{low:20,high:30,name:'Temp',unit:'C'}}},
          {id:'immediate',name:'Gain',group:'main',type:'numeric',mode:'once',handler:'immediate',
           parameters:{gain:2},measurements:{gain:{low:1.9,high:2.1,name:'Gain'}}},
          {id:'legacy',name:'Legacy',group:'main',type:'numeric',mode:'callback',handler:'legacy',timeoutMs:500,
           limits:{low:4.7,high:4.8},unit:'V'},
          {id:'cleanup',name:'Cleanup',group:'cleanup',type:'action',mode:'once',handler:'cleanup'}
        ]};
      TestRunner.loadDefinition(doc,handlers);
    )JS"));
    assert(loaded.toBool());
    assert(runner.run());
    QString wave=js(engine,QStringLiteral("tokenWave")).toString();
    assert(!wave.isEmpty());
    assert(runner.complete(wave,js(engine,QStringLiteral("({vpp:2.04,metrics:{frequency:1000.2},shape:0.352,metadata:{samples:1000}})"))));
    assert(!runner.complete(wave,QJSValue(3.0)));
    waitUntil([&]{return !js(engine,QStringLiteral("tokenArray")).toString().isEmpty();});
    QString channels=js(engine,QStringLiteral("tokenArray")).toString();
    assert(!runner.complete(wave,js(engine,QStringLiteral("({vpp:5})"))));
    assert(runner.complete(channels,js(engine,QStringLiteral("({channels:[4.999,3.301],temperatures:[24.1,25.2,26.3]})"))));
    waitUntil([&]{return !js(engine,QStringLiteral("tokenLegacy")).toString().isEmpty();});
    QString legacy=js(engine,QStringLiteral("tokenLegacy")).toString();
    assert(runner.complete(legacy,QJSValue(4.749)));
    waitUntil([&]{return runner.state()!=QStringLiteral("running");});
    assert(runner.state()==QStringLiteral("passed"));
    const QVariantMap record=runner.reportData();
    const auto measurements=record.value(QStringLiteral("measurements")).toList();
    assert(measurements.size()==10); // 3 wave, 5 array, 1 Once, 1 legacy
    for(const auto& m:measurements)assert(m.toMap().value(QStringLiteral("status"))==QStringLiteral("PASS"));
    bool foundArray=false;
    for(const auto& m:measurements)
        if(m.toMap().value(QStringLiteral("path"))==QStringLiteral("temperatures[2]"))foundArray=true;
    assert(foundArray);
    const auto steps=record.value(QStringLiteral("steps")).toList();
    assert(steps.at(1).toMap().value(QStringLiteral("result")).toMap().value(QStringLiteral("metadata")).toMap().value(QStringLiteral("samples")).toInt()==1000);
    assert(steps.at(3).toMap().value(QStringLiteral("result")).toMap().value(QStringLiteral("gain")).toInt()==2);
    assert(!runner.complete(channels,js(engine,QStringLiteral("({channels:[1,2]})"))));

    // A missing configured field is an execution error; zero partial rows.
    assert(js(engine,QStringLiteral("TestRunner.loadDefinition(doc,handlers)")).toBool());
    assert(runner.run());
    wave=js(engine,QStringLiteral("tokenWave")).toString();
    assert(runner.complete(wave,js(engine,QStringLiteral("({vpp:2.0,metrics:{frequency:1000}})"))));
    waitUntil([&]{return runner.state()!=QStringLiteral("running");});
    assert(runner.state()!=QStringLiteral("passed"));
    assert(runner.resultsData().size()==0);

    // A limit failure is NOT a malformed result. All measurements are recorded.
    assert(js(engine,QStringLiteral("TestRunner.loadDefinition(doc,handlers)")).toBool());
    assert(runner.run());
    wave=js(engine,QStringLiteral("tokenWave")).toString();
    assert(runner.complete(wave,js(engine,QStringLiteral("({vpp:2.5,metrics:{frequency:1000},shape:0.35})"))));
    waitUntil([&]{return runner.state()!=QStringLiteral("running");});
    assert(runner.state()==QStringLiteral("failed"));
    assert(runner.resultsData().size()==3);
    assert(runner.summary().value(QStringLiteral("failedMeasurements")).toInt()==1);

    // Invalid nonfinite field and extra-long array must be rejected atomically.
    assert(js(engine,QStringLiteral("TestRunner.loadDefinition(doc,handlers)")).toBool());
    assert(runner.run());
    wave=js(engine,QStringLiteral("tokenWave")).toString();
    // Invalid data is accepted as a step ERROR, not as a numeric PASS.
    assert(runner.complete(wave,js(engine,QStringLiteral("({vpp:NaN,metrics:{frequency:1000},shape:0.35})"))));
    waitUntil([&]{return runner.state()!=QStringLiteral("running");});
    assert(runner.resultsData().isEmpty());
    runner.shutdown();
    return 0;
}
