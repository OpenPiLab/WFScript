#include "ApiDiagnostics.h"
#include "ApiExplorer.h"
#include "ScriptEditor.h"
#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <cstdlib>

static void require(bool ok,const char* reason) {
    if(!ok) {qCritical()<<"FAIL:"<<reason;std::exit(1);}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QString source=QStringLiteral(
        "// Scope.Channel1.Frequncy should not warn inside a comment\n"
        "const s = 'Wavegen.Channel1.Simple.Frequncy';\n"
        "Scope.Channel1.Range.value = 5;\n"
        "Wavegen.Channel1.Simple.Frequncy.value = 1000;\n"
        "Scope.Channel1.measure('RMS').toFixed(3);\n"
        "var ch = Wavegen.Channel1; ch.Simple.Frequncy.value = 2;\n"
        "/* Device.noSuchMember inside a block comment */\n");
    const auto issues=ApiDiagnostics::analyze(source);
    if(issues.size()!=1) {
        qCritical()<<"Diagnostic issue count:"<<issues.size();
        for(const auto& issue:issues)
            qCritical()<<"Issue"<<issue.line<<issue.path<<issue.message;
    }
    require(issues.size()==1,"exactly one warning on explicit unknown member");
    require(issues.first().line==4,"correct line for unknown member");
    require(issues.first().suggestion==QStringLiteral("Frequency"),"suggested supported spelling");
    require(issues.first().path==QStringLiteral("Wavegen.Channel1.Simple.Frequncy"),"full path");
    require(ApiDiagnostics::analyze(QStringLiteral("Scope.Channel1.Range.value=5;")).isEmpty(),
            "known API path must be accepted");
    require(ApiDiagnostics::analyze(QStringLiteral("// Scope.Broken.foo\n'Wavegen.Broken.foo';")).isEmpty(),
            "comments and strings must be ignored");
    require(ApiDiagnostics::analyze(QStringLiteral(
        "const a='Wavegen.Channel1.Simple.Frequncy';\n"
        "const b=\"Wavegen.Channel1.Simple.Frequncy\";\n"
        "const c=`Wavegen.Channel1.Simple.Frequncy`;\n"
        "/* Wavegen.Channel1.Simple.Frequncy */\n")).isEmpty(),
        "quoted and template literals must not generate bogus diagnostics");
    ApiExplorer explorer;
    require(explorer.containsPath(QStringLiteral("Wavegen.Channel1.Simple.Frequency.value")),
            "explorer must contain supported frequency parameter");
    require(explorer.containsPath(QStringLiteral("Scope.Channel1.measure")),
            "explorer must contain supported measurement method");
    require(!explorer.containsPath(QStringLiteral("Wavegen.Channel1.Simple.Frequncy")),
            "unsupported symbols must not be advertised");
    ScriptEditor editor;editor.show();
    editor.setPlainText(source);
    QEventLoop loop;
    QTimer::singleShot(750,&loop,&QEventLoop::quit);
    loop.exec();
    require(editor.diagnostics().size()==1,"debounced editor diagnostics");
    editor.goToLine(4);
    require(editor.textCursor().blockNumber()==3,"problem navigation selects source line");
    editor.insertApiPath(QStringLiteral("Scope.Channel1.Range.value"));
    require(editor.toPlainText().contains(QStringLiteral("Scope.Channel1.Range.valueWavegen")),
            "API path insertion at caret");
    qInfo()<<"PASS: catalog-backed explorer, conservative diagnostics, navigation and insertion";
    return 0;
}
