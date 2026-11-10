// v5.6.24: native QWidget inspector, event metadata, Undo/Redo, copy and persistence.
#include "PanelDesigner.h"
#include "PanelEventCatalog.h"
#include "PanelProject.h"
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QUndoStack>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

static void require(bool ok, const char* reason) {
    if(!ok) qFatal("WFScript event inspector: %s",reason);
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    require(PanelEventCatalog::supports("button","clicked"),"button event advertised");
    require(PanelEventCatalog::supports("slider","valueChanged"),"slider event advertised");
    require(PanelEventCatalog::supports("toggle","toggled"),"toggle event advertised");
    require(!PanelEventCatalog::supports("plot","clicked"),"no unsupported Plot event");
    PanelDesigner designer;
    auto* canvas=designer.canvas();
    PanelControl control;
    control.id="runDemo";control.type="button";control.text="Start";
    control.geometry=QRect(20,20,150,36);
    require(canvas->addControl(control),"create button");
    canvas->resetDesignHistory();
    designer.show();
    canvas->selectControlById("runDemo");
    QCoreApplication::processEvents();
    auto* tabs=designer.findChild<QTabWidget*>("panelInspectorTabs");
    auto* list=designer.findChild<QListWidget*>("panelEventList");
    auto* handler=designer.findChild<QLineEdit*>("panelEventHandler");
    auto* assign=designer.findChild<QPushButton*>("panelEventAssign");
    auto* create=designer.findChild<QPushButton*>("panelEventCreate");
    auto* remove=designer.findChild<QPushButton*>("panelEventRemove");
    auto* enabled=designer.findChild<QCheckBox*>("panelEventEnabled");
    require(tabs && list && handler && assign && create && remove && enabled,"inspector controls exist");
    require(tabs->count()==2 && list->count()==1 &&
       list->currentItem()->data(Qt::UserRole).toString()=="clicked","button event listed");
    QString createdId, createdHandler;
    bool askedCreate=false;
    QObject::connect(&designer,&PanelDesigner::eventHandlerRequested,&designer,
      [&](const QString& id,const QString&,const QString& function,bool createFlag) {
          createdId=id;createdHandler=function;askedCreate=createFlag;
      });
    create->click();
    require(askedCreate && createdId=="runDemo" && createdHandler=="onRunDemoClicked",
            "Create Handler requests named main script function");
    QJsonObject events=canvas->selectedControl().events;
    require(events.value("clicked").toObject().value("enabled").toBool(),"Create enables project binding");
    require(canvas->undoStack()->canUndo(),"Event edit is undoable");
    canvas->undoStack()->undo();
    require(canvas->selectedControl().events.isEmpty(),"Undo removes binding");
    canvas->undoStack()->redo();
    require(canvas->selectedControl().events.value("clicked").toObject().value("handler").toString()=="onRunDemoClicked",
            "Redo restores binding");
    handler->setText("customHandler");
    assign->click();
    require(canvas->selectedControl().events.value("clicked").toObject().value("handler").toString()=="customHandler",
            "Assign Existing changes function without altering JavaScript");
    enabled->setChecked(false);
    require(!canvas->selectedControl().events.value("clicked").toObject().value("enabled").toBool(),
            "Disable leaves assignment intact");
    const QJsonArray savedControls=canvas->toJson();
    PanelProject project=PanelProject::newProject("Panel.js","function customHandler(value){}\n");
    project.mainPage().controls=savedControls;
    QString error;
    QJsonObject json;
    require(project.toJson(&json,&error),"project serialization with events");
    PanelProject loaded;
    require(PanelProject::fromJson(json,"Test.wfpanel",&loaded,&error),"project round-trip with events");
    require(loaded.mainPage().controls==savedControls,"widget events preserved exactly");
    require(canvas->fromJson(loaded.mainPage().controls,&error),"canvas restores events");
    canvas->selectControlById("runDemo");
    remove->click();
    require(canvas->selectedControl().events.isEmpty(),"Remove Binding does not delete widget");
    canvas->undoStack()->undo();
    require(!canvas->selectedControl().events.isEmpty(),"Undo restores removed binding");
    canvas->duplicateSelected();
    require(canvas->selectionCount()==1 && canvas->selectedControl().events.isEmpty(),
            "duplicated widget does not inherit event connections");
    qInfo()<<"PASS: native inspector, catalog, Create/Assign/Disable/Remove, Undo/Redo, project persistence, safe duplication";
    return 0;
}
