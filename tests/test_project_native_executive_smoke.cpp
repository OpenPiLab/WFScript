// v5.6.63 — native numeric evaluation and project configuration ownership.
// Run with -DWFSCRIPT_BUILD_TESTS=ON on Windows Qt Creator.
#include "TestRunnerBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJSValue>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
static QJSValue script(QJSEngine& js,const char* source) {
    const QJSValue result=js.evaluate(QString::fromUtf8(source));
    if(result.isError())qFatal("JS error: %s",qPrintable(result.toString()));
    return result;
}
int main(int argc,char** argv) {
    QCoreApplication application(argc,argv);
    QJSEngine js;
    std::atomic_bool stop(false);
    TestRunnerBridge runner(&js,[](const QString&,const QVariant&){return true;},&stop);
    QJSEngine::setObjectOwnership(&runner,QJSEngine::CppOwnership);
    js.globalObject().setProperty("TestRunner",js.newQObject(&runner));
    assert(script(js,R"JS(
        var measured=3.301;
        var functions={read:function(){return measured;},cleanup:function(){return true;}};
        var definition={schemaVersion:1,id:'native-numeric',title:'Native numeric',
          config:{totalTimeoutMs:15000,stopOnFailure:true},
          steps:[{id:'voltage',name:'Voltage',type:'numeric',mode:'once',
                  handler:'read',limits:{low:3.25,high:3.35},unit:'V'},
                 {id:'cleanup',name:'Restore',group:'cleanup',type:'action',
                  mode:'once',handler:'cleanup'}]};
        TestRunner.loadDefinition(definition,functions);
    )JS").toBool());
    runner.setProjectManaged(true);
    assert(runner.summary().value("totalTimeoutMs").toInt()==15000);
    assert(!script(js,"TestRunner.configure({totalTimeoutMs:5000})").toBool());
    assert(!script(js,"TestRunner.loadDefinition(definition,functions)").toBool());
    assert(!script(js,"TestRunner.add({id:'bad',name:'bad'})").toBool());
    assert(script(js,"TestRunner.run()").toBool());
    assert(runner.reportData().value("state").toString()=="passed");
    const auto measurements=runner.reportData().value("measurements").toList();
    assert(measurements.size()==1);
    assert(measurements.first().toMap().value("value").toDouble()==3.301);
    assert(measurements.first().toMap().value("status").toString()=="PASS");
    assert(script(js,"TestRunner.reset()").toBool());
    assert(runner.summary().value("totalTimeoutMs").toInt()==15000);
    // A failing numeric measurement automatically generates a FAIL from C++.
    // Reuse the same native bound function but change its return value.
    assert(script(js,"measured = 47.5; true").toBool());
    // A synchronous FAIL is an executed test with a false run() return,
    // not a failure to start. Preserve the native FAIL and Cleanup checks.
    assert(!script(js,"TestRunner.run()").toBool());
    assert(runner.state()=="failed");
    assert(runner.lastError().isEmpty());
    const auto failed=runner.reportData().value("measurements").toList();
    assert(failed.size()==1 && failed.first().toMap().value("status").toString()=="FAIL");
    assert(runner.reportData().value("steps").toList().last().toMap().value("status").toString()=="done");
    runner.shutdown();
    return 0;
}
