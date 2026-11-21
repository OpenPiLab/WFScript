// RC1.5.14.9: regression for stale Panel colors during initial load,
// theme toggles, re-run, and retained virtual measurement/log/data rows.
// Offscreen Qt test: no TestRunner, hardware, or scripting thread needed.
#include "PanelCanvas.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/MeasurementTableWidget.h"
#include "widgets/EventLogWidget.h"
#include "widgets/ExperimentWidgets.h"
#include "widgets/TestSequenceWidget.h"
#include <QApplication>
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QPalette>
#include <QDebug>
#include <QTableView>
#include <QVariantList>
#include <QVariantMap>
#define CHECK(x) do{if(!(x)){qCritical()<<"THEME RC15149 FAIL"<<#x<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PanelCanvas canvas;
    canvas.setDocumentSize(QSize(1300,900));
    canvas.setPageAppearance(QColor("#edf4fa"),true,false);
    auto make=[&](const QString& type,const QString& id,const QRect& rect)->bool {
        const auto* factory=WidgetRegistry::instance().factory(type);
        if(!factory)return false;
        PanelControl def=factory->defaultDefinition(id);
        def.geometry=rect;
        return canvas.addControl(def);
    };
    CHECK(make("measurement_table","measure",QRect(20,20,600,240)));
    CHECK(make("event_log","log",QRect(640,20,600,240)));
    CHECK(make("experiment_table","data",QRect(20,280,600,240)));
    CHECK(make("test_sequence","sequence",QRect(640,280,600,240)));
    CHECK(make("toggle","toggle",QRect(20,530,220,50)));
    auto* measurement=dynamic_cast<MeasurementTableWidget*>(canvas.findChild<QWidget*>("measure"));
    auto* log=dynamic_cast<EventLogWidget*>(canvas.findChild<QWidget*>("log"));
    auto* data=dynamic_cast<ExperimentDataTableWidget*>(canvas.findChild<QWidget*>("data"));
    auto* sequence=dynamic_cast<TestSequenceWidget*>(canvas.findChild<QWidget*>("sequence"));
    auto* toggle=canvas.findChild<QCheckBox*>("toggle");
    CHECK(measurement && log && data && sequence && toggle);
    CHECK(measurement->styleSheet().contains("#edf4fa"));
    CHECK(log->styleSheet().contains("#edf4fa"));
    CHECK(data->tableView()->styleSheet().contains("#ffffff"));
    CHECK(sequence->styleSheet().contains("#edf4fa"));
    toggle->setChecked(true);
    CHECK(data->applyValue(QVariantMap{{"clear",true}}) && data->rowCount()==0);
    CHECK(measurement->setMeasurements(QVariantMap{{"append",QVariantMap{
        {"name","Vout"},{"value",3.3},{"low",3.0},{"high",3.6}}}}));
    CHECK(log->applyEvents(QVariantMap{{"append",QVariantMap{{"message","one"}}}}));
    CHECK(data->applyValue(QVariantMap{{"append",QVariantList{"A","B","C","D"}}}));
    CHECK(sequence->applySteps(QVariantList{QVariantMap{
        {"id","test"},{"name","Measure"},{"status","pass"}}}));
    CHECK(measurement->rowCount()==1 && log->entryCount()==1 && data->rowCount()==1);
    CHECK(sequence->stepCount()==1 && toggle->isChecked());
    int measureResets=0,logResets=0,dataResets=0;
    QObject::connect(measurement->model(),&QAbstractItemModel::modelReset,&canvas,[&]{++measureResets;});
    auto* logView=log->findChild<QTableView*>("eventLogTable");
    CHECK(logView);
    QObject::connect(logView->model(),&QAbstractItemModel::modelReset,&canvas,[&]{++logResets;});
    QObject::connect(data->model(),&QAbstractItemModel::modelReset,&canvas,[&]{++dataResets;});
    for(int i=0;i<8;++i) {
        const bool light=(i%2)==0;
        canvas.setPageAppearance(QColor(light?"#edf4fa":"#0f1824"),light,false);
        app.processEvents();
        CHECK(measurement->styleSheet().contains(light?"#edf4fa":"#101e2f"));
        CHECK(log->styleSheet().contains(light?"#edf4fa":"#101e2f"));
        CHECK(data->tableView()->styleSheet().contains(light?"#ffffff":"#142638"));
        CHECK(sequence->styleSheet().contains(light?"#edf4fa":"#101e2f"));
        CHECK(measurement->rowCount()==1 && log->entryCount()==1 && data->rowCount()==1);
        CHECK(sequence->stepCount()==1 && toggle->isChecked());
    }
    CHECK(measureResets==0 && logResets==0 && dataResets==0);
    // Runtime / Design transition must not be a prerequisite to good colors.
    canvas.setDesignMode(false);
    canvas.setDesignMode(true);
    CHECK(measurement->rowCount()==1 && log->entryCount()==1);
    canvas.setPageAppearance(QColor("#edf4fa"),true,false);
    CHECK(toggle->isChecked());
    qInfo()<<"PASS RC1.5.14.9 Panel initial theme, 8 toggles, rows and checkbox state retained";
    return 0;
}
