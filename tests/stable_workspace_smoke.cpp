// WFScript v5.6.9 workspace regression. Native Qt Widgets only; no AD3 I/O.
#include "MainWindow.h"
#include "PanelDesigner.h"
#include "ApiExplorer.h"
#include "ScriptEditor.h"
#include "widgets/WidgetRegistry.h"
#include "PanelCanvas.h"
#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QDebug>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QLineEdit>
#include <QSet>
#include <QColor>
#include <QTreeWidgetItem>
#include <QJsonArray>
#include <QJsonObject>
#include <QPalette>
#include <QtGlobal>

namespace {
int fail(const char* message) { qCritical() << "FAIL:" << message; return 1; }
QPushButton* button(PanelDesigner* panel,const QString& label) {
    for (auto* item : panel->findChildren<QPushButton*>())
        if(item->text()==label) return item;
    return nullptr;
}
}

int main(int argc, char** argv) {
    QApplication app(argc,argv);
    MainWindow window;
    window.show();
    QCoreApplication::processEvents();
    auto* spaces=window.findChild<QTabWidget*>(QStringLiteral("workspaceTabs"));
    auto* tabs=window.findChild<QTabWidget*>(QStringLiteral("scriptEditorTabs"));
    auto* scriptPage=window.findChild<QSplitter*>(QStringLiteral("scriptWorkspaceSplitter"));
    auto* editorPage=window.findChild<QWidget*>(QStringLiteral("scriptEditorPage"));
    auto* apiPane=window.findChild<QWidget*>(QStringLiteral("scriptApiExplorerPane"));
    auto* explorer=window.findChild<ApiExplorer*>(QStringLiteral("instrumentApiExplorer"));
    auto* panel=window.findChild<PanelDesigner*>();
    if(!spaces || !tabs || !scriptPage || !editorPage || !apiPane || !explorer || !panel)
        return fail("all workspaces and the embedded API Explorer exist");
    if(spaces->widget(0)->objectName()!=QStringLiteral("deviceWorkspace") ||
        spaces->widget(1)!=scriptPage || scriptPage->indexOf(apiPane)!=0 ||
        scriptPage->indexOf(editorPage)!=1 || !editorPage->isAncestorOf(tabs) ||
        !apiPane->isAncestorOf(explorer))
        return fail("API Explorer and editors must share the Script workspace");
    if(window.findChild<QDockWidget*>(QStringLiteral("instrumentApiExplorerDock")))
        return fail("no global API Explorer dock allowed");
    auto* console=window.findChild<QPlainTextEdit*>(QStringLiteral("scriptOutputConsole"));
    auto* outputToggle=window.findChild<QAction*>(QStringLiteral("toggleScriptOutput"));
    if(!console || !outputToggle || !console->isVisible())
        return fail("global output stays visible at startup");
    if(!explorer->containsPath(QStringLiteral("Wavegen.Channel1.Simple.Frequency.value")))
        return fail("existing API catalog remains usable");
    auto* toolbox=button(panel,QStringLiteral("Toolbox"));
    auto* properties=button(panel,QStringLiteral("Properties"));
    auto* focus=button(panel,QStringLiteral("Focus Canvas"));
    auto* edit=button(panel,QStringLiteral("Edit JavaScript"));
    if(!toolbox || !properties || !focus || !edit)
        return fail("simple Panel Designer toolbar buttons exist");

    spaces->setCurrentWidget(panel);
    QCoreApplication::processEvents();
    if(!panel->toolboxShown() || !panel->propertiesShown() || panel->focusMode())
        return fail("Toolbox and Properties must be shown on initial Panel visit");
    if(apiPane->isVisible())
        return fail("Script API Explorer must not occupy Panel workspace");
    // v5.6.65: groups are stable, searchable, and only leaf widgets are
    // draggable/addable (never a category heading).
    auto* widgetTree=panel->findChild<QTreeWidget*>(QStringLiteral("groupedWidgetToolbox"));
    auto* widgetSearch=panel->findChild<QLineEdit*>(QStringLiteral("toolboxSearch"));
    if(!widgetTree || !widgetSearch || widgetTree->topLevelItemCount()<5)
        return fail("grouped Toolbox and search input available");
    QTreeWidgetItem* testGroup=nullptr;
    for(int i=0;i<widgetTree->topLevelItemCount();++i)
        if(widgetTree->topLevelItem(i)->text(0)==QStringLiteral("Test Executive"))
            testGroup=widgetTree->topLevelItem(i);
    if(!testGroup || testGroup->childCount()<5 || !testGroup->data(0,Qt::UserRole).toString().isEmpty())
        return fail("Test Executive group holds test widgets but cannot itself be added");

    // Every registered widget factory must appear exactly once as a leaf.
    // This catches the v0.2-dev.7 missing Communications/Digital Instruments
    // categories even though their widget factories and examples worked.
    // Obtain a stable local copy for iterators.
    const QStringList registered=WidgetRegistry::instance().typeIds();
    const QSet<QString> expectedTypes(registered.cbegin(),registered.cend());
    QSet<QString> displayed;
    bool hasCommunications=false;
    bool hasDigitalInstruments=false;
    for(int i=0;i<widgetTree->topLevelItemCount();++i) {
        const auto* group=widgetTree->topLevelItem(i);
        if(!group->data(0,Qt::UserRole).toString().isEmpty())
            return fail("Toolbox category must not hold a widget type ID");
        const QColor foreground=group->foreground(0).color();
        const QColor background=group->background(0).color();
        if(!foreground.isValid() || !background.isValid() ||
            qAbs(qGray(foreground.rgb())-qGray(background.rgb()))<95)
            return fail("Toolbox category text must contrast before mouse hover");
        for(int j=0;j<group->childCount();++j) {
            const auto* leaf=group->child(j);
            const QString type=leaf->data(0,Qt::UserRole).toString();
            if(type.isEmpty() || displayed.contains(type) ||
               !WidgetRegistry::instance().factory(type) ||
               leaf->text(0)!=WidgetRegistry::instance().factory(type)->displayName())
                return fail("Toolbox entries must have unique, valid widget type and factory label");
            if(!(leaf->flags()&Qt::ItemIsDragEnabled))
                return fail("Every widget must be draggable from Toolbox");
            displayed.insert(type);
            if(type==QStringLiteral("communications_terminal"))
                hasCommunications=(group->text(0)==QStringLiteral("Communications"));
            if(type==QStringLiteral("uart_terminal"))
                hasDigitalInstruments=(group->text(0)==QStringLiteral("Digital Instruments"));
        }
    }
    if(displayed!=expectedTypes || !hasCommunications || !hasDigitalInstruments)
        return fail("Toolbox must include ALL registered widgets, including both terminals");
    if(expectedTypes.size()!=PanelCanvas::supportedTypes().size())
        return fail("WidgetRegistry and PanelCanvas type catalogs must agree");
    widgetSearch->setText(QStringLiteral("Communications Terminal"));
    int matchingTerminal=0;
    for(int i=0;i<widgetTree->topLevelItemCount();++i) {
        auto* group=widgetTree->topLevelItem(i);
        if(group->isHidden())continue;
        for(int j=0;j<group->childCount();++j)
            if(!group->child(j)->isHidden() &&
               group->child(j)->data(0,Qt::UserRole).toString()==QStringLiteral("communications_terminal"))
                ++matchingTerminal;
    }
    if(matchingTerminal!=1)
        return fail("Communications Terminal must be discoverable in Toolbox search");
    widgetSearch->setText(QStringLiteral("Run Information"));
    int visible=0;
    for(int i=0;i<widgetTree->topLevelItemCount();++i) {
        auto* group=widgetTree->topLevelItem(i);
        if(group->isHidden())continue;
        for(int j=0;j<group->childCount();++j)if(!group->child(j)->isHidden())++visible;
    }
    if(visible!=1)return fail("Toolbox search filters to one Run Information control");
    widgetSearch->clear();
    for(int i=0;i<widgetTree->topLevelItemCount();++i)
        if(widgetTree->topLevelItem(i)->isHidden())return fail("Clearing search shows categories");

    const QRect workRect=spaces->geometry();
    const QPoint editPos=edit->pos();
    focus->click();
    QCoreApplication::processEvents();
    if(!panel->focusMode() || panel->toolboxShown() || panel->propertiesShown())
        return fail("Focus must hide both side panes without changing document");
    if(edit->pos()!=editPos)
        return fail("Focus must not reposition panel action buttons");
    focus->click();
    QCoreApplication::processEvents();
    if(panel->focusMode() || !panel->toolboxShown() || !panel->propertiesShown())
        return fail("Focus toggle must restore original panes");
    properties->click();
    if(panel->propertiesShown() || !panel->toolboxShown())
        return fail("Properties button must hide only Properties");
    properties->click();
    if(!panel->propertiesShown()) return fail("Properties button must reopen its pane");
    toolbox->click();
    if(panel->toolboxShown()) return fail("Toolbox button must hide toolbox");
    focus->click(); focus->click();
    if(panel->toolboxShown() || !panel->propertiesShown())
        return fail("Focus preserves user-hidden panes");
    panel->resetUiLayout();
    if(!panel->toolboxShown() || !panel->propertiesShown())
        return fail("Reset restores both panes");

    spaces->setCurrentWidget(scriptPage);
    QCoreApplication::processEvents();
    if(!apiPane->isVisible()) return fail("API Explorer visible in Script workspace");
    spaces->setCurrentWidget(panel);
    QCoreApplication::processEvents();
    if(spaces->geometry()!=workRect)
        return fail("outer workspace geometry unchanged after tab switches");
    if(!panel->toolboxShown() || !panel->propertiesShown())
        return fail("Panel side panes unaffected by tab switching");
    outputToggle->setChecked(false);
    if(console->isVisible()) return fail("manual Output toggle hides console");
    spaces->setCurrentWidget(scriptPage);
    spaces->setCurrentWidget(panel);
    if(console->isVisible()) return fail("Output stays hidden through tab switching");
    outputToggle->setChecked(true);
    if(!console->isVisible()) return fail("manual Output toggle restores console");
    spaces->setCurrentWidget(scriptPage);
    if(!qobject_cast<ScriptEditor*>(tabs->currentWidget()))
        return fail("script tabs remain usable");

    // v0.2-dev.7.4: the Run Panel action itself must focus the canvas.
    // This exercises the real MainWindow Run/Stop path in Simulation; the
    // existing focus button checks above test manual focus separately.
    spaces->setCurrentWidget(panel);
    QCoreApplication::processEvents();
    auto* panelRun=button(panel,QStringLiteral("▶ Run Panel"));
    auto* panelStop=button(panel,QStringLiteral("■ Stop / Design"));
    if(!panelRun || !panelStop || panel->focusMode())
        return fail("Run/Stop Panel buttons and starting unfocused state");
    panelRun->click();
    if(!panel->focusMode() || panel->toolboxShown() || panel->propertiesShown() ||
       spaces->currentWidget()!=panel || panelRun->isEnabled())
        return fail("Run Panel automatically focuses the live canvas");
    QElapsedTimer timer;
    timer.start();
    while(timer.elapsed()<3500 &&
          !console->toPlainText().contains(QStringLiteral("Runtime Running"))) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,30);
        QThread::msleep(5);
    }
    if(!console->toPlainText().contains(QStringLiteral("Runtime Running")))
        return fail("Simulation Panel runtime starts before automatic-focus test Stop");
    panelStop->click();
    timer.restart();
    while(timer.elapsed()<4500 && !panelRun->isEnabled()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,30);
        QThread::msleep(5);
    }
    if(!panelRun->isEnabled() || panel->focusMode() ||
       !panel->toolboxShown() || !panel->propertiesShown())
        return fail("Stop restores original unfocused designer layout");
    // v0.2-dev.8.1: reproduce the reported newly-created variable bug.
    // A project definition edit must seed PanelDesigner::m_runtimeData, or
    // updates for a new name are silently discarded despite Data.set() working
    // in the worker. Use MainWindow's real panelDataEdited signal connection.
    auto* dataTree=panel->findChild<QTreeWidget*>(QStringLiteral("panelDataVariables"));
    if(!dataTree)return fail("Panel Data inspector tree exists");
    const QJsonArray definitions{
        QJsonObject{{"name","Demo.NewVoltage"},{"type","number"},{"initial",0.0}},
        QJsonObject{{"name","Demo.Status"},{"type","text"},{"initial",QStringLiteral("Ready")}}
    };
    panel->panelDataEdited(definitions);
    QCoreApplication::processEvents();
    if(dataTree->topLevelItemCount()!=2 ||
       dataTree->topLevelItem(0)->text(2)!=QStringLiteral("0") ||
       dataTree->headerItem()->text(2)!=QStringLiteral("Initial Value"))
        return fail("new Panel Data definitions immediately appear with correct defaults");
    panel->setRunning(true);
    panel->setPanelDataRuntimeValue(QStringLiteral("Demo.NewVoltage"),3.3);
    panel->setPanelDataRuntimeValue(QStringLiteral("Demo.Status"),QStringLiteral("Updated"));
    if(dataTree->topLevelItemCount()!=2 ||
       dataTree->headerItem()->text(2)!=QStringLiteral("Live Value") ||
       dataTree->topLevelItem(0)->text(2)!=QStringLiteral("3.3") ||
       dataTree->topLevelItem(1)->text(2)!=QStringLiteral("Updated"))
        return fail("new variable values must update the running Property Editor");
    // Repeated live values should not destroy/recreate selected rows.
    auto* selected=dataTree->topLevelItem(0);
    dataTree->setCurrentItem(selected);
    panel->setPanelDataRuntimeValue(QStringLiteral("Demo.NewVoltage"),4.2);
    panel->setPanelDataRuntimeValue(QStringLiteral("Missing.Variable"),17);
    if(dataTree->currentItem()!=selected ||
       selected->text(2)!=QStringLiteral("4.2") || dataTree->topLevelItemCount()!=2)
        return fail("live updates preserve selection and ignore undefined names");
    panel->setRunning(false);
    if(dataTree->headerItem()->text(2)!=QStringLiteral("Initial Value") ||
       dataTree->topLevelItem(0)->text(2)!=QStringLiteral("0"))
        return fail("Stop restores initial-value display without saving runtime values");
    qInfo() << "PASS: stable workspace, all Toolbox widgets, Run Focus, new variable live values and default reset";
    return 0;
}
