// WFScript syntax-highlighting regression. Requires Qt6::Gui only; no AD3.
#include "JavaScriptHighlighter.h"

#include <QGuiApplication>
#include <QColor>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextCursor>
#include <tuple>
#include <QTextLayout>
#include <cstdio>

namespace {
int failures = 0;

void expectColor(const QTextDocument& document, int line, const QString& token,
                 const QColor& expected, int occurrence = 0) {
    const QTextBlock block = document.findBlockByNumber(line);
    if (!block.isValid()) {
        std::fprintf(stderr, "FAIL: block %d not present\n", line);
        ++failures;
        return;
    }
    int pos = -1;
    for (int i = 0; i <= occurrence; ++i) pos = block.text().indexOf(token, pos + 1);
    if (pos < 0) {
        std::fprintf(stderr, "FAIL: token '%s' not found in line %d\n",
                     token.toUtf8().constData(), line);
        ++failures;
        return;
    }
    QColor actual;
    for (const auto& range : block.layout()->formats()) {
        if (pos >= range.start && pos < range.start + range.length)
            actual = range.format.foreground().color();
    }
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: line %d token '%s', got %s, wanted %s\n",
                     line, token.toUtf8().constData(), actual.name().toUtf8().constData(),
                     expected.name().toUtf8().constData());
        ++failures;
    }
}

void run(QString text, bool dark, const QVector<std::tuple<int, QString, QColor>>& checks) {
    QTextDocument doc;
    JavaScriptHighlighter highlighter(&doc);
    highlighter.setDarkTheme(dark);
    doc.setPlainText(text);
    highlighter.rehighlight();
    for (const auto& item : checks)
        expectColor(doc, std::get<0>(item), std::get<1>(item), std::get<2>(item));
}
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    const QColor purple("#c792ea"), blue("#82cfff"), green("#c3e88d"),
                 grey("#7d8c98"), global("#82aaff"), property("#89ddc7"),
                 numeric("#f7bd73"), bracket("#d6b6ff");

    // Control-flow keywords are NOT function calls, even when followed by '(';
    // strings containing comment markers must not start comments.
    run(QStringLiteral("if(flag) for(var i=0;i<2;i++) print('x');\n"
                       "const url = \"https://server/a/*b*/\"; // comment\n"
                       "const fake = '/* not a comment */'; Scope.Channel1.Range.value = 5;"), true,
        {{0, "if", purple}, {0, "for", purple}, {0, "var", purple},
         {0, "print", blue}, {1, "https://", green}, {1, "/*b*/", green},
         {1, "// comment", grey}, {2, "/* not a comment */", green},
         {2, "Scope", global}, {2, "value", property}, {2, "5", numeric}});

    // Multiple block comments per line, multiline comments, and a fresh
    // quoted string after a closing comment.
    run(QStringLiteral("/* opening\n"
                       "Scope1.run() and // are both comments\n"
                       "closing */ if (ready) Scope1.single(); /* again */\n"
                       "const r = /https?:\\/\\/a\\/b/g; var q = 6 / 2;"), true,
        {{0, "opening", grey}, {1, "Scope1.run()", grey},
         {2, "closing", grey}, {2, "if", purple}, {2, "single", blue},
         {2, "again", grey}, {3, "/https?", green}, {3, "var", purple},
         {3, "6", numeric}});

    // Interpolation remains actual code, including a nested template and an
    // object literal; surrounding multiline template text is a string.
    run(QStringLiteral("const s = `raw // line\n"
                       "${Math.max(1, Scope1.Channel1.measure('RMS'))} text\n"
                       "${({v: `inner ${2 + 3}`}).v} end`;\n"
                       "Wavegen.Channel1.Simple.Frequency.value = 1_000.5e-1;"), true,
        {{0, "raw //", green}, {1, "${", bracket}, {1, "Math", global},
         {1, "max", blue}, {1, "Scope1", global}, {1, "measure", blue},
         {1, "RMS", green}, {1, "text", green}, {2, "inner", green},
         {2, "2", numeric}, {2, "3", numeric}, {2, "end", green},
         {3, "Wavegen", global}, {3, "value", property}, {3, "1_000", numeric}});

    // Editor state propagation after rehighlighting an earlier block.
    {
        QTextDocument doc;
        JavaScriptHighlighter highlighter(&doc);
        doc.setPlainText(QStringLiteral("/* block\nif (flag)\n*/ for(x)"));
        highlighter.rehighlight();
        expectColor(doc, 1, QStringLiteral("if"), grey);
        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::Start);
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.insertText(QStringLiteral("// no block"));
        highlighter.rehighlight();
        expectColor(doc, 1, QStringLiteral("if"), purple);
    }

    run(QStringLiteral("if(flag) { Scope1.single(); const x = `v ${3}`; }"), false,
        {{0, "if", QColor("#803ab7")}, {0, "Scope1", QColor("#0758a7")},
         {0, "single", QColor("#04688f")}, {0, "3", QColor("#9b5b08")}});

    if (failures) {
        std::fprintf(stderr, "JavaScriptHighlighter: %d regression check(s) failed\n", failures);
        return 1;
    }
    std::puts("PASS: JavaScriptHighlighter keywords, strings, comments, regexp, templates, nesting, theme, edits");
    return 0;
}
