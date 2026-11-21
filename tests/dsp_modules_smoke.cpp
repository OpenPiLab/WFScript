// v0.2-dev.8.6: Resource-backed, real QJSEngine portable-library integration.
// No physical AD3, QWidget, output activation, or network access required.
#include "PanelProject.h"
#include "PanelSchemaTestEvents.h"
#include "ScriptLibraryBridge.h"
#include "ScriptSizeLimits.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJSEngine>
#include <QJSValue>
#include <QDebug>

static void require(bool condition,const char* description) {
    if(!condition)qFatal("DSP Studio native smoke: %s",description);
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    registerPanelSchemaTestEvents();
    QFile panelFile(QStringLiteral(":/wfscript/examples/JavaScriptDSPStudio.wfpanel"));
    require(panelFile.open(QIODevice::ReadOnly),"embedded DSP Studio resource exists");
    const QJsonDocument doc=QJsonDocument::fromJson(panelFile.readAll());
    require(doc.isObject(),"embedded project is valid JSON");
    PanelProject project;
    QString error;
    require(PanelProject::fromJson(doc.object(),QStringLiteral("studio.wfpanel"),&project,&error),"DSP project passes production schema");
    require(project.libraries.size()==5,"five embedded modules, including three genuine third-party libraries");
    QVariantMap embedded;
    for(const auto& lib:project.libraries) {
        const auto obj=lib.toObject();
        embedded.insert(obj.value(QStringLiteral("name")).toString(),obj.value(QStringLiteral("source")).toString());
    }
    QJSEngine engine;
    ScriptLibraryBridge bridge(&engine,QString(),embedded);
    QJSEngine::setObjectOwnership(&bridge,QJSEngine::CppOwnership);
    require(bridge.valid(),"native bridge materializes all embedded libraries");
    engine.globalObject().setProperty(QStringLiteral("__wfLibraryHost"),engine.newQObject(&bridge));
    require(!engine.evaluate(ScriptLibraryBridge::bootstrapSource()).isError(),"load original module bootstrap");
    const QJSValue value=engine.evaluate(QStringLiteral(R"JS(
        (function() {
            var DSP=modules.load('DSPCore.mjs');
            var Package=modules.load('SignalPackage.mjs');
            if(!DSP || !Package)throw Error('imports failed');
            var wave=DSP.generateSignal({samples:1024,sampleRateHz:16384,toneHz:512,
                   shape:'Sine',harmonicsPct:0,noisePct:0,seed:11});
            var spec=DSP.fftAmplitude(wave.values,16384);
            if(spec.peakHz!==512 || Math.abs(spec.peakAmplitude-1)>0.01)throw Error('FFT peak mismatch');
            var filtered=DSP.biquadLowpass(wave.values,16384,400);
            if(!(DSP.rms(filtered.samples)<DSP.rms(wave.values)))throw Error('filter mismatch');
            var grid=Package.samplesPerCycle(16384,440);
            if(grid.fraction!=='2048/55')throw Error('Fraction.js dependency mismatch');
            var archive=Package.packCapture({wave:wave.values,filtered:filtered.samples});
            var copy=Package.unpackCapture(archive.encoded);
            if(copy.wave.length!==1024 || copy.wave[10]!==wave.values[10] ||
               copy.filtered[23]!==filtered.samples[23])throw Error('Pako/Base64 round trip mismatch');
            return true;
        })()
    )JS"),QStringLiteral("DSPStudioSmoke.js"));
    if(value.isError())qCritical().noquote()<<value.toString()<<value.property("stack").toString();
    require(!value.isError() && value.toBool(),"genuine QJSEngine imported-library calculations pass");

    // Larger than v8.5's 1 MiB cap, still within new bounded limit.
    const QString large=QStringLiteral("/*") + QString(1300000,QLatin1Char(' ')) + QStringLiteral("*/\nexport const answer = 42;\n");
    const QVariantMap largeSources{{QStringLiteral("large.mjs"),large}};
    ScriptLibraryBridge largeBridge(&engine,QString(),largeSources);
    require(largeBridge.valid(),"module larger than former 1 MiB limit materializes");
    const QVariantMap tooLarge{{QStringLiteral("large.mjs"),QString(ScriptSizeLimits::LibraryFileBytes+1,QLatin1Char('x'))}};
    ScriptLibraryBridge blocked(&engine,QString(),tooLarge);
    require(!blocked.valid(),"8 MiB per-library cap still enforced");
    qInfo()<<"PASS: DSP Studio real QJSEngine modules, math/FFT/filter/compression, bounded larger imports";
    return 0;
}
