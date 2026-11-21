// Project and worker-bridge smoke for typed Panel Data; no DWF hardware needed.
#include "PanelDataSchema.h"
#include "PanelProject.h"
#include "PanelUiBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <cmath>

static void require(bool ok,const char* reason) {
    if(!ok)qFatal("Panel Data smoke: %s",reason);
}
static QJsonObject variable(const QString& name,const QString& type,const QJsonValue& initial) {
    return QJsonObject{{"name",name},{"type",type},{"initial",initial}};
}
static QJsonObject widget(const QString& id,const QString& type,const QString& binding) {
    return QJsonObject{{"id",id},{"type",type},{"text",id},
        {"x",20},{"y",20},{"width",200},{"height",90},{"value",0.0},
        {"min",0.0},{"max",3000.0},
        {"bindings",QJsonObject{{"value",binding}}}};
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QJsonArray definitions{
        variable("Motor.Speed","number",0.0),
        variable("Motor.Target","number",1000.0),
        variable("Motor.Ready","boolean",false),
        variable("Motor.Mode","text","Auto")};
    const QJsonArray controls{widget("readout","digital_display","Motor.Speed"),
                              widget("dial","precision_dial","Motor.Speed"),
                              widget("target","numeric_stepper","Motor.Target"),
                              widget("beacon","status_beacon","Motor.Ready"),
                              widget("readyInput","toggle","Motor.Ready"),
                              widget("mode","option_selector","Motor.Mode")};
    PanelProject project=PanelProject::newProject("DataTest.js","print('hello');");
    project.panelData=definitions;
    project.mainPage().controls=controls;
    QString error;
    require(project.validate(&error),"valid typed project and bindings");
    QJsonObject saved;
    require(project.toJson(&saved,&error),"serialize Panel Data");
    PanelProject loaded;
    require(PanelProject::fromJson(saved,"DataTest.wfpanel",&loaded,&error),"deserialize Panel Data");
    require(loaded.panelData==definitions && loaded.mainPage().controls==controls,"data and bindings round-trip");
    // Define the same shared value in two Page widgets; the project enforces types globally.
    require(loaded.addPage("second","Second",QSize(900,550),&error),"create second Page");
    loaded.findPage("second")->controls=QJsonArray{widget("otherReadout","digital_display","Motor.Speed")};
    require(loaded.validate(&error),"cross-Page binding valid");
    const QJsonArray otherPage=loaded.findPage("second")->controls;
    loaded.findPage("second")->controls=QJsonArray{widget("bad","status_beacon","Motor.Speed")};
    require(!loaded.validate(&error),"wrong widget/variable type rejected");
    loaded.findPage("second")->controls=QJsonArray{widget("bad","digital_display","Unknown.Speed")};
    require(!loaded.validate(&error),"missing variable rejected");
    loaded.findPage("second")->controls=otherPage;
    loaded.panelData.append(variable("Motor.Speed","number",0.0));
    require(!loaded.validate(&error),"duplicate variable rejected");
    loaded.panelData=definitions;
    require(loaded.validate(&error),"valid project after rejected cases");

    QJSEngine engine;
    QVariantMap initial{{"readout",0.0},{"dial",0.0},{"target",1000.0},
                        {"beacon",false},{"readyInput",false},{"mode",QStringLiteral("Auto")}};
    PanelUiBridge bridge(&engine,initial,{},definitions,controls);
    int readoutEvents=0, dialEvents=0, dataEvents=0;
    QObject::connect(&bridge,&PanelUiBridge::updateRequested,&bridge,
        [&](const QString& id,const QVariant&){
            if(id=="readout")++readoutEvents;
            if(id=="dial")++dialEvents;
        });
    QObject::connect(&bridge,&PanelUiBridge::dataValueChanged,&bridge,
        [&](const QString&,const QVariant&){++dataEvents;});
    bridge.publishInitialData();
    require(bridge.getData("Motor.Target").toNumber()==1000.0,"initial target available");
    require(bridge.get("readout").toNumber()==0.0,"initial bound widget cache available");
    const int initialReadout=readoutEvents,initialDial=dialEvents;
    require(bridge.setData("Motor.Speed",QJSValue(1250.0)),"accept typed numeric update");
    require(bridge.get("readout").toNumber()==1250.0 &&
            bridge.get("dial").toNumber()==1250.0,"same value reaches two bound widgets");
    require(readoutEvents==initialReadout+1 && dialEvents==initialDial+1,"exactly one update per bound widget");
    require(bridge.setData("Motor.Speed",QJSValue(1250.0)) &&
            readoutEvents==initialReadout+1,"identical data value does not re-emit");
    require(!bridge.set("readout",QJSValue(5.0)),"direct ui.set cannot override bound widget");
    require(!bridge.setData("Motor.Speed",QJSValue(QStringLiteral("bad"))),"strict numeric type");
    require(!bridge.setData("Missing",QJSValue(1.0)),"unknown variable rejected");
    require(bridge.setDataFromInput("target","valueChanged",QVariant(1800.0)) &&
            bridge.getData("Motor.Target").toNumber()==1800.0,"native input edits shared data");
    require(!bridge.setDataFromInput("target","clicked",QVariant(500.0)) &&
            bridge.getData("Motor.Target").toNumber()==1800.0,"non-value events are not bindings");
    require(bridge.setDataFromInput("mode","selectionChanged",QVariant(QStringLiteral("Manual"))) &&
            bridge.getData("Motor.Mode").toString()==QStringLiteral("Manual"),"option selector string binding");
    require(!bridge.setDataFromInput("beacon","toggled",QVariant(true)),
            "status beacon is a read-only boolean display");
    require(bridge.setDataFromInput("readyInput","toggled",QVariant(true)) &&
            bridge.getData("Motor.Ready").toBool() && bridge.get("beacon").toBool(),
            "boolean input updates shared data and bound read-only beacon");
    require(bridge.dataNames().size()==4 && dataEvents>=4,"data names and change notifications");
    const auto jsNames=bridge.dataNamesJs();
    require(jsNames.isArray() && jsNames.property("length").toInt()==4,"Data.names returns native JavaScript Array");
    qInfo()<<"PASS: Panel Data schema, serialization, cross-Page bindings, type safety, exact-once updates";
    return 0;
}
