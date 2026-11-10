// v5.7.0.11: test the current contextual Panel/Page lifecycle editor.
// No AD3 or running script.  The obsolete modal Panel Settings dialog was
// replaced by PageAppearanceEditor during the Panel UI redesign.
#include "MainWindow.h"
#include "PanelDesigner.h"
#include "ScriptEditor.h"
#include "panel/PageAppearanceEditor.h"
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QPushButton>
#include <QTabWidget>
#include <QDebug>

namespace {
int fail(const char* message) { qCritical()<<"FAIL:"<<message;return 1; }
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    MainWindow window;
    window.resize(1400,850);
    window.show();
    auto* panel=window.findChild<PanelDesigner*>();
    auto* tabs=window.findChild<QTabWidget*>(QStringLiteral("scriptEditorTabs"));
    auto* spaces=window.findChild<QTabWidget*>(QStringLiteral("workspaceTabs"));
    if(!panel || !tabs || !spaces)return fail("Panel/script workspaces missing");
    spaces->setCurrentWidget(panel);
    QCoreApplication::processEvents();
    auto* appearance=panel->findChild<PageAppearanceEditor*>();
    auto* initialize=panel->findChild<QCheckBox*>(QStringLiteral("enablePanelInitialize"));
    auto* shutdown=panel->findChild<QCheckBox*>(QStringLiteral("enablePanelShutdown"));
    auto* editInitialize=panel->findChild<QPushButton*>(QStringLiteral("editPanelInitialize"));
    auto* editShutdown=panel->findChild<QPushButton*>(QStringLiteral("editPanelShutdown"));
    if(!appearance || !initialize || !shutdown || !editInitialize || !editShutdown)
        return fail("Contextual Panel lifecycle controls missing");
    if(!initialize->isEnabled() || !shutdown->isEnabled())
        return fail("Lifecycle settings should be editable in Design mode");
    // Exercise the live MainWindow->PanelProject->PageAppearanceEditor path.
    initialize->setChecked(true);
    shutdown->setChecked(true);
    if(!initialize->isChecked() || !shutdown->isChecked())
        return fail("Lifecycle options cannot be enabled");
    editInitialize->click();
    QCoreApplication::processEvents();
    auto* editor=qobject_cast<ScriptEditor*>(tabs->currentWidget());
    if(!editor || !editor->toPlainText().contains(QStringLiteral("function onInitialize()")))
        return fail("Create/Go to onInitialize did not open the Panel editor");
    editShutdown->click();
    QCoreApplication::processEvents();
    editor=qobject_cast<ScriptEditor*>(tabs->currentWidget());
    if(!editor || !editor->toPlainText().contains(QStringLiteral("function onShutdown()")))
        return fail("Create/Go to onShutdown did not insert a template");
    const int count=editor->toPlainText().count(QStringLiteral("function onInitialize()"));
    editInitialize->click();
    if(editor->toPlainText().count(QStringLiteral("function onInitialize()"))!=count)
        return fail("Repeated Create/Go duplicated the hook");
    initialize->setChecked(false);
    shutdown->setChecked(false);
    spaces->setCurrentWidget(panel);
    QCoreApplication::processEvents();
    if(initialize->isChecked() || shutdown->isChecked())
        return fail("Lifecycle options were not retained in the inspector");
    qInfo()<<"PASS: contextual lifecycle settings, create/go hooks, no duplicates, state retention";
    return 0;
}
