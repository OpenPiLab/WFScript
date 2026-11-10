// v5.6.17: no disk access, no DWF, no module execution during completion.
#include "ScriptEditor.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QStandardItemModel>
#include <QTextCursor>
#include <QTextBlock>
#include <cstdlib>

static void require(bool ok,const char* label) {
    if(!ok){qCritical()<<"FAIL:"<<label;std::exit(1);}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    ScriptEditor editor;
    editor.setModuleSourceResolver([](const QString& name) {
        return name==QStringLiteral("MathTools.mjs")
            ? QStringLiteral("/** Square a sample. */\nexport function square(x) {return x*x;}\n"
                             "export const tolerance=0.01;\nfunction hidden(){}\n")
            : QString();
    });
    editor.setPlainText(QStringLiteral(
        "const math = modules.load(\"MathTools.mjs\");\nmath"));
    QTextCursor c=editor.textCursor();c.movePosition(QTextCursor::End);
    editor.setTextCursor(c);
    require(editor.textCursor().block().text()==QStringLiteral("math"),
            "module alias caret must be on math expression");
    editor.resize(700,320);editor.show();editor.setFocus();
    QCoreApplication::processEvents();
    QKeyEvent dot(QEvent::KeyPress,Qt::Key_Period,Qt::NoModifier,QStringLiteral("."));
    QCoreApplication::sendEvent(&editor,&dot);
    QCoreApplication::processEvents();
    auto* completer=editor.findChild<QCompleter*>();
    require(completer,"completer available");
    if(!completer->popup()->isVisible())
        qCritical()<<"Module completion:"<<"prefix"<<completer->completionPrefix()
                   <<"candidates"<<completer->completionCount()
                   <<"text"<<editor.toPlainText();
    require(completer->popup()->isVisible(),"math. completion popup visible");
    QStringList names;
    for(int i=0;i<completer->completionCount();++i)
        if(completer->setCurrentRow(i))names<<completer->currentCompletion();
    require(names.contains(QStringLiteral("square")),"exported function visible");
    require(names.contains(QStringLiteral("tolerance")),"exported constant visible");
    require(!names.contains(QStringLiteral("hidden")),"private function hidden");
    qInfo()<<"PASS: .mjs export names offered in editor without executing module";
    return 0;
}
