// RC1.5.14: native QJSEngine IF / ELSE, variable, and loop-condition smoke.
#include "TestRunnerBridge.h"
#include "TestSequenceConditions.h"
#include "TestSequenceLoops.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJSEngine>
#include <QJsonDocument>
#include <QTimer>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QJSValue js(QJSEngine& engine,const QString& code) {
    const QJSValue v=engine.evaluate(code,QStringLiteral("conditional-smoke"));
    if(v.isError())qFatal("JavaScript error: %s",qPrintable(v.toString()));
    return v;
}
static void finishRun(QCoreApplication& app,TestRunnerBridge& runner) {
    Q_UNUSED(app)
    QElapsedTimer clock;clock.start();
    while(runner.state()==QStringLiteral("running") && clock.elapsed()<3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents,15);
    assert(runner.state()!=QStringLiteral("running"));
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    std::atomic_bool interrupted{false};
    TestRunnerBridge runner(&engine,{},&interrupted);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("TestRunner"),engine.newQObject(&runner));
    assert(js(engine,QStringLiteral(R"JS(
      var visited=[];
      var handlers={
        setup:function() {visited.push('setup'); if(!TestRunner.setVariable('temperature',48.5))throw Error(TestRunner.lastError);return true;},
        read:function(step){visited.push('read:'+step.params.voltage);return step.params.voltage;},
        yes:function(step){visited.push('then:'+step.params.voltage);return true;},
        no:function(step){visited.push('else:'+step.params.voltage);return true;},
        cleanup:function(){visited.push('cleanup');return true;}
      };
      var doc={schemaVersion:1,id:'conditions',title:'IF and ELSE test',
        variables:[{name:'temperature',type:'number',initial:25,description:'Reading from setup'}],
        config:{stopOnFailure:true,totalTimeoutMs:1500},steps:[
          {id:'setup',name:'Setup',group:'setup',type:'action',mode:'once',handler:'setup'},
          {id:'temp_if',name:'IF temperature > 40',group:'main',
           condition:{source:'variable',key:'temperature',operator:'>',value:40},steps:[
             {id:'temp_true',name:'THEN',type:'action',mode:'once',handler:'yes'},
             {id:'temp_else',name:'ELSE',otherwise:true,steps:[
                {id:'temp_false',name:'FALSE',type:'action',mode:'once',handler:'no'}]}]},
          {id:'sweep',name:'Sweep',group:'main',
           loop:{parameter:'voltage',values:[1,2]},steps:[
             {id:'read',name:'Read ${voltage}',type:'numeric',mode:'once',handler:'read',limits:{low:0,high:3}},
             {id:'check',name:'IF voltage > 1.5',condition:{source:'measurement',step:'read',operator:'>',value:1.5},steps:[
                {id:'then_read',name:'THEN ${voltage}',type:'action',mode:'once',handler:'yes'},
                {id:'else_read',name:'ELSE',otherwise:true,steps:[
                   {id:'else_action',name:'ELSE ${voltage}',type:'action',mode:'once',handler:'no'}]}]}
           ]},
          {id:'cleanup',name:'Cleanup',group:'cleanup',type:'action',mode:'once',handler:'cleanup'}
        ]};
      TestRunner.loadDefinition(doc,handlers);
    )JS")).toBool());
    assert(runner.run());finishRun(app,runner);
    assert(runner.state()==QStringLiteral("passed"));
    const QString trace=js(engine,QStringLiteral("visited.join(',')")).toString();
    assert(trace.contains(QStringLiteral("read:1")) && trace.contains(QStringLiteral("read:2")));
    assert(trace.contains(QStringLiteral("else:1")) && trace.contains(QStringLiteral("then:2")));
    assert(!trace.contains(QStringLiteral("then:1")) && !trace.contains(QStringLiteral("else:2")));
    const QVariantList steps=runner.reportData().value(QStringLiteral("steps")).toList();
    int actuallySkipped=0;
    for(const QVariant& entry:steps)
        if(entry.toMap().value(QStringLiteral("status")).toString()==QStringLiteral("skipped"))
            ++actuallySkipped;
    assert(actuallySkipped>=2);
    assert(runner.summary().value(QStringLiteral("skippedSteps")).toInt()==actuallySkipped);
    int branchDecisions=0;
    for(const QVariant& entry:steps) {
        const QVariantMap record=entry.toMap();
        if(record.value(QStringLiteral("mode"))==QStringLiteral("condition")){
            ++branchDecisions;assert(record.contains(QStringLiteral("result")));
        }
    }
    assert(branchDecisions==3);
    // Unknown at runtime (declared but not initialized) is ERROR, not FALSE.
    assert(js(engine,QStringLiteral(R"JS(
       doc.variables=[{name:'missing',type:'number'}];
       doc.steps=[{id:'if',name:'Missing variable',group:'main',
         condition:{source:'variable',key:'missing',operator:'>',value:1},steps:[
         {id:'true',name:'THEN',type:'action',mode:'once',handler:'yes'}]},
         {id:'cleanup',name:'Cleanup',group:'cleanup',type:'action',mode:'once',handler:'cleanup'}];
       TestRunner.loadDefinition(doc,handlers);
    )JS")).toBool());
    // The missing variable is detected synchronously, so run() returns false;
    // the error state and cleanup execution remain the actual contract.
    assert(!runner.run());finishRun(app,runner);
    assert(runner.state()==QStringLiteral("error"));
    assert(js(engine,QStringLiteral("visited[visited.length-1]")).toString()==QStringLiteral("cleanup"));
    runner.shutdown();return 0;
}
