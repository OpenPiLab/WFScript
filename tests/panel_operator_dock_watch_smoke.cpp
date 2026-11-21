// WFScript v0.2-dev.8.9.8.1 — native offscreen Console and Panel Data Watch test.
// Runs PanelDataLab in simulation only. No Digilent hardware operations.
#include "MainWindow.h"
#include "PanelDesigner.h"
#include "PanelCanvas.h"
#include <QApplication>
#include <QAction>
#include <QAbstractItemView>
#include <QFont>
#include <QTextDocument>
#include <QPalette>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QMainWindow>
#include <QCoreApplication>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDebug>
#include <functional>

static int fail(const char* message) {
    qCritical() << "DOCK/WATCH FAIL:" << message;
    return 1;
}
static bool until(const std::function<bool()>& ready,int timeoutMs=6500) {
    QElapsedTimer timer;
    timer.start();
    while(timer.elapsed()<timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,40);
        if(ready())return true;
        QThread::msleep(10);
    }
    return ready();
}
static QTreeWidgetItem* findRow(QTreeWidget* tree,const QString& name) {
    for(int i=0;i<tree->topLevelItemCount();++i) {
        auto* row=tree->topLevelItem(i);
        if(row->text(0)==name)return row;
    }
    return nullptr;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QTemporaryDir settings;
    if(!settings.isValid())return fail("isolated settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());

    MainWindow window;
    window.resize(1350,850);
    window.show();
    QCoreApplication::processEvents();
    auto* panel=window.findChild<PanelDesigner*>();
    auto* operatorHeader=window.findChild<QWidget*>("operatorHeader");
    auto* console=window.findChild<QPlainTextEdit*>("scriptOutputConsole");
    auto* consoleView=window.findChild<QPlainTextEdit*>("operatorConsoleOutput");
    auto* consoleDock=window.findChild<QDockWidget*>("operatorConsoleDock");
    auto* watchDock=window.findChild<QDockWidget*>("operatorDataWatchDock");
    auto* consoleToggle=window.findChild<QPushButton*>("operatorConsoleToggle");
    auto* watchToggle=window.findChild<QPushButton*>("operatorDataWatchToggle");
    auto* consoleFloat=window.findChild<QPushButton*>("operatorConsoleFloatToggle");
    auto* logScope=window.findChild<QComboBox*>("operatorLogScope");
    auto* logLevel=window.findChild<QComboBox*>("operatorLogLevel");
    auto* logWrap=window.findChild<QCheckBox*>("operatorLogWrap");
    auto* watchFloat=window.findChild<QPushButton*>("operatorDataWatchFloatToggle");
    auto* watchTree=window.findChild<QTreeWidget*>("operatorDataWatchTree");
    auto* watchFilter=window.findChild<QLineEdit*>("operatorDataWatchFilter");
    auto* watchPause=window.findChild<QCheckBox*>("operatorDataWatchPause");
    auto* f11=window.findChild<QAction*>("operatorFullScreenAction");
    auto* stop=window.findChild<QPushButton*>("operatorStop");
    if(!panel || !operatorHeader || !console || !consoleView || !consoleDock ||
       !watchDock || !consoleToggle || !watchToggle || !consoleFloat || !watchFloat ||
       !watchTree || !watchFilter || !watchPause || !logScope || !logLevel ||
       !logWrap || !f11 || !stop)
        return fail("all native operator tools exist");
    // Both dropdown POPUPS must have readable text before hover, even if
    // the Windows application palette or dark-mode theme changes.
    for(QComboBox* combo : {logScope,logLevel}) {
        auto* popup=combo->view();
        if(!popup || popup->styleSheet().isEmpty())
            return fail("Console dropdown popup has explicit theme styling");
        const QPalette palette=popup->palette();
        for(QPalette::ColorGroup group : {QPalette::Active,QPalette::Inactive}) {
            if(palette.color(group,QPalette::Text)!=QColor(QStringLiteral("#e7edf7")) ||
               palette.color(group,QPalette::Base)!=QColor(QStringLiteral("#102235")) ||
               palette.color(group,QPalette::HighlightedText)!=QColor(QStringLiteral("#ffffff")))
                return fail("Console dropdown has contrasting text in normal/selected states");
        }
    }
    // The normal console remains the sole log writer. This Run / severity
    // views use a separate filtered document, All History / All shares it.
    if(logScope->currentData().toString()!=QStringLiteral("current"))
        return fail("new operator console defaults to current Run session");

    // Use a real bundled project with seven typed variables and no physical I/O.
    panel->exampleRequested(QStringLiteral(":/wfscript/examples/PanelDataLab.wfpanel"));
    QCoreApplication::processEvents();
    const auto savedPanel=panel->canvas()->toJson();
    const auto savedSize=panel->panelSize();
    console->appendPlainText(QStringLiteral("[TEST] PRIOR-RUN-HISTORY-BOUNDARY"));
    panel->runRequested();
    if(!until([&]{return operatorHeader->isVisible() || !panel->running();}))
        return fail("simulation runtime enters operator view");
    if(!window.isFullScreen() || !operatorHeader->isVisible())
        return fail("operator view full screen");

    const QString log=console->toPlainText();
    consoleToggle->click();
    QCoreApplication::processEvents();
    if(!consoleDock->isVisible() || consoleDock->isFloating() ||
       window.dockWidgetArea(consoleDock)!=Qt::RightDockWidgetArea || console->isVisible())
        return fail("Console opens docked at the right without consuming Panel height");
    if(!consoleView->toPlainText().contains(QStringLiteral("[WFScript RUN #")) ||
       consoleView->toPlainText().contains(QStringLiteral("PRIOR-RUN-HISTORY-BOUNDARY")))
        return fail("This Run view includes only the latest session boundary and its output");
    logScope->setCurrentIndex(logScope->findData(QStringLiteral("all")));
    logLevel->setCurrentIndex(logLevel->findData(QStringLiteral("all")));
    if(consoleView->document()!=console->document() || consoleView->toPlainText()!=log)
        return fail("All History / All shares complete original Console document");
    console->appendPlainText(QStringLiteral("[TEST] same live output document"));
    if(!consoleView->toPlainText().contains(QStringLiteral("same live output document")))
        return fail("subsequent worker output appears in the shared Console document");
    logScope->setCurrentIndex(logScope->findData(QStringLiteral("current")));
    logLevel->setCurrentIndex(logLevel->findData(QStringLiteral("warnings")));
    console->appendPlainText(QStringLiteral("[Panel] normal progress message"));
    console->appendPlainText(QStringLiteral("[Panel WARNING] diagnostic warning"));
    console->appendPlainText(QStringLiteral("[Panel ERROR] diagnostic error"));
    if(!until([&]{return consoleView->toPlainText().contains(QStringLiteral("diagnostic error"));},1800))
        return fail("severity-filtered Console refreshes from original live log");
    if(consoleView->toPlainText().contains(QStringLiteral("normal progress message")) ||
       !consoleView->toPlainText().contains(QStringLiteral("diagnostic warning")))
        return fail("warning filter removes normal output but retains warnings/errors");
    // Compare actual document fonts while switching between shared and
    // filtered views. Widget-level setFont() alone is not sufficient.
    const QFont unfilteredFont=console->document()->defaultFont();
    const QFont filteredFont=consoleView->document()->defaultFont();
    if(unfilteredFont!=filteredFont || unfilteredFont!=consoleView->font())
        return fail("filtered and complete logs must use identical monospace font/size");
    logLevel->setCurrentIndex(logLevel->findData(QStringLiteral("errors")));
    if(consoleView->toPlainText().contains(QStringLiteral("diagnostic warning")) ||
       !consoleView->toPlainText().contains(QStringLiteral("diagnostic error")))
        return fail("error-only filter selects script errors");
    logWrap->setChecked(false);
    if(consoleView->lineWrapMode()!=QPlainTextEdit::NoWrap)return fail("no-wrap display mode");
    logWrap->setChecked(true);
    if(consoleView->lineWrapMode()!=QPlainTextEdit::WidgetWidth)return fail("wrap display mode");
    logLevel->setCurrentIndex(logLevel->findData(QStringLiteral("all")));
    logScope->setCurrentIndex(logScope->findData(QStringLiteral("all")));
    if(consoleView->document()!=console->document() ||
       consoleView->document()->defaultFont()!=filteredFont)
        return fail("restoring All History does not change Console log font");
    logScope->setCurrentIndex(logScope->findData(QStringLiteral("current")));
    if(consoleView->document()->defaultFont()!=filteredFont)
        return fail("returning to This Run preserves Console log font");

    consoleFloat->click();
    QCoreApplication::processEvents();
    if(!consoleDock->isFloating())return fail("Console can float and free Panel width");
    consoleFloat->click();
    QCoreApplication::processEvents();
    if(consoleDock->isFloating() || window.dockWidgetArea(consoleDock)!=Qt::RightDockWidgetArea)
        return fail("Console docks back to right-hand Diagnostics area");
    consoleDock->close();
    QCoreApplication::processEvents();
    if(consoleToggle->isChecked() || consoleDock->isVisible())
        return fail("closing the Console updates its toolbar action");
    consoleToggle->click();
    if(!consoleView->toPlainText().contains(QStringLiteral("diagnostic error")))
        return fail("reopened Console retains history and current filter");

    watchToggle->click();
    QCoreApplication::processEvents();
    if(!watchDock->isVisible() || watchDock->isFloating() ||
       window.dockWidgetArea(watchDock)!=Qt::RightDockWidgetArea ||
       !window.tabifiedDockWidgets(consoleDock).contains(watchDock) ||
       watchTree->topLevelItemCount()!=7)
        return fail("Watch tabs with Console on the right and exposes seven project variables");
    consoleDock->raise();
    QCoreApplication::processEvents();
    if(!watchToggle->isChecked() || !consoleToggle->isChecked())
        return fail("switching diagnostics tabs must not close hidden background tab");
    watchDock->raise();
    auto* rpm=findRow(watchTree,QStringLiteral("Motor.TargetRPM"));
    auto* ready=findRow(watchTree,QStringLiteral("Motor.Ready"));
    if(!rpm || !ready || rpm->text(1)!=QStringLiteral("number") ||
       rpm->text(2)!=QStringLiteral("1200") || ready->text(2)!=QStringLiteral("false"))
        return fail("typed initial values come from project-owned definitions");
    if(watchTree->editTriggers()!=QAbstractItemView::NoEditTriggers)
        return fail("watch must be read-only");

    // Exercise the established UI-to-PanelRuntime event route, not a private
    // test-only watch method. Its Value Binding writes the same Panel Data used
    // by Data.get, Data.onChange and native display bindings.
    panel->canvas()->controlEvent(QStringLiteral("targetInput"),
                                  QStringLiteral("valueChanged"),QVariant(1500.0));
    if(!until([&]{return rpm->text(2)==QStringLiteral("1500");}))
        return fail("live target input propagates to Panel Data Watch");
    if(rpm->text(3).toInt()<1)return fail("watch reports changed values");
    if(rpm->background(2).color()!=QColor(QStringLiteral("#275b50")))
        return fail("recently changed Panel Data value is visually highlighted");
    if(!until([&]{return rpm->background(2).color()!=QColor(QStringLiteral("#275b50"));},3000))
        return fail("recent-change highlight expires without user intervention");
    watchPause->setChecked(true);
    panel->canvas()->controlEvent(QStringLiteral("targetInput"),
                                  QStringLiteral("valueChanged"),QVariant(1700.0));
    if(!until([&]{return console->toPlainText().contains(QStringLiteral("1700"));},2000)) {
        // The operator Console may not print this particular event. Continue
        // pumping long enough to ensure the queued worker event was handled.
        until([]{return false;},400);
    }
    if(rpm->text(2)!=QStringLiteral("1500"))
        return fail("Pause freezes display without changing Panel runtime");
    watchPause->setChecked(false);
    if(!until([&]{return rpm->text(2)==QStringLiteral("1700");}))
        return fail("Resume reveals the latest live Panel Data value");
    watchFilter->setText(QStringLiteral("Motor.TargetRPM"));
    QCoreApplication::processEvents();
    if(rpm->isHidden() || !ready->isHidden())
        return fail("Watch filter affects display only");
    watchFloat->click();
    QCoreApplication::processEvents();
    if(!watchDock->isFloating())return fail("Watch can float to a second monitor");
    watchFloat->click();
    if(watchDock->isFloating() || window.dockWidgetArea(watchDock)!=Qt::RightDockWidgetArea ||
       !window.tabifiedDockWidgets(consoleDock).contains(watchDock))
        return fail("Watch can rejoin the tabbed Diagnostics area");

    // F11 is strictly a view change: worker continues, Page is unchanged.
    f11->trigger();
    QCoreApplication::processEvents();
    if(window.isFullScreen() || !panel->running() || consoleDock->isVisible() || watchDock->isVisible())
        return fail("F11 restores editor but does not stop the Panel");
    f11->trigger();
    QCoreApplication::processEvents();
    if(!window.isFullScreen() || !consoleToggle->isChecked() || !watchToggle->isChecked() ||
       !consoleDock->isVisible() && !watchDock->isVisible())
        return fail("F11 restores tabbed diagnostic tools during same run");
    stop->click();
    if(!until([&]{return !panel->running();},9500))
        return fail("Stop completes cleanup and restores Design mode");
    if(window.isFullScreen() || consoleDock->isVisible() || watchDock->isVisible() ||
       panel->canvas()->toJson()!=savedPanel || panel->panelSize()!=savedSize)
        return fail("Stopping preserves Page and hides operator tools");
    QSettings persisted(QStringLiteral("WFScript"),QStringLiteral("WFScript"));
    if(!persisted.contains(QStringLiteral("operator/diagnostics/rightWidth")) ||
       persisted.value(QStringLiteral("operator/diagnostics/logScope")).toString()!=QStringLiteral("current") ||
       !persisted.value(QStringLiteral("operator/diagnostics/wordWrap")).toBool())
        return fail("console scope, wrap and dock width persist outside .wfpanel");
    qInfo() << "PASS: tabbed right diagnostics, floating, per-run filtering, wrap, highlighting, persistence, F11 and Stop";
    return 0;
}
