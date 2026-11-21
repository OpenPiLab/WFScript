// v0.2-dev.8.9 AC Network Laboratory — real QJSEngine, no AD3 or GUI required.
#include "PanelProject.h"
#include "PanelSchemaTestEvents.h"
#include "ScriptLibraryBridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJSEngine>
#include <QJSValue>
#include <QDebug>
#include <cmath>
static void need(bool ok,const char* msg){ if(!ok)qFatal("AC Lab native smoke: %s",msg); }
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    registerPanelSchemaTestEvents();
    QFile f(QStringLiteral(":/wfscript/examples/ACNetworkLaboratory.wfpanel"));
    need(f.open(QIODevice::ReadOnly),"Qt resource is embedded");
    const QJsonDocument doc=QJsonDocument::fromJson(f.readAll());
    need(doc.isObject(),"valid Panel JSON");
    PanelProject project;QString error;
    need(PanelProject::fromJson(doc.object(),QStringLiteral("ac-lab.wfpanel"),&project,&error),"production Panel schema accepts lab");
    need(project.pages.size()==10,"ten educational Pages");
    need(project.libraries.size()==6,"all 6 portable libraries embedded");
    need(project.lifecycle.initializeEnabled && project.lifecycle.shutdownEnabled,"explicit startup/shutdown hooks enabled");
    int controls=0;for(const auto& pg:project.pages)controls+=pg.controls.size();
    need(controls==131,"all 131 widgets registered");
    bool foundNotebook=false;
    for(const auto& page:project.pages) for(const auto& value:page.controls) {
        need(value.isObject(),"Panel controls are JSON objects");
        const QJsonObject control=value.toObject();
        if(control.value(QStringLiteral("id")).toString()==QStringLiteral("resultsTable")) {
            foundNotebook=true;
            need(control.value(QStringLiteral("type")).toString()==QStringLiteral("experiment_table"),
                 "notebook uses Experiment Data Table");
            const QJsonObject properties=control.value(QStringLiteral("properties")).toObject();
            need(properties.value(QStringLiteral("columns")).toString().split('|').size()==11,
                 "notebook headers match 11-cell runtime rows");
            need(properties.value(QStringLiteral("maxRows")).toInt()==500,
                 "notebook has 500-row capacity");
        }
    }
    need(foundNotebook,"Notebook table is present");
    need(project.script.contains(QStringLiteral("function onArmClicked")),"explicit ARM handler exists");
    need(project.script.contains(QStringLiteral("function onShutdown")),"W1 shutdown cleanup exists");
    QVariantMap embedded;
    for(const auto& v:project.libraries){
        const auto lib=v.toObject();embedded.insert(lib.value(QStringLiteral("name")).toString(),lib.value(QStringLiteral("source")).toString());
    }
    QJSEngine engine;
    ScriptLibraryBridge host(&engine,QString(),embedded);
    QJSEngine::setObjectOwnership(&host,QJSEngine::CppOwnership);
    need(host.valid(),"embedded library package materializes");
    engine.globalObject().setProperty(QStringLiteral("__wfLibraryHost"),engine.newQObject(&host));
    QJSValue bootstrap=engine.evaluate(ScriptLibraryBridge::bootstrapSource());
    need(!bootstrap.isError(),"Qt native module bootstrap");
    QJSValue outcome=engine.evaluate(QStringLiteral(R"JS(
      (function() {
        var C=modules.load('CircuitLab.mjs');
        var A=modules.load('CaptureAnalysis.mjs');
        var Package=modules.load('SignalPackage.mjs');
        var fc=C.parameters(1000,100).fc;
        var lp=C.transfer('Low-pass',1000,100,fc);
        var hp=C.transfer('High-pass',1000,100,fc);
        if(Math.abs(lp.gain-Math.SQRT1_2)>1e-10 || Math.abs(hp.gain-Math.SQRT1_2)>1e-10)
            throw Error('cutoff gain');
        if(Math.abs(lp.phaseDeg+45)>1e-10 || Math.abs(hp.phaseDeg-45)>1e-10)
            throw Error('cutoff phases');
        var fs=65536,f=1024,N=1024, input=[],output=[];
        var response=C.transfer('Low-pass',1000,100,f);
        for(var i=0;i<N;i++) {
            var a=2*Math.PI*f*i/fs+0.27;
            input.push(Math.sin(a));
            output.push(response.gain*Math.sin(a+response.phaseDeg*Math.PI/180));
        }
        var measured=A.analyzeTwoChannel(input,output,fs,f);
        if(Math.abs(measured.gain-response.gain)>1e-9 ||
           Math.abs(measured.phaseDeg-response.phaseDeg)>1e-9)
            throw Error('two-channel sine/cosine fit failed');
        if(!measured.inputFit || measured.inputFit.rmsResidualV>1e-10 ||
           measured.outputFit.rmsResidualV>1e-10 || measured.inputFit.rSquared<0.999999999)
            throw Error('least-squares diagnostics for perfect sine failed');
        var reconstructed=A.predictSample(measured.inputFit,fs,f,137);
        if(Math.abs(reconstructed-input[137])>1e-10)throw Error('fitted sample reconstruction failed');
        var distorted=input.map(function(v,i){return v+0.06*Math.sin(4*Math.PI*f*i/fs+0.4);});
        var poor=A.fitSinusoid(distorted,fs,f);
        if(!(poor.rmsResidualV>0.02 && poor.rSquared<1))
            throw Error('residual did not flag a harmonic');
        if(C.nominalSampleRatio(32000,1000)!=='32')throw Error('Fraction.js dependency failed');
        var packet=Package.packCapture({ch1:input,ch2:output,tag:'AD3 simulation test'});
        var restored=Package.unpackCapture(packet.encoded);
        if(restored.ch2.length!==N || restored.ch1[77]!==input[77])
            throw Error('Pako/base64 library roundtrip failed');
        return true;
      })()
    )JS"),QStringLiteral("ACNetworkLabSmoke.js"));
    if(outcome.isError())qCritical().noquote()<<outcome.toString()<<outcome.property("stack").toString();
    need(!outcome.isError() && outcome.toBool(),"native QJSEngine modules, RC theory, phase fit, compression");
    qInfo()<<"PASS AC Lab: 10 pages, 131 widgets, six embedded modules, real QJSEngine, RC math, fitted gain/phase, Pako/Base64";
    return 0;
}
