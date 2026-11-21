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
    // All user-event payloads are described by each registered widget factory.
    // Create Handler must not accidentally emit a parameterless function when
    // the widget actually passes the selected text or terminal payload.
    require(PanelEventCatalog::argumentFor("option_selector","selectionChanged")=="text" &&
            PanelEventCatalog::argumentJsTypeFor("option_selector","selectionChanged")=="string",
            "option selector handler signature uses text catalog metadata");
    require(PanelEventCatalog::argumentFor("communications_terminal","sendRequested")=="payload" &&
            PanelEventCatalog::argumentJsTypeFor("communications_terminal","sendRequested")=="object",
            "terminal handler signature uses payload catalog metadata");
    require(PanelEventCatalog::argumentFor("experiment_table","cellEdited")=="edit" &&
            PanelEventCatalog::argumentJsTypeFor("experiment_table","cellEdited")=="object",
            "table handler uses lightweight cellEdited metadata");
    require(PanelEventCatalog::argumentFor("experiment_table","valueChanged")=="rows",
            "explicitly opted-in historical table handlers remain valid");
    require(PanelEventCatalog::argumentFor("button","clicked").isEmpty(),
            "button handler needs no argument");
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
    require(tabs->count()==3 && tabs->tabText(2)==QStringLiteral("Panel Data") && list->count()==1 &&
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
    // Native parser + event catalog validates the real embedded examples, not
    // just a standalone Python interpretation of the JSON document format.
    const QStringList convertedExamples={"GettingStarted","WidgetPlayground",
        "PanelTimers","SQLiteDataLab","SerialCommsLab","TcpCommsLab",
        "UdpCommsLab","CommunicationsTerminalLab",
        // Phase 2: operator, educational, analog and protocol examples.
        "ScriptUtilityLab","LabelFormatting","EquationExperimentLab",
        "AD3ScopeExplorer","PowerSupplies","ClockPWM","LogicAnalyzer",
        "QuadratureEncoder","SequenceLab","StaticDigitalIO",
        "ProtocolUART","ProtocolSPI","ProtocolI2C","ProtocolCAN","ProtocolSWD",
        // Phase 3: remaining interactive examples, including protected W1 actions.
        "AD3HardwareTestStation","AnalogCharts","AutomatedTestBench",
        "AutomatedTestStation","CalibrationTable","ChartTechniques",
        "DigitalInputMonitor","DigitalTimingCharts","ProtocolUartAsync",
        "SignalLab","TableDrivenWavegen","TableLimitPlot"};
    int validatedBindings=0;
    for(const QString& stem:convertedExamples) {
        PanelProject example;
        QString parseError;
        const QString path=QStringLiteral(":/wfscript/examples/")+stem+QStringLiteral(".wfpanel");
        require(PanelProject::loadFile(path,&example,&parseError),
                "embedded example opens with production project parser");
        for(const auto& page:example.pages)for(const auto& value:page.controls) {
            const auto control=value.toObject();
            const QString id=control.value(QStringLiteral("id")).toString();
            const QString type=control.value(QStringLiteral("type")).toString();
            const QJsonObject bindings=control.value(QStringLiteral("events")).toObject();
            require(PanelEventCatalog::validateBindings(type,bindings),
                    "embedded event is supported by its factory");
            for(auto it=bindings.begin();it!=bindings.end();++it) {
                const QString handler=it.value().toObject().value(QStringLiteral("handler")).toString();
                require(it.value().toObject().value(QStringLiteral("enabled")).toBool(),
                        "converted binding enabled");
                const QRegularExpression fn(QStringLiteral("\\bfunction\\s+%1\\s*\\(")
                    .arg(QRegularExpression::escape(handler)));
                require(fn.match(example.script).hasMatch(),
                        "designer binding refers to a global named function");
                const QRegularExpression duplicate(QStringLiteral("\\bui\\.on\\s*\\(\\s*['\"]%1['\"]\\s*,\\s*['\"]%2['\"]")
                    .arg(QRegularExpression::escape(id),QRegularExpression::escape(it.key())));
                require(!duplicate.match(example.script).hasMatch(),
                        "no duplicate manual UI event handler");
                ++validatedBindings;
            }
        }
    }
    require(validatedBindings==229,"all 229 visual event bindings persisted and parseable");
    qInfo()<<"PASS: native inspector, catalog, Create/Assign/Disable/Remove, Undo/Redo, project persistence, safe duplication";
    return 0;
}
