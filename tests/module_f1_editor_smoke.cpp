// v5.6.18: GUI F1 must show export docs after completion popup is closed.
#include "ScriptEditor.h"
#include <QApplication>
#include <QDialog>
#include <QTextBrowser>
#include <QCompleter>
#include <QAbstractItemView>
#include <QTimer>
#include <QDebug>
#include <cstdlib>

static void expect(bool ok,const char* message) {
    if(!ok){qCritical()<<"FAIL"<<message;std::exit(EXIT_FAILURE);}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    ScriptEditor editor;
    editor.setModuleSourceResolver([](const QString& name){
        if(name!=QStringLiteral("Calc.mjs"))return QString();
        return QStringLiteral("/**\n * Squares the input.\n * @param {number} value - Input.\n"
                              " * @returns {number} Square.\n */\n"
                              "export function square(value) {return value*value;}\n");
    });
    editor.setPlainText(QStringLiteral(
        "var math = modules.load(\"Calc.mjs\");\n"
        "var result = math.square(5);\n"));
    editor.resize(700,360);editor.show();editor.setFocus();
    QApplication::processEvents();
    if(auto* completer=editor.findChild<QCompleter*>())completer->popup()->hide();
    QTextCursor cursor=editor.textCursor();
    const int member=editor.toPlainText().indexOf(QStringLiteral("square(5)"))+2;
    cursor.setPosition(member);editor.setTextCursor(cursor);
    bool found=false,docsCorrect=false;
    QTimer::singleShot(0,&app,[&]{
        for(QWidget* w:QApplication::topLevelWidgets()) {
            auto* dlg=qobject_cast<QDialog*>(w);
            if(!dlg || !dlg->isVisible())continue;
            auto* browser=dlg->findChild<QTextBrowser*>();
            if(!browser)continue;
            found=true;
            const QString text=browser->toPlainText();
            docsCorrect=text.contains(QStringLiteral("math.square")) &&
                text.contains(QStringLiteral("Calc.mjs")) &&
                text.contains(QStringLiteral("Squares the input")) &&
                text.contains(QStringLiteral("@param"));
            dlg->accept();return;
        }
    });
    // Safety valve so a broken F1 dialog never hangs automated CI indefinitely.
    QTimer::singleShot(1500,&app,[]{
        for(QWidget* w:QApplication::topLevelWidgets())
            if(auto* dlg=qobject_cast<QDialog*>(w);dlg && dlg->isVisible())dlg->reject();
    });
    editor.showApiHelpAtCursor();
    expect(found,"F1 opened a help dialog after completion popup closed");
    expect(docsCorrect,"F1 dialog contains exported function and JSDoc");
    qInfo()<<"PASS: module F1 Qt dialog uses source-level module metadata";
    return 0;
}
