// v5.6.53: offscreen Qt test, no instrument access and no DWF DLL.
#include "PanelCanvas.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include "widgets/WidgetPropertyInspector.h"
#include "widgets/MeasurementResultWidget.h"
#include "widgets/MeasurementTableWidget.h"
#include "widgets/TestSequenceWidget.h"
#include <QApplication>
#include <QDebug>
#include <QVariantList>
#include <QVariantMap>
#include <QSpinBox>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>

#define CHECK(condition) do {if(!(condition)){qCritical()<<"FAIL"<<#condition<<"line"<<__LINE__;return 1;}}while(false)

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    auto& registry=WidgetRegistry::instance();
    const auto* resultFactory=registry.factory("measurement_result");
    const auto* tableFactory=registry.factory("measurement_table");
    CHECK(resultFactory && tableFactory);
    QWidget root;
    PanelControl definition=resultFactory->defaultDefinition("resultA");
    CHECK(definition.minimum==3.25 && definition.maximum==3.35);
    QWidget* resultBase=resultFactory->create(definition,&root,[](const QString&,const QVariant&){});
    auto* result=dynamic_cast<MeasurementResultWidget*>(resultBase);
    CHECK(result && result->resultState()=="NOT TESTED");
    resultFactory->applyRuntimeValue(result,definition,3.3);
    CHECK(result->resultState()=="PASS");
    resultFactory->applyRuntimeValue(result,definition,3.4);
    CHECK(result->resultState()=="FAIL");
    resultFactory->applyRuntimeValue(result,definition,3.25);
    CHECK(result->resultState()=="PASS"); // inclusive bounds
    resultFactory->applyRuntimeValue(result,definition,QVariantMap{{"reset",true}});
    CHECK(result->resultState()=="NOT TESTED");
    resultFactory->applyRuntimeValue(result,definition,QVariantMap{{"status","ERROR"}});
    CHECK(result->resultState()=="ERROR");
    resultFactory->applyRuntimeValue(result,definition,3.3);
    CHECK(result->resultState()=="PASS");
    PanelControl copy=definition;
    copy.maximum=3.29;
    resultFactory->applyDefinition(result,copy);
    CHECK(result->resultState()=="FAIL");

    // Editing designer properties and saving to JSON uses the original codec.
    WidgetPropertyInspector inspector;
    inspector.setControl(definition,true);
    CHECK(inspector.findChild<QSpinBox*>("panelProperty_decimals"));
    const PanelControl readback=inspector.readControl(definition);
    PanelControl decoded;QString error;
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(readback),&decoded,&error));
    CHECK(decoded.type=="measurement_result" && decoded.minimum==3.25);
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(tableFactory->defaultDefinition("tableA")),&decoded,&error));

    PanelControl tableDefinition=tableFactory->defaultDefinition("tableA");
    QWidget* tableBase=tableFactory->create(tableDefinition,&root,[](const QString&,const QVariant&){});
    auto* table=dynamic_cast<MeasurementTableWidget*>(tableBase);
    CHECK(table && table->displayedRows()==0);
    QVariantList rows{
        QVariantMap{{"name","V+"},{"value",3.301},{"low",3.25},{"high",3.35},{"unit","V"}},
        QVariantMap{{"name","PWM"},{"value",47.5},{"low",49.0},{"high",51.0},{"unit","%"}}
    };
    tableFactory->applyRuntimeValue(table,tableDefinition,rows);
    CHECK(table->displayedRows()==2);
    // A real QJSEngine creates JS arrays of objects. The worker must convert
    // them before crossing into the GUI thread or the Table will see a JS wrapper.
    QJSEngine jsEngine;
    const QJSValue jsRows=jsEngine.evaluate(QStringLiteral(
        "[{name:'V+',value:3.301,low:3.25,high:3.35,unit:'V'},"
        "{name:'PWM',value:47.5,low:49,high:51,unit:'%'}]"));
    CHECK(!jsRows.isError() && jsRows.isArray());
    const QVariant nativeRows=jsRows.toVariant(QJSValue::ConvertJSObjects);
    CHECK(nativeRows.metaType().id()==QMetaType::QVariantList);
    CHECK(nativeRows.toList().size()==2);
    CHECK(nativeRows.toList().at(0).metaType().id()==QMetaType::QVariantMap);
    tableFactory->applyRuntimeValue(table,tableDefinition,nativeRows);
    CHECK(table->displayedRows()==2 && table->item(1,3)->text()=="FAIL");
    CHECK(table->item(0,3)->text()=="PASS");
    CHECK(table->item(1,3)->text()=="FAIL");
    QVariantList invalid=rows;
    invalid.append(QVariantMap{{"name","Invalid"},{"value",1.0},{"low",7.0},{"high",1.0}});
    CHECK(!table->setMeasurements(invalid));
    CHECK(table->displayedRows()==2); // atomic invalid-row rejection
    CHECK(table->setMeasurements(QVariantList{QVariantMap{{"name","Untested"}}}));
    CHECK(table->item(0,3)->text()=="NOT TESTED");
    tableFactory->applyRuntimeValue(table,tableDefinition,QVariantMap{{"reset",true}});
    CHECK(table->displayedRows()==0);
    // These QWidget-derived visual controls intentionally do not declare Q_OBJECT.
    // Qt 6.9 requires Q_OBJECT for findChild<T*>; search by QWidget then use
    // C++ RTTI to verify the concrete subclass without changing production UI.
    // No duplicate runtimes: updates to retained Qt widgets persist across
    // Page switches and never alter the saved design-time default.
    PanelCanvas canvas;
    PanelControl def=resultFactory->defaultDefinition("crossPageResult");
    def.geometry=QRect(20,20,310,138);
    QJsonArray summary{WidgetDocumentCodec::encode(def)};
    PanelControl tbl=tableFactory->defaultDefinition("crossPageTable");
    tbl.geometry=QRect(20,20,660,300);
    QJsonArray tablePage{WidgetDocumentCodec::encode(tbl)};
    CHECK(canvas.activatePage("summary",summary,QSize(900,500),&error));
    auto* retained=dynamic_cast<MeasurementResultWidget*>(canvas.findChild<QWidget*>("crossPageResult"));
    CHECK(retained && retained->resultState()=="NOT TESTED");
    CHECK(canvas.activatePage("table",tablePage,QSize(900,500),&error));
    auto* retainedTable=dynamic_cast<MeasurementTableWidget*>(canvas.findChild<QWidget*>("crossPageTable"));
    CHECK(retainedTable && retainedTable->displayedRows()==0);
    canvas.applyValue("crossPageResult",3.3);
    CHECK(retained->resultState()=="PASS");
    canvas.applyValue("crossPageTable",nativeRows);
    CHECK(retainedTable->displayedRows()==2);
    CHECK(canvas.activatePage("summary",summary,QSize(900,500),&error));
    // The Table is now hidden. A fresh ui.set() must still update its rows.
    canvas.applyValue("crossPageTable",QVariantList{
        QVariantMap{{"name","PWM"},{"value",50.1},{"low",49.0},{"high",51.0},{"unit","%"}}});
    CHECK(retainedTable->displayedRows()==1 && retainedTable->item(0,3)->text()=="PASS");
    CHECK(canvas.activatePage("table",tablePage,QSize(900,500),&error));
    CHECK(dynamic_cast<MeasurementTableWidget*>(canvas.findChild<QWidget*>("crossPageTable"))==retainedTable);
    CHECK(retainedTable->displayedRows()==1 && retainedTable->item(0,3)->text()=="PASS");
    CHECK(canvas.activatePage("summary",summary,QSize(900,500),&error));
    CHECK(dynamic_cast<MeasurementResultWidget*>(canvas.findChild<QWidget*>("crossPageResult"))==retained);
    CHECK(retained->resultState()=="PASS");
    CHECK(canvas.toJson().at(0).toObject().value("value").toDouble()==def.value);
    table->resize(660,300);table->show();result->resize(310,138);result->show();
    app.processEvents();
    // v5.6.54: Native visual Test Sequence. This is a display, not a scheduler.
    const auto* sequenceFactory=registry.factory("test_sequence");
    CHECK(sequenceFactory);
    PanelControl seqDef=sequenceFactory->defaultDefinition("seqA");
    CHECK(seqDef.type==QStringLiteral("test_sequence"));
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(seqDef),&decoded,&error));
    CHECK(decoded.type==QStringLiteral("test_sequence"));
    CHECK(sequenceFactory->initialRuntimeValue(seqDef).toList().size()==4);
    QWidget* sequenceBase=sequenceFactory->create(seqDef,&root,[](const QString&,const QVariant&){});
    auto* sequence=dynamic_cast<TestSequenceWidget*>(sequenceBase);
    CHECK(sequence && sequence->stepCount()==4);
    CHECK(sequence->overallState()==QStringLiteral("PENDING"));
    // Real JS arrays/objects must become native Qt data BEFORE a queued UI call.
    QJSValue jsSteps=jsEngine.evaluate(QStringLiteral(
        "[{id:'init',name:'Initialize',status:'pending'},"
        "{id:'power',name:'Verify power',status:'pending'}]"));
    CHECK(!jsSteps.isError() && jsSteps.isArray());
    const QVariant nativeSteps=jsSteps.toVariant(QJSValue::ConvertJSObjects);
    CHECK(nativeSteps.metaType().id()==QMetaType::QVariantList);
    CHECK(sequence->applySteps(nativeSteps));
    CHECK(sequence->stepCount()==2 && sequence->completedSteps()==0);
    QJSValue jsUpdate=jsEngine.evaluate(QStringLiteral(
        "({update:{id:'init',status:'pass',elapsedMs:125,message:'ready'}})"));
    CHECK(!jsUpdate.isError() && jsUpdate.isObject());
    CHECK(sequence->applySteps(jsUpdate.toVariant(QJSValue::ConvertJSObjects)));
    CHECK(sequence->statusFor("init")==QStringLiteral("pass"));
    CHECK(sequence->completedSteps()==1 && sequence->percentComplete()==50);
    CHECK(!sequence->applySteps(QVariantMap{{"update",QVariantMap{{"id","unknown"},{"status","pass"}}}}));
    CHECK(sequence->completedSteps()==1); // invalid updates are atomic
    CHECK(!sequence->applySteps(QVariantList{
        QVariantMap{{"id","dup"},{"name","First"}},
        QVariantMap{{"id","dup"},{"name","Duplicate"}}}));
    CHECK(sequence->completedSteps()==1);
    CHECK(!sequence->applySteps(QVariantMap{{"update",QVariantMap{{"id","power"},{"status","garbage"}}}}));
    CHECK(sequence->statusFor("power")==QStringLiteral("pending"));
    CHECK(sequence->applySteps(QVariantMap{{"update",QVariantMap{{"id","power"},{"status","fail"},{"elapsedMs",150}}}}));
    CHECK(sequence->overallState()==QStringLiteral("FAILED"));
    CHECK(sequence->completedSteps()==2 && sequence->percentComplete()==100);
    CHECK(sequence->applySteps(QVariantMap{{"reset",true}}));
    CHECK(sequence->completedSteps()==0 && sequence->stepCount()==2);
    CHECK(sequence->applySteps(QVariantMap{{"clear",true}}));
    CHECK(sequence->stepCount()==0);
    sequence->resize(540,380);sequence->show();app.processEvents();
    // Verify hidden-Page routing, not just direct factory update.
    PanelControl seqPageDef=sequenceFactory->defaultDefinition("crossPageSequence");
    seqPageDef.geometry=QRect(20,20,540,380);
    QJsonArray sequencePage{WidgetDocumentCodec::encode(seqPageDef)};
    CHECK(canvas.activatePage("steps",sequencePage,QSize(900,500),&error));
    auto* heldSequence=dynamic_cast<TestSequenceWidget*>(canvas.findChild<QWidget*>("crossPageSequence"));
    CHECK(heldSequence && heldSequence->stepCount()==4);
    CHECK(canvas.activatePage("summary",summary,QSize(900,500),&error));
    canvas.applyValue("crossPageSequence",nativeSteps);
    canvas.applyValue("crossPageSequence",jsUpdate.toVariant(QJSValue::ConvertJSObjects));
    CHECK(heldSequence->statusFor("init")==QStringLiteral("pass"));
    CHECK(canvas.activatePage("steps",sequencePage,QSize(900,500),&error));
    CHECK(dynamic_cast<TestSequenceWidget*>(canvas.findChild<QWidget*>("crossPageSequence"))==heldSequence);
    CHECK(heldSequence->completedSteps()==1);
    // The design-time default is unaffected by transient statuses.
    CHECK(canvas.toJson().at(0).toObject().value("type").toString()==QStringLiteral("test_sequence"));
    qInfo()<<"PASS v5.6.54 Qt Test Sequence: conversion, limits, atomic updates, progress, hidden Pages, persistence";
    qInfo()<<"PASS v5.6.53 Qt measurement result, tolerance, table, validation and serialization";
    return 0;
}
