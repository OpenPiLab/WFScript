#include "ModuleExportCatalog.h"
#include "ScriptLibraryBridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJSEngine>
#include <QVariantMap>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    int failed=0;
    auto check=[&](bool cond,const char* message) {
        if(!cond) { ++failed;qCritical().noquote()<<"FAIL:"<<message; }
        else qInfo().noquote()<<"PASS:"<<message;
    };
    const QString script=QStringLiteral(
        "const math = modules.load(\"SignalMath.mjs\");\n"
        "var classic = modules.load('Other.mjs');\n"
        "math.\n");
    check(ModuleExportCatalog::moduleForAlias(script,QStringLiteral("math"))==
          QStringLiteral("SignalMath.mjs"),"modules.load alias detected");
    check(ModuleExportCatalog::moduleForAlias(script,QStringLiteral("classic"))==
          QStringLiteral("Other.mjs"),"single quote load alias detected");
    check(ModuleExportCatalog::moduleForAlias(script+QStringLiteral("math = 42;\n"),
          QStringLiteral("math")).isEmpty(),"reassignment invalidates inference");
    check(ModuleExportCatalog::moduleForAlias(script,QStringLiteral("unknown")).isEmpty(),
          "unrelated variable has no type");
    const QString mod=QStringLiteral(
        "/** Square a number.\n * @param {number} x\n */\n"
        "export function square(x) { return x * x; }\n"
        "export async function fetchValue(a, b) { return a + b; }\n"
        "export const PI = 3.14;\n"
        "export class Calibrator {}\n"
        "const local=5;\n"
        "export { local as renamed };\n"
        "function hidden() { return -1; }\n");
    const auto members=ModuleExportCatalog::namedExports(mod);
    QStringList names;
    for(const auto& m:members)names<<m.item.name;
    check(names.contains(QStringLiteral("square")),"exported function completion");
    check(names.contains(QStringLiteral("fetchValue")),"async exported function completion");
    check(names.contains(QStringLiteral("PI")),"exported const completion");
    check(names.contains(QStringLiteral("Calibrator")),"exported class completion");
    check(names.contains(QStringLiteral("renamed")),"aliased named export completion");
    check(!names.contains(QStringLiteral("hidden")),"private function not completed");
    for(const auto& item:members)if(item.item.name==QStringLiteral("square")) {
        check(item.signature==QStringLiteral("square(x)"),"function signature preserved");
        check(item.item.description.contains(QStringLiteral("Square a number")),
              "JSDoc summary attached to function");
    }
    QJSEngine engine;
    ScriptLibraryBridge host(&engine,QString(),{
        {QStringLiteral("SignalMath.mjs"),mod},
        {QStringLiteral("TextHelpers.js"),QStringLiteral(
            "function later() { throw new Error('oops'); }\n")}});
    check(host.valid(),"embedded test libraries created");
    // Error stack normalization is asserted using the actual function error.
    engine.globalObject().setProperty(QStringLiteral("__wfLibraryHost"),engine.newQObject(&host));
    check(!engine.evaluate(ScriptLibraryBridge::bootstrapSource()).isError(),"bootstrap loads");
    check(!engine.evaluate(QStringLiteral("include('TextHelpers.js');")).isError(),"classic include still works");
    const auto err=engine.evaluate(QStringLiteral("later();"),QStringLiteral("MainPanel.js"));
    check(err.isError(),"embedded included library throws normally");
    const QString raw=err.property(QStringLiteral("stack")).toString();
    const QString display=host.displayStack(raw);
    check(display.contains(QStringLiteral("TextHelpers.js")),"trace keeps logical library name");
    check(!display.contains(QStringLiteral("WFScript-")),"embedded temp folder redacted in trace");
    return failed?1:0;
}
