// WFScript v5.2 simulation-only smoke test. No Qt Widgets or AD3 required.
// Build with -DWFSCRIPT_BUILD_TESTS=ON, then run ctest --output-on-failure.
#include "WaveFormsBindings.h"
#include "ScriptHelpers.h"
#include "CompletionCatalog.h"
#include "ApiKnowledge.h"
#include <QMetaMethod>
#include <QMetaProperty>
#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QDebug>
#include <atomic>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    std::atomic_bool stop{false};
    MockInstrumentState mock;
    ScopeBridge scope(nullptr, &mock, &stop);
    WavegenBridge wavegen(nullptr, &mock);
    DeviceBridge device(nullptr, true);
    ScriptUtilities utils(&stop);
    QJSEngine engine;

    // Match the ownership guarantees in both production JavaScript runtimes:
    // nested QObject parameters are member objects, not JavaScript allocations.
    auto* trg = static_cast<ScopeTrigger*>(scope.trigger());
    auto* st = static_cast<ScopeTime*>(scope.time());
    auto* c1 = static_cast<ScopeChannel*>(scope.channel1());
    auto* c2 = static_cast<ScopeChannel*>(scope.channel2());
    auto* w1 = static_cast<WavegenChannel*>(wavegen.channel1());
    auto* w2 = static_cast<WavegenChannel*>(wavegen.channel2());
    auto* s1 = static_cast<WavegenSimple*>(w1->simple());
    auto* s2 = static_cast<WavegenSimple*>(w2->simple());
    QObject* owned[] = {&scope, &wavegen, &device, &utils,
        trg, trg->trigger(), trg->type(), trg->source(), trg->condition(),
        trg->level(), trg->hysteresis(),trg->lengthCondition(),trg->length(),trg->holdOff(),trg->filter(), st, st->rate(), st->samples(), st->position(),st->mode(),st->recordLength(),
        c1, c1->range(),c1->offset(),c1->attenuation(),c1->sampleMode(),c2, c2->range(),c2->offset(),c2->attenuation(),c2->sampleMode(), w1, w2, w1->mode(), w2->mode(),w1->custom(),w2->custom(),w1->burst(),w2->burst(),
        s1, s1->type(), s1->frequency(), s1->amplitude(), s1->offset(), s1->period(),s1->symmetry(),s1->phase(),
        s2, s2->type(), s2->frequency(), s2->amplitude(), s2->offset(), s2->period(),s2->symmetry(),s2->phase()};
    // v5.6.5: auto-compare the actual Qt-exposed API against the catalog.
    // This makes new Q_PROPERTY / Q_INVOKABLE methods show up as test failures
    // until the editor metadata has been updated. No special 'current' list.
    auto checkInventory=[&](const QObject* object,const QString& apiPath)->bool {
        if(!object) {qCritical()<<"Null inventory object"<<apiPath;return false;}
        const auto* meta=object->metaObject();
        const auto entries=CompletionCatalog::entries(apiPath);
        auto has=[&](const QString& name) {
            for(const auto& e:entries) if(e.name==name) return true;
            return false;
        };
        for(int i=meta->propertyOffset();i<meta->propertyCount();++i) {
            const QString name=QString::fromLatin1(meta->property(i).name());
            if(!has(name)) {qCritical()<<"API catalog missing property"<<apiPath<<name;return false;}
        }
        for(int i=meta->methodOffset();i<meta->methodCount();++i) {
            const auto method=meta->method(i);
            if(method.methodType()==QMetaMethod::Signal) continue;
            const QString name=QString::fromLatin1(method.name());
            if(!has(name)) {qCritical()<<"API catalog missing method"<<apiPath<<name;return false;}
        }
        return true;
    };
    const QVector<QPair<const QObject*,QString>> exposed={
        {&scope,"Scope1"}, {st,"Scope1.Time"}, {trg,"Scope1.Trigger"},
        {c1,"Scope1.Channel1"}, {c1->range(),"Scope1.Channel1.Range"},
        {c1->sampleMode(),"Scope1.Channel1.SampleMode"},
        {st->mode(),"Scope1.Time.Mode"}, {st->rate(),"Scope1.Time.Rate"},
        {&wavegen,"Wavegen1"}, {w1,"Wavegen1.Channel1"},
        {w1->mode(),"Wavegen1.Channel1.Mode"},
        {s1,"Wavegen1.Channel1.Simple"},
        {s1->frequency(),"Wavegen1.Channel1.Simple.Frequency"},
        {w1->custom(),"Wavegen1.Channel1.Custom"},
        {w1->burst(),"Wavegen1.Channel1.Burst"},
        {w1->modulation(),"Wavegen1.Channel1.Modulation"},
        {static_cast<WavegenModulation*>(w1->modulation())->fm(),"Wavegen1.Channel1.Modulation.FM"},
        {w1->sweep(),"Wavegen1.Channel1.Sweep"},
        {static_cast<WavegenSweep*>(w1->sweep())->frequency(),"Wavegen1.Channel1.Sweep.Frequency"},
        {w1->trigger(),"Wavegen1.Channel1.Trigger"},
        {w1->playSettings(),"Wavegen1.Channel1.Play"},
        {&device,"Device"}
    };
    for(const auto& pair:exposed)
        if(!checkInventory(pair.first,pair.second)) return 1;
    qInfo()<<"PASS: runtime-exposed Qt properties/methods match the editor API catalog";
    for (QObject* obj : owned) QJSEngine::setObjectOwnership(obj, QJSEngine::CppOwnership);

    for (auto* ch : {w1,w2}) {
        auto* burst=static_cast<WavegenBurst*>(ch->burst());
        for (auto* obj : {burst->run(),burst->waitTime(),burst->repeat(),burst->master()})
            QJSEngine::setObjectOwnership(obj, QJSEngine::CppOwnership);
    }
    // v5.5 Release B: ensure nested modulation/sweep/trigger/play parameters
    // remain C++ owned when visible through QJSEngine.
    for(auto* ch : {w1,w2}) {
        auto* mod=static_cast<WavegenModulation*>(ch->modulation());
        auto* sw=static_cast<WavegenSweep*>(ch->sweep());
        auto* trig=static_cast<WavegenTrigger*>(ch->trigger());
        auto* play=static_cast<WavegenPlay*>(ch->playSettings());
        // A braced auto* initializer list cannot deduce a common type from
        // different QObject subclasses (GCC/MinGW C++17).  Give the array
        // an explicit QObject* element type; all nested nodes are C++ owned.
        QObject* nestedObjects[] = {
            mod, mod->fm(), mod->am(),
            sw, sw->frequency(), sw->amplitude(),
            trig, trig->source(), trig->idle(),
            play, play->rate()
        };
        for (QObject* object : nestedObjects)
            QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
        for(QObject* v : {mod->fm(),mod->am()}) {
            auto* node=static_cast<WavegenModulationNode*>(v);
            for(auto* x : {node->type(),node->frequency(),node->depth()})
                QJSEngine::setObjectOwnership(x,QJSEngine::CppOwnership);
        }
        for(QObject* v : {sw->frequency(),sw->amplitude()}) {
            auto* ax=static_cast<WavegenSweepAxis*>(v);
            for(auto* x : {ax->start(),ax->stop(),ax->time()})
                QJSEngine::setObjectOwnership(x,QJSEngine::CppOwnership);
        }
    }
    auto global = engine.globalObject();
    global.setProperty("Scope1", engine.newQObject(&scope));
    global.setProperty("Scope", global.property("Scope1"));
    global.setProperty("Wavegen1", engine.newQObject(&wavegen));
    global.setProperty("Wavegen", global.property("Wavegen1"));
    global.setProperty("Device", engine.newQObject(&device));

    const QJSValue helpers = engine.evaluate(ScriptHelpers::source(), "<helpers>");
    if (helpers.isError()) {
        qCritical().noquote() << "Helper bootstrap failed:" << helpers.toString();
        return 1;
    }
    const QString test = QStringLiteral(R"JS(
        function check(value, message) { if (!value) throw Error(message); }
        check(Scope === Scope1 && Wavegen === Wavegen1, "alias identity");
        check(Device.isConnected() && Device.SN === 'DEMO', "device stub");
        check(Math.abs(log10(1000)-3) < 1e-12 &&
              Math.abs(acos(0)-PI/2)<1e-12 &&
              Math.abs(atan2(1,0)-PI/2)<1e-12, "global math helpers");
        check(Scope1.Trigger.Condition.preset.includes('Falling'), "enum presets");
        Scope.Time.Samples.value = 8192;
        Scope.Time.Rate.value = 1000000;
        Scope.Channel1.Range.value = 2;
        Wavegen.Channel1.enable = true;
        Wavegen.Channel1.Simple.Type.text = 'Sine';
        Wavegen.Channel1.Simple.Frequency.value = 1000;
        Wavegen.Channel1.Simple.Amplitude.value = 1.0;
        Wavegen.Channel1.Simple.Offset.value = 0;
        check(Wavegen.run(), "wavegen run");
        check(Scope.config(), "scope config");
        check(Scope.Time.Samples.real === 8192, "simulation sample count real");
        check(Scope.Time.Rate.real === 1000000, "simulation rate real");
        check(Scope.Channel1.Range.real === 2, "independent CH1 range");
        check(Scope.single() && Scope.wait(1), "acquisition");
        var a = Scope.Channel1.getData();
        check(a.length === 8192 && Scope.Channel1.data.length === 8192, "array data");
        var rms = Scope.Channel1.measure('RMS');
        var vpp = Scope.Channel1.measure('Peak2Peak');
        var amp = Scope.Channel1.measure('Amplitude');
        var frequency = Scope.Channel1.measure('Frequency');
        var period = Scope.Channel1.measure('Period');
        check(Math.abs(rms-Math.SQRT1_2)<0.01, "RMS");
        check(Math.abs(vpp-2.0)<0.01 && Math.abs(amp-1.0)<0.01, "Peak2Peak / Amplitude");
        check(Math.abs(frequency-1000)<0.1 && Math.abs(period-0.001)<1e-7, "Frequency / Period");
        check(isNaN(Scope.Channel1.measure('not-implemented')), "unknown measurement NaN");
        var firstSnapshot = a.slice(0, 3);
        Wavegen.Channel1.Simple.Offset.value = 0.5;
        check(Wavegen.config(), "update waveform while running");
        check(Scope.single() && Scope.wait(1), "second acquisition");
        check(Math.abs(Scope.Channel1.getData()[0]-0.5)<0.01, "updated capture");
        check(a[0] === firstSnapshot[0], "stable previous JS snapshot");
        Scope.Time.Rate.value = 0;
        check(!Scope.config() && Scope.lastError.length>0, "reject zero sample rate");
        Scope.Time.Rate.value = 1000000;
        Wavegen.stop();
        true;
    )JS");
    const QJSValue result = engine.evaluate(test, "<simulation-test>");
    if (result.isError() || !result.toBool()) {
        qCritical().noquote() << "Simulation smoke test failed:" << result.toString()
                              << "line" << result.property("lineNumber").toInt();
        return 1;
    }
    // v5.3 extended API checks execute the actual Qt meta-object bridge.
    const QJSValue extended = engine.evaluate(QStringLiteral(R"JS(
        function requireOk(value,message) {if (!value) throw Error(message);}
        Scope.Time.TriggerPositionPercent.value=35;
        Scope.Channel1.Range.value=2;
        Scope.Channel2.Range.value=5;
        Scope.Channel1.Offset.value=0.2;
        Scope.Channel2.Offset.value=-0.1;
        Scope.Channel1.Attenuation.value=1;
        Scope.Channel2.Attenuation.value=1;
        requireOk(Scope.config(), 'extended scope config');
        requireOk(Scope.Time.TriggerPositionPercent.real===35, 'trigger position readback');
        requireOk(Scope.Channel1.Range.real===2 && Scope.Channel2.Range.real===5, 'two distinct ranges');
        requireOk(Scope.Channel1.Offset.real===0.2, 'offset readback');
        Wavegen.Channel1.enable=true;
        Wavegen.Channel2.enable=true;
        Wavegen.Channel1.Simple.Type.text='Square';
        Wavegen.Channel1.Simple.Amplitude.value=1;
        Wavegen.Channel1.Simple.Offset.value=0;
        Wavegen.Channel1.Simple.Symmetry.value=30;
        Wavegen.Channel1.Simple.Phase.value=0;
        Wavegen.Channel2.Simple.Type.text='Sine';
        Wavegen.Channel2.Simple.Frequency.value=1800;
        Wavegen.Channel2.Simple.Amplitude.value=0.6;
        Wavegen.Channel2.Simple.Offset.value=0;
        requireOk(Wavegen.run(),'two-channel generator');
        requireOk(Scope.single() && Scope.wait(1),'two-channel scope');
        requireOk(Math.abs(Scope.Channel1.measure('DutyCycle')-30)<3,'30 percent duty');
        requireOk(Math.abs(Scope.Channel2.measure('Frequency')-1800)<2,'W2->CH2 frequency');
        requireOk(Math.abs(Scope.Channel2.measure('RMS')-0.6/Math.sqrt(2))<0.03,'W2->CH2 amplitude');
        requireOk(Math.abs(Scope.Channel1.measure('Peak2Peak')-2)<0.01,'square peak to peak');
        requireOk(Scope.Channel1.timeOfIndex(100)===0.0001,'sample time');
        requireOk(Scope.Channel1.sample(0)===Scope.Channel1.getData()[0],'sample indexed getter');
        requireOk(Scope.Time.taken.length>10,'capture timestamp');
        var custom=[];
        for(var i=0;i<128;++i) custom.push(Math.sin(2*Math.PI*i/128));
        requireOk(Wavegen.Channel1.Custom.set(custom),'custom set');
        requireOk(!Wavegen.Channel1.Custom.set([1.1,-0.1]),'custom out-of-range rejection');
        requireOk(Wavegen.Channel1.Custom.length===128,'custom length');
        Wavegen.Channel1.Simple.Type.text='Custom';
        requireOk(Wavegen.config(), 'custom config');
        requireOk(Scope.single() && Scope.wait(1), 'custom capture');
        requireOk(Math.abs(Scope.Channel1.measure('RMS')-Math.SQRT1_2)<0.02, 'custom RMS');
        // Reject invalid burst settings rather than silently truncating fractions.
        Wavegen.Channel1.Burst.Repeat.value=1.5;
        requireOk(!Wavegen.config() && Wavegen.lastError.length>0,'reject noninteger burst repeat');
        Wavegen.Channel1.Burst.Repeat.value=0;
        Wavegen.stop();
        true;
    )JS"), "<v5.3-test>");
    if (extended.isError() || !extended.toBool()) {
        qCritical().noquote() << "v5.3 extended API failed:" << extended.toString()
                              << "line" << extended.property("lineNumber").toInt();
        return 1;
    }
    const QJSValue releaseA = engine.evaluate(QStringLiteral(R"JS(
        function must(c,m) {if(!c) throw Error(m);}
        var caps=Scope.capabilities();
        must(caps.recordSupported && caps.screenSupported && caps.shiftSupported,'simulation capability report');
        Scope.Time.Rate.value=100000;
        Scope.Time.Samples.value=2048;
        Scope.Time.Mode.text='Record';
        Scope.Time.RecordLength.value=0.05;
        Scope.Trigger.Trigger.text='None';
        Wavegen.Channel1.enable=true;
        Wavegen.Channel1.Simple.Type.text='Sine';
        Wavegen.Channel1.Simple.Frequency.value=1000;
        Wavegen.Channel1.Simple.Amplitude.value=1;
        must(Wavegen.run(),'generator for record');
        must(Scope.single() && Scope.wait(2),'record capture');
        must(Scope.Channel1.getData().length===5000, 'record sample count');
        must(Scope.Time.Samples.real===5000,'record real count');
        must(Scope.recordLostSamples===0 && Scope.recordCorruptSamples===0, 'record status');
        must(Math.abs(Scope.Channel1.measure('RMS')-Math.SQRT1_2)<0.01,'record amplitude');
        Scope.Time.Mode.text='Screen';
        must(!Scope.single(),'screen requires run');
        must(Scope.run() && Scope.wait(2) && Scope.wait(2),'screen snapshots');
        must(Scope.Channel1.getData().length===2048, 'screen buffer');
        Scope.stop();
        Scope.Time.Mode.text='Shift';
        must(Scope.run() && Scope.wait(2),'shift snapshot');
        Scope.stop();
        Scope.Time.Mode.text='Repeated';
        Scope.Channel1.SampleMode.text='Average';
        Scope.Channel2.SampleMode.text='Min/Max';
        Scope.Trigger.Type.text='Pulse';
        Scope.Trigger.Length.value=0.00025;
        Scope.Trigger.LCondition.value=2;
        Scope.Trigger.HoldOff.value=0.00001;
        Scope.Trigger.Filter.text='Average';
        must(Scope.config(),'advanced trigger/filter config');
        must(Scope.Trigger.Type.text==='Pulse' && Scope.Channel1.SampleMode.text==='Average',
             'advanced parameter readback');
        Scope.Trigger.Type.text='Edge';
        Scope.Trigger.HoldOff.value=0;
        Scope.Trigger.Filter.text='Decimate';
        Scope.Channel1.SampleMode.text='Decimate';
        Scope.Channel2.SampleMode.text='Decimate';
        Wavegen.stop();
        true;
    )JS"), "<release-A-simulation>");
    if (releaseA.isError() || !releaseA.toBool()) {
        qCritical().noquote() << "Release A simulation API failed:" << releaseA.toString()
                              << "line" << releaseA.property("lineNumber").toInt();
        return 1;
    }
    // Release C: actual QObject methods, not a mock of JS result structures.
    stop.store(false);
    const QJSValue releaseC=engine.evaluate(QStringLiteral(R"JS(
        function check56(x,m){if(!x)throw Error(m);}
        Wavegen.Channel1.Mode.text='Simple';
        Wavegen.Channel1.Sweep.Frequency.enable=false;
        Wavegen.Channel1.Modulation.AM.enable=false;
        Wavegen.Channel1.Modulation.FM.enable=false;
        Wavegen.Channel1.Simple.Type.text='Sine';
        Wavegen.Channel1.Simple.Frequency.value=1000;
        Wavegen.Channel1.Simple.Amplitude.value=1;
        Wavegen.Channel1.Simple.Offset.value=0;
        Wavegen.Channel1.enable=true;
        check56(Wavegen.run(),'sine setup');
        Scope.Time.Mode.text='Repeated';
        Scope.Time.Samples.value=16384;
        Scope.Time.Rate.value=100000;
        Scope.Trigger.Trigger.text='None';
        check56(Scope.single()&&Scope.wait(3),'acquire');
        var first=Scope.Channel1.getData();
        var windowed=Scope.Channel1.measureWindow('RMS',0.02,0.07);
        check56(Math.abs(windowed-Math.SQRT1_2)<0.02,'windowed RMS');
        check56(Math.abs(Scope.Channel1.measure('RMS',0.02,0.07)-windowed)<1e-9,'measure overload');
        check56(Scope.Channel1.statistics(0.02,0.07).count===5000,'windowed count');
        check56(isNaN(Scope.Channel1.measureWindow('RMS',0.07,0.02)),'invalid window NaN');
        check56(Math.abs(Scope.Channel1.measure('ACRMS')-Math.SQRT1_2)<0.02,'AC RMS');
        var spec=Scope.Channel1.spectrum(8192,'Hann');
        check56(spec.fftSize===8192 && spec.frequencyHz.length===4097,'FFT output arrays');
        check56(Math.abs(spec.peakFrequencyHz-1000)<5,'FFT peak frequency');
        check56(Math.abs(spec.peakAmplitude-1)<0.1,'FFT voltage amplitude');
        check56(Object.keys(Scope.Channel1.spectrum(1000,'Hann')).length===0,'invalid FFT size');
        check56(Scope.saveReference(1),'capture reference');
        check56(Scope.referenceData(1).length===16384,'reference size');
        check56(Scope.mathData('CH1-REF1').every(function(v){return Math.abs(v)<1e-10;}),'reference subtraction');
        check56(Scope.mathData('CH1-CH2').length===16384,'dual channel math');
        check56(Scope.Time.timestampSource==='HostAfterTransfer','timestamp provenance');
        check56(Scope.captureInfo().sampleCount===16384,'capture info');
        check56(Scope.Time.takenEpochMs>0,'host epoch');
        Wavegen.stop();true;
    )JS"),"<release-C-simulation>");
    if(releaseC.isError()||!releaseC.toBool()) {
        qCritical().noquote()<<"Release C smoke failed:"<<releaseC.toString()
                            <<"line"<<releaseC.property("lineNumber").toInt();
        return 1;
    }
    // Native wait(negative) is specified to return false *after* cancellation,
    // not after an arbitrary one-hour timeout. Test immediate-cancel branch only.
    stop.store(true);
    if (utils.wait(-1) || utils.wait(0.1)) {
        qCritical() << "Cancellation contract failed";
        return 1;
    }
    qInfo() << "PASS: simulated WaveForms subset, readbacks, measurements, snapshots, helpers, stop";
    // Earlier cancellation test intentionally set stop=true.  Reset it before
    // the independent Release B AM/Record smoke (which is not a stop test).
    stop.store(false);
    const QJSValue releaseB=engine.evaluate(QStringLiteral(R"JS(
        function ok(x,m){if(!x)throw Error(m);}
        Wavegen.Channel1.enable=true;
        Wavegen.Channel2.enable=false;
        Wavegen.Channel1.Mode.text='Simple';
        Wavegen.Channel1.Simple.Type.text='Sine';
        Wavegen.Channel1.Simple.Amplitude.value=1;
        Wavegen.Channel1.Simple.Offset.value=0;
        Wavegen.Channel1.Simple.Frequency.value=1000;
        Wavegen.Channel1.Modulation.AM.enable=true;
        Wavegen.Channel1.Modulation.AM.Frequency.value=20;
        Wavegen.Channel1.Modulation.AM.Depth.value=50;
        ok(Wavegen.run(),'simulate AM');
        Scope.Time.Mode.text='Record';
        Scope.Time.RecordLength.value=0.1;
        Scope.Time.Rate.value=100000;
        ok(Scope.single()&&Scope.wait(2),'AM recording');
        ok(Scope.Channel1.getData().length===10000,'AM data length');
        Wavegen.Channel1.Modulation.AM.enable=false;
        Wavegen.Channel1.Mode.text='Sweep';
        Wavegen.Channel1.Sweep.Frequency.enable=true;
        Wavegen.Channel1.Sweep.Frequency.Start.value=500;
        Wavegen.Channel1.Sweep.Frequency.Stop.value=1500;
        Wavegen.Channel1.Sweep.Frequency.Time.value=0.1;
        ok(Wavegen.config(),'Sweep config');
        Wavegen.Channel1.Mode.text='Play';
        var v=[];for(var i=0;i<256;i++)v.push(Math.sin(i*2*Math.PI/256));
        ok(Wavegen.Channel1.Play.set(v),'Play set');
        Wavegen.Channel1.Play.Rate.value=10000;
        ok(Wavegen.play(1,2),'simulation Play dryrun');
        ok(!Wavegen.run(),'reject unattended Play run');
        Wavegen.stop();true;
    )JS"),"<v5.5-test>");
    if(releaseB.isError()||!releaseB.toBool()) {
        qCritical().noquote()<<"Release B smoke failed:"<<releaseB.toString();
        return 1;
    }
    return 0;
}
