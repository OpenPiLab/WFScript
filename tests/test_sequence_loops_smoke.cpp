// RC1.5.11: bounded, data-only loop expansion and runtime integration smoke.
#include "TestSequenceLoops.h"
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJSValue>
#include <atomic>
#include <functional>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QJsonObject parse(const char* text) {
    QJsonParseError err;
    const QJsonDocument document=QJsonDocument::fromJson(QByteArray(text),&err);
    assert(err.error==QJsonParseError::NoError && document.isObject());
    return document.object();
}
static void waitFor(const std::function<bool()>& finished) {
    QElapsedTimer elapsed;elapsed.start();
    while(!finished()&&elapsed.elapsed()<3000)QCoreApplication::processEvents(QEventLoop::AllEvents,25);
    assert(finished());
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QJsonObject definition=parse(R"JSON({
      "schemaVersion":1,"id":"loops-lab","title":"Loops and sweeps",
      "config":{"stopOnFailure":true,"totalTimeoutMs":2000},
      "steps":[
        {"id":"preflight","name":"Preflight","group":"setup","mode":"once","type":"action","handler":"ready"},
        {"id":"dc_range","name":"Voltage sweep","group":"main",
         "loop":{"parameter":"voltage","start":5,"stop":4.5,"increment":-0.25},
         "steps":[
            {"id":"psu","name":"PSU ${voltage} V","mode":"callback","type":"numeric","handler":"readPsu",
             "limits":{"low":{"param":"voltage","offset":-0.1},"high":{"param":"voltage","offset":0.1}}},
            {"id":"dmm","name":"DMM ${voltage} V","mode":"callback","type":"numeric","handler":"readDmm",
             "limits":{"low":{"param":"voltage","offset":-0.075},"high":{"param":"voltage","offset":0.075}}}
          ]},
        {"id":"wave_sweep","name":"Waveform grid","group":"main",
         "loop":{"axes":[{"parameter":"amplitude","values":[1,2]},
                         {"parameter":"frequency","values":[1000,5000]}]},
         "steps":[{"id":"wave","name":"Wave ${amplitude} Vpp ${frequency} Hz", "type":"numeric","mode":"once",
          "handler":"readWave", "measurements":{
             "vpp":{"low":{"param":"amplitude","factor":0.85},"high":{"param":"amplitude","factor":1.15},"name":"Vpp ${amplitude}"},
             "hz":{"low":{"param":"frequency","factor":0.97},"high":{"param":"frequency","factor":1.03},"name":"Hz ${frequency}"}
          }}]},
        {"id":"repeat","name":"Repeat diagnostics","group":"main",
         "repeat":{"count":3,"index":"trial"},
         "steps":[{"id":"diag","name":"Trial ${trial}","mode":"once","type":"action","handler":"diagnostic"}]},
        {"id":"cleanup","name":"Cleanup","group":"cleanup","type":"action","mode":"once","handler":"ready"}
    ]})JSON");
    QJsonObject expanded;
    QString error;
    assert(TestSequenceLoops::expand(definition,&expanded,&error));
    const auto steps=expanded.value(QStringLiteral("steps")).toArray();
    assert(steps.size()==15); // 1 setup + 6 DC + 4 wave + 3 repeat + 1 cleanup
    assert(steps.at(1).toObject().value(QStringLiteral("parameters")).toObject().value(QStringLiteral("voltage")).toDouble()==5.0);
    assert(steps.at(5).toObject().value(QStringLiteral("parameters")).toObject().value(QStringLiteral("voltage")).toDouble()==4.5);
    assert(steps.at(1).toObject().value(QStringLiteral("limits")).toObject().value(QStringLiteral("low")).toDouble()==4.9);
    assert(steps.at(2).toObject().value(QStringLiteral("limits")).toObject().value(QStringLiteral("high")).toDouble()==5.075);
    assert(steps.at(7).toObject().value(QStringLiteral("measurements")).toObject().value(QStringLiteral("vpp")).toObject().value(QStringLiteral("high")).toDouble()==1.15);
    assert(steps.at(13).toObject().value(QStringLiteral("parameters")).toObject().value(QStringLiteral("trial")).toInt()==2);
    assert(steps.at(14).toObject().value(QStringLiteral("group")).toString()==QStringLiteral("cleanup"));
    // No hidden unbounded, mismatched or unsafe loops.
    auto rejected=[&](QJsonObject definition)->bool{return !TestSequenceLoops::expand(definition,&expanded,&error);};
    {
        QJsonObject bad=definition;QJsonArray s=bad.value(QStringLiteral("steps")).toArray();
        QJsonObject group=s.at(1).toObject();QJsonObject loop=group.value(QStringLiteral("loop")).toObject();
        loop.insert(QStringLiteral("increment"),0);group.insert(QStringLiteral("loop"),loop);s[1]=group;
        bad.insert(QStringLiteral("steps"),s);assert(rejected(bad));
    }
    {
        QJsonObject bad=definition;QJsonArray s=bad.value(QStringLiteral("steps")).toArray();
        QJsonObject group=s.at(1).toObject();group.insert(QStringLiteral("group"),QStringLiteral("cleanup"));s[1]=group;
        bad.insert(QStringLiteral("steps"),s);assert(rejected(bad));
    }
    {
        QJsonObject bad=definition;QJsonArray s=bad.value(QStringLiteral("steps")).toArray();
        QJsonObject group=s.at(1).toObject();QJsonObject loop=group.value(QStringLiteral("loop")).toObject();
        loop.insert(QStringLiteral("increment"),-0.000001);group.insert(QStringLiteral("loop"),loop);s[1]=group;
        bad.insert(QStringLiteral("steps"),s);assert(rejected(bad));
    }
    QJSEngine engine;
    std::atomic_bool stop(false);
    TestRunnerBridge runner(&engine,TestRunnerBridge::UiWriter{},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    QJSValue library=engine.evaluate(QStringLiteral(R"JS(({
      ready:function(){return true;},
      readPsu:function(step){return TestRunner.complete(step.token,step.params.voltage+0.001),undefined;},
      readDmm:function(step){return TestRunner.complete(step.token,step.params.voltage-0.002),undefined;},
      readWave:function(step){return {vpp:step.params.amplitude*1.02,hz:step.params.frequency};},
      diagnostic:function(step){return step.params.trial>=0;}
    }))JS"));
    assert(!library.isError());
    assert(runner.loadDefinition(engine.toScriptValue(definition.toVariantMap()),library));
    assert(runner.definition().value(QStringLiteral("steps")).toList().size()==5); // compact source retained
    assert(runner.run());
    waitFor([&]{return runner.state()!=QStringLiteral("running");});
    assert(runner.state()==QStringLiteral("passed"));
    const auto report=runner.reportData();
    assert(report.value(QStringLiteral("steps")).toList().size()==15);
    assert(report.value(QStringLiteral("measurements")).toList().size()==14); // 6 DC + 4x2 wave
    const auto firstStep=report.value(QStringLiteral("steps")).toList().at(1).toMap();
    assert(firstStep.value(QStringLiteral("templateId")).toString()==QStringLiteral("psu"));
    // A rejected new plan never overwrites the previously validated runnable one.
    QJsonObject broken=definition;QJsonArray entries=broken.value(QStringLiteral("steps")).toArray();
    QJsonObject item=entries.at(1).toObject();QJsonObject badLoop=item.value(QStringLiteral("loop")).toObject();
    badLoop.insert(QStringLiteral("stop"),9);item.insert(QStringLiteral("loop"),badLoop);entries[1]=item;
    broken.insert(QStringLiteral("steps"),entries);
    assert(!runner.loadDefinition(engine.toScriptValue(broken.toVariantMap()),library));
    assert(runner.definition().value(QStringLiteral("steps")).toList().size()==5);
    return 0;
}
