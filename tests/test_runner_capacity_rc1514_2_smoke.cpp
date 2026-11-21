// RC1.5.14.2 — verifies >100 expanded steps and >100 numeric results.
// Also reproduces the prior 98+3 overflow without instruments.
#include "TestRunnerBridge.h"
#include "TestSequenceLoops.h"
#include "TestSequenceCapacity.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine js;
    std::atomic_bool stop(false);
    int streamedMeasurements=0,virtualSequenceCount=0;
    int progressTotal=0, progressCompleted=0,virtualDefinitions=0;
    TestRunnerBridge runner(&js,[&](const QString& id,const QVariant& payload){
        if(id==QStringLiteral("table")) {
            const QVariantMap operation=payload.toMap();
            if(operation.contains(QStringLiteral("append"))) {
                const QVariant value=operation.value(QStringLiteral("append"));
                streamedMeasurements+=value.metaType().id()==QMetaType::QVariantList?
                    value.toList().size():1;
            }
        }
        if(id==QStringLiteral("seq")) {
            const QVariantMap op=payload.toMap();
            if(op.contains(QStringLiteral("virtualSteps"))) {
                ++virtualDefinitions;
                virtualSequenceCount=op.value(QStringLiteral("virtualSteps")).toList().size();
            }
            if(op.contains(QStringLiteral("progress"))) {
                const auto progress=op.value(QStringLiteral("progress")).toMap();
                progressTotal=progress.value(QStringLiteral("total")).toInt();
                progressCompleted=progress.value(QStringLiteral("completed")).toInt();
            }
        }
        return true;
    },&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    js.globalObject().setProperty(QStringLiteral("TestRunner"),js.newQObject(&runner));
    QString definition=QStringLiteral(R"JS(
      var handlers={
        noop:function(){return true;},
        measure:function(step){return {a:step.params.voltage,b:step.params.voltage,c:step.params.voltage};}
      };
      var doc={schemaVersion:1,id:'capacity-smoke',title:'Capacity smoke',
        config:{stopOnFailure:true,totalTimeoutMs:30000,sequenceId:'seq',tableId:'table'},steps:[
        {id:'setup',name:'Setup',group:'setup',type:'action',mode:'once',handler:'noop'},
        {id:'sweep',name:'Sweep',group:'main',
         loop:{parameter:'voltage',start:1,stop:101,increment:1},
         steps:[{id:'measure',name:'Three values at ${voltage}',mode:'once',type:'numeric',handler:'measure',
           measurements:{a:{name:'A',low:0,high:200},b:{name:'B',low:0,high:200},c:{name:'C',low:0,high:200}}}]},
        {id:'cleanup',name:'Cleanup',group:'cleanup',type:'action',mode:'once',handler:'noop'}]};
      TestRunner.loadDefinition(doc,handlers);
    )JS");
    // One sweep axis with 101 points is now valid (previously rejected).
    QString valid=definition;
    valid.replace(QStringLiteral("stop:101,increment:1"),QStringLiteral("stop:110,increment:1"));
    const QJSValue loaded=js.evaluate(valid,QStringLiteral("capacity-smoke.js"));
    assert(!loaded.isError() && loaded.toBool());
    assert(runner.run());
    assert(runner.state()==QStringLiteral("passed"));
    assert(runner.resultsData().size()==330); // 110 points x three results
    assert(runner.summary().value(QStringLiteral("totalSteps")).toInt()==112);
    assert(runner.summary().value(QStringLiteral("completedSteps")).toInt()==112);
    assert(progressTotal==112 && progressCompleted==112);
    assert(streamedMeasurements==330 && virtualSequenceCount==112);
    assert(virtualDefinitions>=1);
    const QVariantList steps=runner.reportData().value(QStringLiteral("steps")).toList();
    assert(steps.size()==112 && steps.last().toMap().value(QStringLiteral("status"))==QStringLiteral("done"));

    // Exact Rigol failure pattern: 31 DC points * 2 results = 62,
    // then 12 waveform captures * 3 = 98, then one more creates 101.
    // This previously produced a terminal ERROR with 98/98 PASS + skipped steps.
    const QString reproduce=QStringLiteral(R"JS(
      var h={noop:function(){return true;},
        dc:function(){return {psu:3,dmm:3};},
        wave:function(){return {vpp:1,hz:1000,shape:0.35};}};
      var d={schemaVersion:1,id:'rigol-overflow',title:'98 plus 3',
        config:{stopOnFailure:true,totalTimeoutMs:30000},steps:[
        {id:'setup',name:'Setup',group:'setup',type:'action',mode:'once',handler:'noop'},
        {id:'dcgroup',name:'DC',group:'main',loop:{parameter:'v',start:1,stop:31,increment:1},steps:[
          {id:'dc',name:'DC',type:'numeric',mode:'once',handler:'dc',measurements:{
            psu:{low:0,high:5},dmm:{low:0,high:5}}}]},
        {id:'wavegroup',name:'Waveforms',group:'main',loop:{parameter:'n',start:1,stop:13,increment:1},steps:[
          {id:'wave',name:'Waveform',type:'numeric',mode:'once',handler:'wave',measurements:{
            vpp:{low:0,high:2},hz:{low:970,high:1030},shape:{low:0.3,high:0.4}}}]},
        {id:'cleanup',name:'Cleanup',group:'cleanup',type:'action',mode:'once',handler:'noop'}]};
      TestRunner.loadDefinition(d,h);
    )JS");
    const QJSValue loadedRepro=js.evaluate(reproduce,QStringLiteral("98-plus-3.js"));
    assert(!loadedRepro.isError() && loadedRepro.toBool());
    assert(runner.run());
    assert(runner.state()==QStringLiteral("passed"));
    assert(runner.resultsData().size()==101);
    assert(runner.summary().value(QStringLiteral("skippedSteps")).toInt()==0);
    assert(runner.reportData().value(QStringLiteral("measurements")).toList().size()==101);

    // Hard upper bound remains explicit to prevent memory exhaustion on bad plans.
    QJsonArray rows;
    rows.append(QJsonObject{{"id","setup"},{"name","Setup"},{"group","setup"}});
    rows.append(QJsonObject{{"id","oversweep"},{"name","10k grid"},{"group","main"},
        {"loop",QJsonObject{{"axes",QJsonArray{
            QJsonObject{{"parameter","x"},{"start",1},{"stop",100},{"increment",1}},
            QJsonObject{{"parameter","y"},{"start",1},{"stop",100},{"increment",1}}
        }}}},
        {"steps",QJsonArray{QJsonObject{{"id","cell"},{"name","Cell"}}}}});
    QJsonObject tooMany{{"steps",rows}};
    QJsonObject output;QString error;
    assert(!TestSequenceLoops::expand(tooMany,&output,&error));
    assert(error.contains(QStringLiteral("10000")));
    // 10,000 points along a SINGLE axis (no artificial 100-point limit).
    QJsonObject tenThousand{{"steps",QJsonArray{
        QJsonObject{{"id","sweep"},{"name","Long axis"},{"group","main"},
            {"loop",QJsonObject{{"parameter","hz"},{"start",1},{"stop",10000},{"increment",1}}},
            {"steps",QJsonArray{QJsonObject{{"id","cell"},{"name","Point ${hz}"}}}}}
    }}};
    error.clear();
    assert(TestSequenceLoops::expand(tenThousand,&output,&error));
    assert(output.value("steps").toArray().size()==10000);
    QJsonObject tooLong=tenThousand;
    QJsonArray longRows=tooLong.value("steps").toArray();
    QJsonObject sweep=longRows.at(0).toObject();
    QJsonObject axis=sweep.value("loop").toObject();axis.insert("stop",10001);
    sweep.insert("loop",axis);longRows.replace(0,sweep);tooLong.insert("steps",longRows);
    error.clear();assert(!TestSequenceLoops::expand(tooLong,&output,&error));
    assert(error.contains("10000"));
    // Repeat has the same 10,000 bound, still subject to total steps.
    QJsonObject repeatDoc{{"steps",QJsonArray{
        QJsonObject{{"id","repeat"},{"name","Ten thousand iterations"},{"group","main"},
            {"repeat",QJsonObject{{"count",10000},{"index","iteration"}}},
            {"steps",QJsonArray{QJsonObject{{"id","leaf"},{"name","Repeat"}}}}}
    }}};
    error.clear();assert(TestSequenceLoops::expand(repeatDoc,&output,&error));
    assert(output.value("steps").toArray().size()==10000);
    runner.shutdown();
    return 0;
}
