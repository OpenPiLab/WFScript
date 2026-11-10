#include "ScriptLibraryBridge.h"
#include <QCoreApplication>
#include <QJSEngine>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDebug>

static bool check(bool cond,const QString& what) {
    if (!cond) qCritical().noquote()<<"FAIL:"<<what;
    else qInfo().noquote()<<"PASS:"<<what;
    return cond;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QCoreApplication::setOrganizationName("WFScriptSmoke");
    QCoreApplication::setApplicationName("LibrarySmoke");
    QStandardPaths::setTestModeEnabled(true);
    int failures=0;
    auto expect=[&](bool result,const QString& label){if(!check(result,label))++failures;};
    const QString userDir=ScriptLibraryBridge::userLibraryDirectory();
    expect(QDir(userDir).exists(),"Writable per-user library directory");
    expect(!ScriptLibraryBridge::validLibraryName("../outside.mjs"),"Traversal rejected");
    expect(!ScriptLibraryBridge::validLibraryName("C:/outside.mjs"),"Absolute path rejected");
    expect(ScriptLibraryBridge::validLibraryName("dsp/phase.mjs"),"Nested module names allowed");
    auto write=[](const QString& path,const QByteArray& data){
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate) && f.write(data)==data.size();
    };
    expect(write(userDir+"/classic.js","var Classic = { value: 7 };"),"Create shared classic library");
    expect(write(userDir+"/shared.mjs","export function value() { return 23; }"),"Create shared ES module");
    {
        QJSEngine engine;
        const QVariantMap embedded{
            {"classic.js",QStringLiteral("var Classic = { value: 41 };\n")},
            {"child.mjs",QStringLiteral("export const base = 20;\n")},
            {"parent.mjs",QStringLiteral("import {base} from './child.mjs'; export function value() {return base + 2;}\n")}
        };
        ScriptLibraryBridge host(&engine,QString(),embedded);
        QJSEngine::setObjectOwnership(&host,QJSEngine::CppOwnership);
        expect(host.valid(),"Materialized embedded libraries");
        engine.globalObject().setProperty("__wfLibraryHost",engine.newQObject(&host));
        auto setup=engine.evaluate(ScriptLibraryBridge::bootstrapSource(),"<bootstrap>");
        expect(!setup.isError(),"Bootstrap installed");
        auto result=engine.evaluate("include('./classic.js'); Classic.value;");
        expect(!result.isError() && result.toInt()==41,"Embedded include shadows shared library");
        result=engine.evaluate("var math = modules.load('parent.mjs'); math.value();");
        expect(!result.isError() && result.toInt()==22,"Native relative .mjs dependency");
        result=engine.evaluate("modules.load('parent.mjs') === math;");
        expect(!result.isError() && result.toBool(),"Module namespace cached");
        result=engine.evaluate("modules.load('shared.mjs').value();");
        expect(!result.isError() && result.toInt()==23,"Shared per-user ES module");
        result=engine.evaluate("include('../classic.js')");
        expect(result.isError(),"Reject parent traversal at JS API");
    }
    return failures ? 1 : 0;
}
