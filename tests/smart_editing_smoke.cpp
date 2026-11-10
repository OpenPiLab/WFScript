// WFScript v5.6.4: source-only editing tests; no AD3 or JS evaluation.
#include "ScriptEditor.h"
#include "EditorAssist.h"
#include <QApplication>
#include <QCompleter>
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>
#include <cstdlib>

static void check(bool ok, const char* message) {
    if (!ok) { qCritical() << "FAIL:" << message; std::exit(1); }
}
static void press(ScriptEditor& e, Qt::Key key, QString input={}, Qt::KeyboardModifiers mods=Qt::NoModifier) {
    QKeyEvent ev(QEvent::KeyPress,key,mods,input);
    QCoreApplication::sendEvent(&e,&ev);
    QCoreApplication::processEvents();
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    // Lexer and metadata: no execution, no guessed instrument values.
    check(EditorAssist::matchingBracket("a(b + (c))",1)==9,"outer bracket lookup");
    check(EditorAssist::matchingBracket("a(b + (c))",6)==8,"nested bracket lookup");
    check(EditorAssist::matchingBracket("'(' // )",1)<0,"ignore string bracket");
    check(!EditorAssist::inCode("var x=\"unterminated",int(QStringLiteral("var x=\"unterminated").size())),"string state");
    auto call=EditorAssist::callContext("Scope.Channel1.measureWindow(\"RMS\", 0.1, ");
    check(call.valid && call.apiPath=="Scope.Channel1.measureWindow" && call.argument==2,
          "signature active argument");
    check(!EditorAssist::signature("Scope1.Channel1.measureWindow",2).isEmpty(),
          "signature catalog");
    auto value=EditorAssist::valueContext("ch.Simple.Type.text = \"S");
    check(value.valid && value.typed=="S" && value.apiPath=="ch.Simple.Type.text",
          "enum text context");
    check(!EditorAssist::valueContext("// ch.Simple.Type.text = \"S").valid,
          "comment must not offer enum values");
    check(EditorAssist::enumValues("Wavegen1.Channel1.Simple.Type.text").contains("Sine"),
          "generator supported shape list");
    check(EditorAssist::enumValues("Scope.Channel2.SampleMode.text").contains("Min/Max"),
          "scope mode choices");

    ScriptEditor editor;
    editor.resize(800,430);editor.show();editor.setFocus();
    QCoreApplication::processEvents();
    editor.setPlainText({});
    press(editor,Qt::Key_BraceLeft,"{");
    check(editor.toPlainText()=="{}" && editor.textCursor().position()==1,"auto close brace");
    press(editor,Qt::Key_Return,"\r");
    check(editor.toPlainText()=="{\n    \n}","newlines between paired braces");
    check(editor.textCursor().block().text()=="    ","cursor stays on indented inner line");
    editor.setPlainText({});
    press(editor,Qt::Key_ParenLeft,"(");
    check(editor.toPlainText()=="()","auto close parenthesis");
    press(editor,Qt::Key_Backspace);
    check(editor.toPlainText().isEmpty(),"paired Backspace");

    editor.setPlainText("var x = 2;\n");
    QTextCursor c=editor.textCursor();c.movePosition(QTextCursor::Start);editor.setTextCursor(c);
    press(editor,Qt::Key_Slash,"/",Qt::ControlModifier);
    check(editor.toPlainText().startsWith("//var x = 2;"),"toggle comment on");
    press(editor,Qt::Key_Slash,"/",Qt::ControlModifier);
    check(editor.toPlainText().startsWith("var x = 2;"),"toggle comment off");

    editor.setPlainText("function f() {\n" 
                        "  var ch = Wavegen.Channel1;\n"
                        "  ch.Simple.Type.text = \n"
                        "}\n");
    c=editor.textCursor();c.movePosition(QTextCursor::Start);
    c.movePosition(QTextCursor::Down,QTextCursor::MoveAnchor,2);
    c.movePosition(QTextCursor::EndOfLine);editor.setTextCursor(c);
    press(editor,Qt::Key_QuoteDbl,"\"");
    check(editor.textCursor().block().text().endsWith("\"\""),"auto close quote");
    QCompleter* completer=editor.findChild<QCompleter*>();
    check(completer && completer->completionCount()>0,"enum choices popup appears");
    press(editor,Qt::Key_S,"S");
    check(completer->completionPrefix()=="S" && completer->completionCount()>0,
          "one-letter enum completion");
    check(completer->popup()->currentIndex().data(Qt::EditRole).toString()=="Sine",
          "correct selected enum value");
    press(editor,Qt::Key_Return,"\r");
    check(editor.textCursor().block().text().contains("Type.text = \"Sine\""),
          "accept enum without duplicating quote");
    qInfo() << "PASS: smart editing, lexical contexts, value hints, comment toggles, pairing";
    return 0;
}
