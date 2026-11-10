// v5.6.18: static module-export F1 lookup, no JavaScript execution or hardware.
#include "ModuleExportCatalog.h"
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

static void expect(bool condition,const char* message) {
    if(!condition) {qCritical()<<"FAIL"<<message;std::exit(EXIT_FAILURE);}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const QString library=QStringLiteral(
        "/** Module-level overview. */\n"
        "/**\n * Squares a number.\n * @param {number} value - Input.\n"
        " * @returns {number} Square.\n * @example\n"
        " * print(math.square(5));\n */\n"
        "export function square(value) { return value * value; }\n"
        "export const tolerance=0.01;\nfunction internalOnly() {}\n");
    int calls=0;
    const auto loader=[&](const QString& name) {
        ++calls;
        return name==QStringLiteral("Calc.mjs") ? library : QString();
    };
    const QString script=QStringLiteral(
        "var math = modules.load('Calc.mjs');\n"
        "var answer = math.square(5);\n"
        "var threshold=math.tolerance;\n"
        "// math.square(1) should not be documented inside comments\n"
        "var example='math.square(2)';\n");
    const int member=script.indexOf(QStringLiteral("math.square(5)"))+7;
    const auto resolved=ModuleExportCatalog::resolveAtCursor(script,member,loader);
    expect(resolved.has_value(),"F1 resolves exported function with popup closed");
    expect(resolved->alias==QStringLiteral("math"),"resolved alias retained");
    expect(resolved->module==QStringLiteral("Calc.mjs"),"module name retained");
    expect(resolved->member.signature==QStringLiteral("square(value)"),"signature resolved");
    const QString html=ModuleExportCatalog::helpHtml(*resolved);
    expect(html.contains(QStringLiteral("math.square")),"F1 shows expression");
    expect(html.contains(QStringLiteral("@param")),"F1 shows JSDoc parameters");
    expect(html.contains(QStringLiteral("@returns")),"F1 shows JSDoc result");
    expect(html.contains(QStringLiteral("Calc.mjs")),"F1 shows origin module");
    expect(ModuleExportCatalog::resolveAtCursor(script,script.indexOf(QStringLiteral("math.square(5)"))+1,loader)==std::nullopt,
           "caret on alias does not claim member documentation");
    expect(ModuleExportCatalog::resolveAtCursor(script,script.indexOf(QStringLiteral("math.tolerance"))+7,loader).has_value(),
           "exported constant can be documented");
    expect(!ModuleExportCatalog::resolveAtCursor(script,script.indexOf(QStringLiteral("// math.square"))+10,loader),
           "comments ignored");
    expect(!ModuleExportCatalog::resolveAtCursor(script,script.indexOf(QStringLiteral("'math.square"))+7,loader),
           "strings ignored");
    const QString reassigned=script+QStringLiteral("math = {};\nmath.square(4);\n");
    expect(!ModuleExportCatalog::resolveAtCursor(reassigned,reassigned.lastIndexOf(QStringLiteral("square"))+2,loader),
           "reassignment invalidates static resolution");
    const QString unknown=QStringLiteral("var math=modules.load('Calc.mjs');\nmath.notExported();");
    expect(!ModuleExportCatalog::resolveAtCursor(unknown,unknown.indexOf(QStringLiteral("notExported"))+4,loader),
           "unknown export gets no fake help");
    expect(calls>0,"source read without executing module");
    qInfo()<<"PASS: popup-independent module F1, signature, JSDoc, guards";
    return 0;
}
