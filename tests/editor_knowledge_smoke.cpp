#include "ScriptEditor.h"
#include "ApiKnowledge.h"
#include <QApplication>
#include <QCompleter>
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QTextCursor>
#include <QDebug>
#include <cstdlib>
static void check(bool ok,const char* msg) {if(!ok) {qCritical()<<"FAIL"<<msg;std::exit(1);}}
static void press(ScriptEditor& e,Qt::Key key,QString text={},Qt::KeyboardModifiers mod=Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress,key,mod,text);
    QCoreApplication::sendEvent(&e,&event);
    QCoreApplication::processEvents();
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    ScriptEditor ed;ed.resize(900,480);ed.show();ed.setFocus();
    ed.setPlainText(QStringLiteral("wgs"));
    QTextCursor snippetCursor=ed.textCursor();
    snippetCursor.movePosition(QTextCursor::End);
    ed.setTextCursor(snippetCursor);
    check(ed.textCursor().position()==3,"snippet prefix caret at end");
    press(ed,Qt::Key_Space," ",Qt::ControlModifier);
    auto* c=ed.findChild<QCompleter*>();
    check(c && c->completionCount()>0,"snippet completion not offered");
    if(c->popup()->currentIndex().data(Qt::EditRole).toString()!=QStringLiteral("wgsine"))
        qCritical()<<"Snippet selection:"<<c->popup()->currentIndex().data(Qt::EditRole)
                   <<"count:"<<c->completionCount()<<"prefix:"<<c->completionPrefix()
                   <<"visible:"<<c->popup()->isVisible();
    check(c->popup()->currentIndex().data(Qt::EditRole).toString()=="wgsine","snippet completion selection");
    press(ed,Qt::Key_Return,"\r");
    check(ed.toPlainText().contains("Wavegen.Channel1.Simple.Frequency.value = 1000;"),"snippet inserted");
    check(ed.textCursor().selectedText()=="1000","first editable snippet field selected");
    press(ed,Qt::Key_2,"2");
    check(ed.toPlainText().contains("Frequency.value = 2;"),"first snippet field edited");
    press(ed,Qt::Key_Tab,"\t");
    check(ed.textCursor().selectedText()=="1.0","Tab advances to second placeholder");
    check(ApiKnowledge::html(ApiKnowledge::lookup("Scope.Channel1.measure"),true).contains("RMS"),"help HTML");
    qInfo()<<"PASS: documented snippet completion, insertion and editable placeholders";
}
