// WFScript v0.2-dev.8.9.8.2 — offscreen operator-view and live title smoke test.
// Uses the built-in simulation Panel: no instrument can be energized.
#include "MainWindow.h"
#include "PanelDesigner.h"
#include "PanelCanvas.h"
#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDockWidget>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <functional>
#include <QThread>
#include <QDebug>

static int fail(const char* message) {
    qCritical() << "OPERATOR VIEW FAIL:" << message;
    return 1;
}
static bool until(const std::function<bool()>& condition, int ms=6500) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed()<ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,35);
        if (condition()) return true;
        QThread::msleep(8);
    }
    return condition();
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    // Keep the simulated application's auto-Run preference out of the user's
    // real settings, even when this test runs on a Windows developer machine.
    QTemporaryDir settingsHome;
    if(!settingsHome.isValid())return fail("test settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsHome.path());
    MainWindow window;
    window.resize(1350,800);
    window.show();
    QCoreApplication::processEvents();

    auto* panel=window.findChild<PanelDesigner*>();
    auto* workspaces=window.findChild<QTabWidget*>("workspaceTabs");
    auto* runButton=window.findChild<QPushButton*>("panelRunButton");
    auto* bar=window.findChild<QWidget*>("operatorHeader");
    auto* title=window.findChild<QLabel*>("operatorProjectTitle");
    auto* pageTabs=window.findChild<QTabBar*>("panelPageTabs");
    auto* console=window.findChild<QPlainTextEdit*>("scriptOutputConsole");
    auto* consoleButton=window.findChild<QPushButton*>("operatorConsoleToggle");
    auto* consoleDock=window.findChild<QDockWidget*>("operatorConsoleDock");
    auto* operatorConsole=window.findChild<QPlainTextEdit*>("operatorConsoleOutput");
    auto* fitButton=window.findChild<QPushButton*>("operatorFitPage");
    auto* stopButton=window.findChild<QPushButton*>("operatorStop");
    auto* editorToolbar=window.findChild<QWidget*>("panelEditorToolbar");
    auto* f11=window.findChild<QAction*>("operatorFullScreenAction");
    auto* autoOnRun=window.findChild<QAction*>("operatorAutoOnRunAction");
    if(!panel || !workspaces || !bar || !title || !pageTabs || !console || !consoleButton || !fitButton ||
       !stopButton || !editorToolbar || !f11 || !autoOnRun || !consoleDock || !operatorConsole)
        return fail("required operator controls exist");
    if(bar->isVisible() || window.isFullScreen())return fail("editor opens normally");
    if(!runButton) {
        for(auto* candidate:panel->findChildren<QPushButton*>())
            if(candidate->text().contains("Run Panel"))runButton=candidate;
    }
    if(!runButton)return fail("Run Panel button exists");
    // Load a genuinely multi-Page, simulation-only project. Its display name
    // comes from GettingStarted.js; Page names come from the selected tab ID.
    panel->exampleRequested(QStringLiteral(":/wfscript/examples/GettingStarted.wfpanel"));
    QCoreApplication::processEvents();
    if(pageTabs->count()!=3 || pageTabs->tabData(0).toString()!=QStringLiteral("docs") ||
       pageTabs->tabData(1).toString()!=QStringLiteral("eventstour"))
        return fail("Getting Started test project exposes both Page IDs");
    const auto before=panel->canvas()->toJson();
    const auto beforeSize=panel->panelSize();
    // Explicit user opt-in for this test; ordinary application defaults to
    // automatically entering operator view after successful initialization.
    autoOnRun->setChecked(true);
    workspaces->setCurrentWidget(panel);
    runButton->click();
    if(!until([&]{return bar->isVisible() || !panel->running();}))
        return fail("runtime reaches operator view or terminates");
    if(!bar->isVisible() || !window.isFullScreen())
        return fail("successful Run enters full screen");
    if(editorToolbar->isVisible() || workspaces->tabBar()->isVisible())
        return fail("development chrome is hidden");
    if(console->isVisible())return fail("console starts closed in operator view");
    if(title->text()!=QStringLiteral("Getting Started / Guide") ||
       title->toolTip()!=title->text())
        return fail("header shows friendly Panel name and initially active Page");
    // Switching Pages in Run must update ONLY presentation, preserving
    // the same long-lived JavaScript runtime and current project state.
    pageTabs->setCurrentIndex(1);
    QCoreApplication::processEvents();
    if(title->text()!=QStringLiteral("Getting Started / Events Tutorial") || !panel->running())
        return fail("header follows current Page while shared runtime keeps running");
    pageTabs->setCurrentIndex(0);
    QCoreApplication::processEvents();
    if(title->text()!=QStringLiteral("Getting Started / Guide") || !panel->running())
        return fail("header restores original Page when switching back");
    if(panel->canvas()->toJson()!=before || panel->panelSize()!=beforeSize)
        return fail("operator entry must not alter persisted Panel geometry");

    // Full console output is retained and surfaced as a drawer.
    const QString savedText=console->toPlainText();
    consoleButton->click();
    QCoreApplication::processEvents();
    if(!consoleDock->isVisible() || consoleDock->isFloating() ||
       window.dockWidgetArea(consoleDock)!=Qt::RightDockWidgetArea ||
       console->isVisible() || operatorConsole->toPlainText().isEmpty())
        return fail("right-docked Console preserves the current-run output");
    consoleButton->click();
    QCoreApplication::processEvents();
    if(consoleDock->isVisible() || console->toPlainText()!=savedText || !panel->running())
        return fail("Console closes without losing output or stopping runtime");
    const bool firstFit=fitButton->isChecked();
    fitButton->click();
    if(panel->fitPageEnabled()==firstFit)return fail("operator Fit Page toggles native view");
    fitButton->click();
    if(panel->canvas()->toJson()!=before)return fail("Fit Page does not mutate the document");

    // F11 exits presentation WITHOUT stopping the script. The original
    // user workspace, designer toolbar and console visibility must return.
    f11->trigger();
    QCoreApplication::processEvents();
    if(window.isFullScreen() || bar->isVisible() || !panel->running())
        return fail("F11 returns to running editor, not Design mode");
    if(!editorToolbar->isVisible() || !workspaces->tabBar()->isVisible())
        return fail("editor controls restored");
    f11->trigger();
    if(!bar->isVisible() || !window.isFullScreen())
        return fail("F11 can reenter same live Panel without restarting it");
    stopButton->click();
    if(!until([&]{return !panel->running();},9000))
        return fail("Stop returns to Design only after cleanup");
    if(window.isFullScreen() || bar->isVisible() || panel->canvas()->toJson()!=before)
        return fail("Stop restores editor and saved Panel geometry");
    qInfo() << "PASS: operator Panel/Page titles, live Page switching, console, F11, Stop";
    return 0;
}
