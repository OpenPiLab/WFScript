// v5.6.25 native Qt Page test: document round trip and designer UI.
#include "PanelDesigner.h"
#include "plot/PanelChartWidget.h"
#include "PanelProject.h"
#include <QApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QTabBar>
#include <QScrollArea>
#include <QSplitter>
#include <QPushButton>
#include <QAbstractButton>
#include <QSignalBlocker>
#include "PanelEventCatalog.h"
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonObject>

static int failures=0;
static void check(bool yes,const QString& reason){if(!yes){qCritical().noquote()<<"FAIL:"<<reason;++failures;}}
static QJsonObject widget(const QString& id,const QString& handler={}) {
    QJsonObject o{{"id",id},{"type","button"},{"text",id},
                  {"x",20},{"y",30},{"width",140},{"height",36},
                  {"value",0},{"min",0},{"max",100}};
    if(!handler.isEmpty())o.insert("events",QJsonObject{{"clicked",QJsonObject{{"handler",handler},{"enabled",true}}}});
    return o;
}
int main(int argc,char**argv){
    QApplication app(argc,argv);
    PanelProject doc=PanelProject::newProject("Multipage.js","var count=0;");
    doc.mainPage().controls.append(widget("buttonMain","onMain"));
    QString err;
    check(doc.addPage("measure","Measurements",QSize(1024,768),&err),"Add Measurements Page: "+err);
    check(doc.addPage("settings","Settings",QSize(700,500),&err),"Add Settings Page: "+err);
    doc.findPage("measure")->controls.append(widget("buttonMeasure","onMeasure"));
    doc.findPage("settings")->controls.append(widget("buttonSettings"));
    doc.activePageId="measure";
    QJsonObject output;
    check(doc.toJson(&output,&err),"Write v2: "+err);
    check(output.value("version").toInt()==2 && output.value("pages").toArray().size()==3,"v2 Page list");
    PanelProject restored;
    check(PanelProject::fromJson(output,"Multi.wfpanel",&restored,&err),"Read v2: "+err);
    check(restored.activePageId=="measure" && restored.pages.size()==3,"Active Page persists");
    check(restored.findPage("measure") && restored.findPage("measure")->controls.at(0).toObject()
          .value("events").toObject().value("clicked").toObject().value("handler").toString()=="onMeasure",
          "Page event bindings survive round trip");
    restored.findPage("settings")->controls.append(widget("buttonMain"));
    check(!restored.validate(&err),"Cross-Page widget ID collision rejected");
    restored.findPage("settings")->controls.removeLast();
    check(restored.deletePage("settings",&err) && restored.findPage("settings")==nullptr,"Delete Page removes widgets");
    check(restored.pages.size()==2 && restored.findPage("measure")!=nullptr, "Remaining Pages preserved");
    PanelDesigner designer;
    auto* tabs=designer.findChild<QTabBar*>("panelPageTabs");
    check(tabs!=nullptr,"Page tab bar exists");
    auto* centerPane=designer.findChild<QWidget*>("panelCenterPane");
    check(centerPane && tabs && tabs->parentWidget()==centerPane &&
          designer.canvasScrollArea()->parentWidget()==centerPane,
          "Page tabs and the canvas share the center splitter pane");
    check(centerPane && qobject_cast<QSplitter*>(centerPane->parentWidget())!=nullptr,
          "Page strip is inside the splitter, not above the Toolbox");
    designer.setPageTabs({{"main","Main"},{"measure","Measurements"},{"settings","Settings"}},"measure");
    check(tabs && tabs->count()==4 && tabs->tabData(tabs->currentIndex()).toString()=="measure",
          "Three document Pages plus trailing action tab");
    check(tabs && tabs->tabText(3)=="+" && tabs->tabData(3).toString()=="__add_page__",
          "+ action tab is not a document Page");
    check(tabs && !tabs->tabButton(3,QTabBar::RightSide) &&
          !tabs->tabButton(3,QTabBar::LeftSide),"Action tab has no close button");
    QString requested;
    int addRequests=0, deleteRequests=0;
    QObject::connect(&designer,&PanelDesigner::pageSelected,&designer,[&](const QString& id){requested=id;});
    QObject::connect(&designer,&PanelDesigner::addPageRequested,&designer,[&]{++addRequests;});
    QObject::connect(&designer,&PanelDesigner::deletePageRequested,&designer,[&]{++deleteRequests;});
    if(tabs)tabs->setCurrentIndex(2);
    check(requested=="settings","Tab switching emits the stable Page ID");
    if(tabs)tabs->setCurrentIndex(3);
    check(addRequests==1 && tabs->currentIndex()==2 && designer.selectedPageId()=="settings",
          "Clicking plus requests Add, leaves existing Page selected");
    if(tabs) {
        QWidget* closeWidget=tabs->tabButton(0,QTabBar::RightSide);
        if(!closeWidget)closeWidget=tabs->tabButton(0,QTabBar::LeftSide);
        auto* close=qobject_cast<QAbstractButton*>(closeWidget);
        check(close!=nullptr,"Each deletable Page has a close icon");
        if(close)close->click();
    }
    check(deleteRequests==1 && designer.selectedPageId()=="main",
          "Closing an inactive tab selects the correct Page before Delete");
    designer.setRunning(true);
    check(tabs && !tabs->tabsClosable() && !tabs->isTabEnabled(3),
          "Page editing disabled while Running; navigation remains enabled");
    designer.setRunning(false);
    check(tabs && tabs->tabsClosable() && tabs->isTabEnabled(3),
          "Page editing restored after Stop");
    check(!PanelEventCatalog::carriesPersistentState("clicked") &&
          PanelEventCatalog::carriesPersistentState("valueChanged") &&
          PanelEventCatalog::carriesPersistentState("toggled"),
          "Button click argument not stored as caption; Slider and Toggle state retained");
    // A button click must pass a boolean argument without changing its label.
    PanelCanvas* canvas=designer.canvas();
    PanelControl button;
    button.id="runDemo";button.type="button";button.text="Increase counter";
    button.geometry=QRect(10,10,160,36);
    check(canvas->addControl(button),"Add test button");
    PanelControl trace;
    trace.id="traceMain";trace.type="plot";
    trace.geometry=QRect(220,45,400,300);
    check(canvas->addControl(trace),"Add persistent Panel XY chart");
    auto* plotWidget=canvas->findChild<PanelChartWidget*>("traceMain");
    PlotCommand plotUpdate;
    plotUpdate.plotId="traceMain";
    plotUpdate.seriesName="Captured CH1";
    plotUpdate.x={0,0.001,0.002,0.003,0.004,0.005};
    plotUpdate.y={0,0.25,0.5,-0.25,-0.5,0};
    canvas->applyPlotCommand(plotUpdate);
    check(plotWidget && plotWidget->pointCount("Captured CH1")==6,
          "Plot receives only explicitly addressed X/Y samples");
    bool received=false;QVariant eventPayload;
    QObject::connect(canvas,&PanelCanvas::controlEvent,&designer,
        [&](const QString& id,const QString& event,const QVariant& value){
            if(id=="runDemo" && event=="clicked") {received=true;eventPayload=value;}
        });
    canvas->setDesignMode(false);
    auto* runButton=canvas->findChild<QPushButton*>("runDemo");
    if(runButton)runButton->click();
    check(received && eventPayload.toBool(),"Button still emits true to callback");
    check(runButton && runButton->text()=="Increase counter",
          "Button caption unchanged after click");
    const QJsonArray mainControls=canvas->toJson();
    QJsonArray invalidControls{widget("invalidControl")};
    QJsonObject invalid=invalidControls.at(0).toObject();
    invalid.insert("type","unknown_type");
    invalidControls.replace(0,invalid);
    QString invalidError;
    check(!PanelCanvas::validateJson(invalidControls,&invalidError) && !invalidError.isEmpty(),
          "Validate unsupported widget types before replacing current project");
    check(canvas->activatePage("main",mainControls,QSize(900,550)),"Adopt existing Main Page widgets");
    check(canvas->activatePage("measure",QJsonArray{widget("otherButton")},QSize(1024,768)),
          "First visit to Measurements creates its widgets once");
    auto* other=canvas->findChild<QPushButton*>("otherButton");
    check(runButton && runButton->isHidden() && other && !other->isHidden(),
          "Main widgets are hidden and Measurement widgets are shown");
    check(plotWidget && plotWidget->isHidden() && plotWidget->pointCount("Captured CH1")==6,
          "Hidden plot retains its own series");
    plotUpdate.y={1,2,3,4,5,6};
    canvas->applyPlotCommand(plotUpdate);
    check(plotWidget && plotWidget->pointCount("Captured CH1")==6,
          "Hidden plot receives explicit script updates without rebuilding");
    canvas->applyValue("runDemo",QStringLiteral("Runtime caption"));
    check(canvas->activatePage("main",mainControls,QSize(900,550)),
          "Return to Main Page without rebuilding Qt widgets");
    check(canvas->findChild<QPushButton*>("runDemo")==runButton &&
          runButton && runButton->text()=="Runtime caption" && !runButton->isHidden(),
          "Same Button QWidget preserves runtime ui.set on hidden Page");
    check(canvas->findChild<PanelChartWidget*>("traceMain")==plotWidget &&
          plotWidget && plotWidget->pointCount("Captured CH1")==6 &&
          !plotWidget->isHidden(),
          "Returning to Page preserves the very same plot and explicit X/Y samples");
    check(other && other->isHidden(),"Inactive Measurement widgets remain hidden");
    canvas->restoreDocumentValues(mainControls);
    check(runButton && runButton->text()=="Increase counter",
          "Stopping runtime restores original Button caption without reconstruction");
    canvas->activatePage("measure",QJsonArray{},QSize(1024,768));
    check(canvas->findChild<QPushButton*>("otherButton")==other && !other->isHidden(),
          "Repeated switching preserves original native QWidget pointers");
    canvas->activatePage("main",mainControls,QSize(900,550));
    canvas->forgetPage("measure");
    check(!canvas->findChild<QPushButton*>("otherButton"),
          "Deleting a Page destroys its widgets");

    designer.canvas()->setReservedIds(QSet<QString>{QStringLiteral("buttonMain")});
    check(!designer.canvas()->addControl(PanelControl{QStringLiteral("buttonMain"),QStringLiteral("button"),QStringLiteral("Duplicate"),QRect(0,0,100,36)}),"Reject cross-Page duplicate at canvas");
    check(designer.canvas()->uniqueId("button")=="button1","Auto IDs remain available for new Page");
    check(designer.findChild<QPushButton*>("addPanelPageButton")==nullptr,
          "No redundant Page management buttons");
    if(failures==0)qInfo()<<"PASS: v5.6.28 centered Page tabs, persistent plot and native QWidget state";
    return failures?1:0;
}
