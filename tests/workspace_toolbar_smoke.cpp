// WFScript v5.6.23: native widget hierarchy and action affordance smoke test.
// Set QT_QPA_PLATFORM=offscreen; no AD3 connection or script execution required.
#include "MainWindow.h"
#include "PanelDesigner.h"
#include "ScriptEditor.h"
#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QWidget>

static int fail(const char* why) { qCritical() << "FAIL:" << why; return 1; }
static bool isAction(QAction* a, const QString& text) {
    return a && a->text().remove('&') == text;
}
static QPushButton* named(PanelDesigner* panel, const QString& label) {
    for (auto* button : panel->findChildren<QPushButton*>())
        if (button->text() == label) return button;
    return nullptr;
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow window;
    window.resize(1440, 860);
    window.show();
    QCoreApplication::processEvents();

    auto* workspaces = window.findChild<QTabWidget*>("workspaceTabs");
    auto* splitter = window.findChild<QSplitter*>("scriptWorkspaceSplitter");
    auto* editorPage = window.findChild<QWidget*>("scriptEditorPage");
    auto* editorTabs = window.findChild<QTabWidget*>("scriptEditorTabs");
    auto* scriptActions = window.findChild<QToolBar*>("scriptEditorToolbar");
    auto* devicePage = window.findChild<QWidget*>("deviceWorkspace");
    auto* panel = window.findChild<PanelDesigner*>();
    if (!workspaces || !splitter || !editorPage || !editorTabs ||
        !scriptActions || !devicePage || !panel)
        return fail("workspace, script toolbar, Device page and designer exist");
    if (splitter->indexOf(editorPage) != 1 || !editorPage->isAncestorOf(editorTabs) ||
        scriptActions->parentWidget() != editorPage || scriptActions->parentWidget() == &window)
        return fail("Script commands must be inside the Script Editor tab, not at window level");
    if (workspaces->widget(0) != devicePage || workspaces->widget(1) != splitter ||
        workspaces->currentWidget() != devicePage)
        return fail("Device is the startup workspace, Script is next");
    if (window.findChild<QToolBar*>("deviceConfigurationToolbar"))
        return fail("global device toolbar must not consume vertical space");
    const auto actions = scriptActions->actions();
    if (actions.size() != 6 || !isAction(actions[0], "New") ||
        !isAction(actions[1], "Open...") || !isAction(actions[2], "Save") ||
        !actions[3]->isSeparator() || !isAction(actions[4], "Run") ||
        !isAction(actions[5], "Stop"))
        return fail("Script actions have the correct order, text and separator");
    if (actions[0]->shortcut() != QKeySequence::New ||
        actions[1]->shortcut() != QKeySequence::Open ||
        actions[2]->shortcut() != QKeySequence::Save ||
        actions[4]->shortcut() != QKeySequence("F5") ||
        actions[5]->shortcut() != QKeySequence("Shift+F5"))
        return fail("keyboard shortcuts were preserved");
    for (auto* action : actions)
        if (!action->isSeparator() && action->icon().isNull())
            return fail("Script buttons have embedded icons");
    if(!devicePage->findChild<QPushButton*>("refreshDevicesButton") ||
       !devicePage->findChild<QPushButton*>("inspectDeviceCapabilities"))
        return fail("Device page exposes discovery and inspection actions");
    // Current Panel toolbar has Focus Canvas and Design Tools toggles; the
    // old Panel Settings dialog moved to the contextual Page/Panel inspector.
    for (const auto& text : {"New Panel", "Open Panel…", "Save/Export…",
                             "Edit JavaScript", "Libraries...", "Focus Canvas",
                             "Toolbox", "Properties"}) {
        auto* button = named(panel, QString::fromUtf8(text));
        if (!button || button->icon().isNull())
            return fail("Current Panel toolbar controls have labels and icons");
    }
    auto* sequence=panel->findChild<QPushButton*>("openVisualTestSequenceEditor");
    auto* designTools=panel->findChild<QPushButton*>("panelDesignToolsToggle");
    if(!sequence || sequence->text()!=QStringLiteral("Test Sequence...") || !designTools ||
       designTools->text()!=QStringLiteral("Design Tools") || !designTools->isCheckable())
        return fail("Test Sequence and contextual Design Tools remain accessible");
    const QRect workspaceRect = workspaces->geometry();
    workspaces->setCurrentWidget(panel);
    QCoreApplication::processEvents();
    if (scriptActions->isVisible() || !panel->toolboxShown() || !panel->propertiesShown())
        return fail("Switching to Panel does not expose script actions or hide designer tools");
    workspaces->setCurrentWidget(splitter);
    QCoreApplication::processEvents();
    if (!scriptActions->isVisible() || !qobject_cast<ScriptEditor*>(editorTabs->currentWidget()) ||
        workspaces->geometry() != workspaceRect)
        return fail("Script toolbar returns without changing outer workspace geometry");
    qInfo() << "PASS: in-tab Script actions, native icons, unchanged shortcuts, stable outer workspace";
    return 0;
}
