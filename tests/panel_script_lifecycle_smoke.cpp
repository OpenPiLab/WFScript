// v5.6.7 GUI regression: panel script survives editor tab closure.
// This test uses real MainWindow/PanelDesigner widgets but never opens DWF hardware.
#include "MainWindow.h"
#include "PanelDesigner.h"
#include "ScriptEditor.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPushButton>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QDebug>

namespace {
int fail(const char* what) { qCritical() << "FAIL:" << what; return 1; }

QPushButton* editButton(PanelDesigner* panel) {
    for (auto* b : panel->findChildren<QPushButton*>())
        if (b->text() == QStringLiteral("Edit JavaScript")) return b;
    return nullptr;
}
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    MainWindow window;
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("scriptEditorTabs"));
    auto* spaces = window.findChild<QTabWidget*>(QStringLiteral("workspaceTabs"));
    auto* panel = window.findChild<PanelDesigner*>();
    if (!tabs || !spaces || !panel) return fail("panel and script widgets created");
    auto* edit = editButton(panel);
    if (!edit) return fail("panel Edit JavaScript button exists");

    // v5.7: the default panel is Getting Started, and its script view is
    // deliberately lazy.  Open it via the same action as a real user.
    spaces->setCurrentWidget(panel);
    edit->click();
    QCoreApplication::processEvents();
    auto* script = qobject_cast<ScriptEditor*>(tabs->currentWidget());
    if (!script || !script->toPlainText().contains(
            QStringLiteral("WFScript Example: Getting Started")))
        return fail("Getting Started panel script opens from Edit JavaScript");
    const QString originalScript=script->toPlainText();

    // Editing must be captured before closing the view (without Save As .js).
    const QString marker = QStringLiteral("// LIFECYCLE_REOPEN_MARKER_567");
    QTextCursor cursor=script->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n")+marker+QStringLiteral("\n"));
    if (!script->document()->isModified()) return fail("panel script edit marks document modified");

    const int oldIndex=tabs->indexOf(script);
    if (oldIndex<0) return fail("panel view is in script tabs");
    if (!QMetaObject::invokeMethod(tabs,"tabCloseRequested",Qt::DirectConnection,Q_ARG(int,oldIndex)))
        return fail("tab close signal works");
    QCoreApplication::processEvents();
    for (int i=0; i<tabs->count(); ++i) {
        auto* visible=qobject_cast<ScriptEditor*>(tabs->widget(i));
        if (visible && visible->toPlainText().contains(marker))
            return fail("panel script tab was actually closed");
    }

    // The panel button must recreate the view with the *edited* source.
    spaces->setCurrentWidget(panel);
    edit->click();
    QCoreApplication::processEvents();
    if (spaces->currentWidget()!=spaces->widget(1))
        return fail("Edit JavaScript switches to Script Editor workspace");
    auto* recovered=qobject_cast<ScriptEditor*>(tabs->currentWidget());
    if (!recovered || !recovered->toPlainText().contains(marker))
        return fail("Edit JavaScript restores the exact edited source");
    if (!recovered->toPlainText().contains(originalScript))
        return fail("original Getting Started script also preserved");
    const int count=tabs->count();
    edit->click();
    if (tabs->count()!=count || tabs->currentWidget()!=recovered)
        return fail("repeated Edit JavaScript focuses one existing tab without duplicates");

    // Double-close and reopen: the project is the owner, not this editor instance.
    if (!QMetaObject::invokeMethod(tabs,"tabCloseRequested",Qt::DirectConnection,
                                   Q_ARG(int,tabs->indexOf(recovered))))
        return fail("second tab close");
    edit->click();
    auto* restored=qobject_cast<ScriptEditor*>(tabs->currentWidget());
    if (!restored || !restored->toPlainText().contains(marker))
        return fail("panel source preserved after repeated close/reopen");

    // At shutdown the panel's own unsaved-changes confirmation is expected;
    // do not close the window here (it would deliberately display a dialog).
    qInfo() << "PASS: panel editor closure/reopen, source retention, workspace navigation, no duplicates";
    return 0;
}
