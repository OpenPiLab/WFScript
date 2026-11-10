#include "ScriptLibraryBridge.h"
#include "ScriptErrorDetails.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariantMap>

// Reproduces include("TextHelpers.js") from an actual QJSEngine. These
// assertions test Qt's real filename/line metadata, not a fabricated parser.
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("WFScriptSmoke"));
    QCoreApplication::setApplicationName(QStringLiteral("ClassicIncludeDiagnostics"));
    QStandardPaths::setTestModeEnabled(true);
    int failures=0;
    auto expect=[&](bool ok,const char* label) {
        if (ok) qInfo().noquote()<<"PASS:"<<label;
        else {qCritical().noquote()<<"FAIL:"<<label;++failures;}
    };
    const QVariantMap embedded {
        {QStringLiteral("TextHelpers.js"), QStringLiteral(
            "var TextHelpers = { describe: function() { return 'Working'; } };\n"
            "function later() {\n"
            "  throw new Error('delayed include failure');\n"
            "}\n")},
        {QStringLiteral("BadSyntax.js"), QStringLiteral(
            "var n = 1;\n"
            "print('x = ' n);\n")},
        {QStringLiteral("BadStartup.js"), QStringLiteral(
            "var n = 1;\n"
            "coolerror;\n")},
        {QStringLiteral("Nested.js"), QStringLiteral(
            "include('TextHelpers.js');\n"
            "var Nested = 42;\n")},
        {QStringLiteral("Recursive.js"), QStringLiteral("include('Recursive.js');\n")},
        {QStringLiteral("OneTime.js"), QStringLiteral(
            "var inclusionCount = (typeof inclusionCount === 'undefined' ? 0 : inclusionCount) + 1;\n")},
        {QStringLiteral("Calc.mjs"), QStringLiteral(
            "export function square(x) { return x*x; }\n")}
    };
    QJSEngine engine;
    ScriptLibraryBridge host(&engine,QString(),embedded);
    QJSEngine::setObjectOwnership(&host,QJSEngine::CppOwnership);
    expect(host.valid(),"embedded test libraries created");
    engine.globalObject().setProperty(QStringLiteral("__wfLibraryHost"),engine.newQObject(&host));
    const auto boot=engine.evaluate(ScriptLibraryBridge::bootstrapSource(),QStringLiteral("<panel-bootstrap>"));
    expect(!boot.isError(),"shared include/modules bootstrap installed");

    auto result=engine.evaluate(QStringLiteral("include('TextHelpers.js'); TextHelpers.describe();"),
                                QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(!result.isError() && result.toString()==QStringLiteral("Working"),
           "classic included globals accessible in main script");
    result=engine.evaluate(QStringLiteral("later()"),QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(result.isError(),"delayed function exception propagates");
    auto detail=ScriptErrorFormatting::fromException(result,QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(host.displayNameForPath(detail.source)==QStringLiteral("TextHelpers.js"),
           "delayed function reports TextHelpers.js rather than eval code");
    expect(detail.line==3,"delayed function keeps original library line number");
    expect(detail.message.contains(QStringLiteral("delayed include failure")),"delayed message preserved");

    result=engine.evaluate(QStringLiteral("include('BadSyntax.js')"),
                           QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(result.isError(),"included syntax error aborts panel initialization");
    detail=ScriptErrorFormatting::fromException(result,QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(host.displayNameForPath(detail.source)==QStringLiteral("BadSyntax.js"),
           "syntax error reports included filename");
    expect(detail.line==2,"included syntax error reports original line 2");

    result=engine.evaluate(QStringLiteral("include('BadStartup.js')"),
                           QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(result.isError(),"included initialization ReferenceError propagated");
    detail=ScriptErrorFormatting::fromException(result,QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(host.displayNameForPath(detail.source)==QStringLiteral("BadStartup.js"),
           "included ReferenceError reports BadStartup.js");
    expect(detail.line==2,"included ReferenceError retains original line 2");

    result=engine.evaluate(QStringLiteral("include('Nested.js'); Nested;"),
                           QStringLiteral("Embedded_Libraries_Demo.js"));
    expect(!result.isError() && result.toInt()==42,"nested includes use one shared global scope");
    result=engine.evaluate(QStringLiteral("include('OneTime.js'); include('OneTime.js'); inclusionCount;"));
    expect(!result.isError() && result.toInt()==1,"classic include cached once per engine");
    result=engine.evaluate(QStringLiteral("include('Recursive.js')"));
    expect(result.isError() && result.toString().contains(QStringLiteral("Circular include")),
           "circular includes rejected with explicit error");
    result=engine.evaluate(QStringLiteral("include('../BadStartup.js')"));
    expect(result.isError() && result.toString().contains(QStringLiteral("relative")),
           "traversal still rejected");
    result=engine.evaluate(QStringLiteral("modules.load('Calc.mjs').square(4)"));
    expect(!result.isError() && result.toInt()==16,"native modules.load() still works");

    // Shared filesystem libraries must also retain a useful source identity.
    QTemporaryDir temp;
    expect(temp.isValid(),"temporary project directory created");
    QDir().mkpath(temp.path()+QStringLiteral("/libraries"));
    QFile f(temp.path()+QStringLiteral("/libraries/FileError.js"));
    expect(f.open(QIODevice::WriteOnly|QIODevice::Truncate) &&
           f.write("var a=1;\nthrow new Error('shared file error');\n")>0,
           "filesystem library fixture written");
    f.close();
    QFile projectFile(temp.path()+QStringLiteral("/App.js"));
    expect(projectFile.open(QIODevice::WriteOnly) && projectFile.write("// project\n")>0,
           "project file fixture created");
    projectFile.close();
    QJSEngine engine2;
    ScriptLibraryBridge sharedHost(&engine2,projectFile.fileName());
    QJSEngine::setObjectOwnership(&sharedHost,QJSEngine::CppOwnership);
    engine2.globalObject().setProperty(QStringLiteral("__wfLibraryHost"),engine2.newQObject(&sharedHost));
    expect(!engine2.evaluate(ScriptLibraryBridge::bootstrapSource()).isError(),"standalone include setup");
    result=engine2.evaluate(QStringLiteral("include('FileError.js')"),QStringLiteral("App.js"));
    detail=ScriptErrorFormatting::fromException(result,QStringLiteral("App.js"));
    expect(result.isError() && detail.source.contains(QStringLiteral("FileError.js")),
           "filesystem classic include reports its physical file");
    expect(detail.line==2,"filesystem include retains line number");
    return failures?1:0;
}
