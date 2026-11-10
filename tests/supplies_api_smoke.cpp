#include "PowerSuppliesBridge.h"
#include "CompletionCatalog.h"
#include "ApiKnowledge.h"
#include "DwfRuntime.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QDebug>
#include <cstdlib>

static void check(bool ok, const char* detail) {
    if(!ok) { qCritical()<<"FAIL supplies smoke:"<<detail;std::exit(1); }
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QJSEngine engine;
    PowerSuppliesBridge supplies(nullptr); // simulation - SDK is not loaded
    QJSEngine::setObjectOwnership(&supplies,QJSEngine::CppOwnership);
    auto& outputs=supplies.outputs();
    auto* p=&outputs.positiveChannel();auto* n=&outputs.negativeChannel();
    for(QObject* o : {supplies.masterEnable(),supplies.output(),static_cast<QObject*>(p),
                  p->enable(),p->voltage(),static_cast<QObject*>(n),n->enable(),n->voltage()})
        QJSEngine::setObjectOwnership(o,QJSEngine::CppOwnership);
    engine.globalObject().setProperty("Supplies",engine.newQObject(&supplies));
    check(!supplies.running(),"must boot OFF");
    auto value=engine.evaluate(R"JS(
        Supplies.Output.PositiveSupply.Voltage.value=3.3;
        Supplies.Output.NegativeSupply.Voltage.value=-2.5;
        Supplies.Output.PositiveSupply.Enable.value=1;
        Supplies.Output.NegativeSupply.Enable.value=1;
        !Supplies.running && !Supplies.MasterEnable.checked;
    )JS");
    check(!value.isError() && value.toBool(),"properties stage with no output");
    value=engine.evaluate(R"JS(
       if (!Supplies.run()) throw Error(Supplies.lastError);
       var x=Supplies.status();
       x.simulation && !x.measuredAvailable && x.positiveVoltage===3.3 && x.negativeVoltage===-2.5 && Supplies.MasterEnable.checked;
    )JS");
    check(!value.isError() && value.toBool(),"run and ideal simulated status");
    value=engine.evaluate(R"JS(
      Supplies.Output.PositiveSupply.Voltage.value=5.2;
      !Supplies.config() && !Supplies.running && !Supplies.MasterEnable.checked;
    )JS");
    check(!value.isError() && value.toBool(),"out-of-range voltage fails closed");
    value=engine.evaluate(R"JS(
      Supplies.Output.PositiveSupply.Voltage.value=0.5;
      Supplies.Output.NegativeSupply.Voltage.value=-5;
      Supplies.Output.NegativeSupply.Enable.value=0;
      Supplies.run() && Supplies.status().negativeVoltage===0 && Supplies.stop() && !Supplies.running;
    )JS");
    check(!value.isError() && value.toBool(),"rail gating, min and max, and stop");
    check(ApiKnowledge::isDocumentedSymbol("Supplies.MasterEnable.checked"),"completion master");
    check(ApiKnowledge::isDocumentedSymbol("Supplies1.Output.PositiveSupply.Voltage.value"),"alias completion");
    check(ApiKnowledge::lookup("Supplies.Output.NegativeSupply.Voltage.value").known,"F1 detail");
    qInfo()<<"PASS v5.6.44: native Qt simulation, power fail-closed, nested QJSEngine properties, F1 and completion";
}
