// WFScript v5.6.9 workspace regression. Native Qt Widgets only; no AD3 I/O.
#include "MainWindow.h"
#include "PanelDesigner.h"
#include "ApiExplorer.h"
#include "ScriptEditor.h"
#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QLineEdit>

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

    qInfo() << "PASS: stable tab geometry; in-tab Explorer; visible panel tools; focus and restore";
    return 0;
}
