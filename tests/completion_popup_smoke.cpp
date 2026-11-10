// WFScript v5.6.3: actual QPlainTextEdit/QCompleter interaction regression.
// No DWF hardware, QtQml or scripts are executed. Run with
// QT_QPA_PLATFORM=offscreen for CI, or run normally on Windows.
#include "ScriptEditor.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>
#include <cstdlib>

namespace {
void fail(const QString& message) {
    qCritical().noquote() << "FAIL:" << message;
    std::exit(1);
}
void check(bool condition, const QString& message) {
    if (!condition) fail(message);
}
void press(ScriptEditor& editor, Qt::Key key, const QString& text = {}) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(&editor, &event);
    QCoreApplication::processEvents();
}
void expectSelected(QCompleter* completer, const QString& prefix, const QString& label) {
    const auto* popup = completer->popup();
    check(popup->isVisible(), QStringLiteral("popup vanished at %1").arg(label));
    check(completer->completionPrefix() == prefix,
          QStringLiteral("wrong prefix at %1: got %2").arg(label, completer->completionPrefix()));
    check(completer->completionCount() > 0,
          QStringLiteral("no completions at %1").arg(label));
    const QModelIndex selected = popup->currentIndex();
    check(selected.isValid(), QStringLiteral("no highlighted row at %1").arg(label));
    check(popup->selectionModel() && popup->selectionModel()->isSelected(selected),
          QStringLiteral("highlight is not selected at %1").arg(label));
    const QString value = selected.data(Qt::EditRole).toString();
    check(value.startsWith(prefix, Qt::CaseInsensitive),
          QStringLiteral("selected completion doesn't match prefix at %1: %2")
              .arg(label, value));
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ScriptEditor editor;
    editor.resize(750, 350);
    editor.setPlainText(QStringLiteral(
        "function setup56(f1,amp1,f2,amp2) {\n"
        "  for (var i=1; i<=2; i++) {\n"
        "    var ch = i === 1 ? Wavegen.Channel1 : Wavegen.Channel2;\n"
        "    ch\n"
        "  }\n"
        "}\n"));
    // Put the cursor just after 'ch', inside the loop where the alias is in scope.
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, 3);
    cursor.movePosition(QTextCursor::EndOfLine);
    editor.setTextCursor(cursor);
    editor.show();
    editor.setFocus();
    QCoreApplication::processEvents();

    QCompleter* completer = editor.findChild<QCompleter*>();
    check(completer, "QCompleter was not created");

    press(editor, Qt::Key_Period, QStringLiteral("."));
    expectSelected(completer, QString(), QStringLiteral("ch."));

    press(editor, Qt::Key_S, QStringLiteral("S"));
    expectSelected(completer, QStringLiteral("S"), QStringLiteral("ch.S"));
    check(completer->popup()->currentIndex().data(Qt::EditRole).toString() ==
              QStringLiteral("Simple"), "ch.S first match should be Simple");

    press(editor, Qt::Key_I, QStringLiteral("i"));
    expectSelected(completer, QStringLiteral("Si"), QStringLiteral("ch.Si"));

    press(editor, Qt::Key_Backspace);
    expectSelected(completer, QStringLiteral("S"), QStringLiteral("ch.S after Backspace"));

    press(editor, Qt::Key_Backspace);
    expectSelected(completer, QString(), QStringLiteral("ch. after Backspace"));

    press(editor, Qt::Key_S, QStringLiteral("S"));
    expectSelected(completer, QStringLiteral("S"), QStringLiteral("ch.S retyped"));

    // When the completer receives Return, the current match should be inserted
    // without the user having to navigate with an arrow key first.
    press(editor, Qt::Key_Return, QStringLiteral("\r"));
    check(editor.textCursor().block().text().trimmed() == QStringLiteral("ch.Simple"),
          "Return did not insert selected 'Simple' completion");

    press(editor, Qt::Key_Period, QStringLiteral("."));
    expectSelected(completer, QString(), QStringLiteral("ch.Simple."));
    press(editor, Qt::Key_F, QStringLiteral("F"));
    expectSelected(completer, QStringLiteral("F"), QStringLiteral("ch.Simple.F"));
    check(completer->popup()->currentIndex().data(Qt::EditRole).toString() ==
              QStringLiteral("Frequency"), "ch.Simple.F should select Frequency");
    press(editor, Qt::Key_Tab, QStringLiteral("\t"));
    check(editor.textCursor().block().text().trimmed() == QStringLiteral("ch.Simple.Frequency"),
          "Tab did not insert selected 'Frequency' completion");

    qInfo() << "PASS: popup remains visible with one-character prefixes," \
               "first filtered result selected, backspace and Return/Tab accepted";
    return 0;
}
