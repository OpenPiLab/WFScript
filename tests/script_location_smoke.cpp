#include "ScriptErrorDetails.h"
#include "ScriptLibraryBridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJSEngine>
#include <QFileInfo>
#include <QVariantMap>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    int failures=0;
    auto expect=[&](bool ok,const char* label){
        if(ok)qInfo()<<"PASS"<<label;
        else {qCritical()<<"FAIL"<<label; ++failures;}
    };
    QJSEngine engine;
    QStringList frames;
    const auto bad=engine.evaluate("var n = 4;\nprint(\"x = \" n);",
                                   "Embedded_Libraries_Demo.js",1,&frames);
    expect(bad.isError(),"SyntaxError is produced");
    auto err=ScriptErrorFormatting::fromException(bad,"Embedded_Libraries_Demo.js",frames);
    expect(err.source=="Embedded_Libraries_Demo.js","Main script name preserved");
    expect(err.line==2,"Correct syntax error line");
    expect(err.column>0,"Syntax error column captured");
    // Event callbacks are parsed at startup but execute later.
    auto defined=engine.evaluate("var callback=function(){ throw new Error('click failed'); };",
                                 "Embedded_Libraries_Demo.js");
    expect(!defined.isError(),"Callback syntax valid at startup");
    const QJSValue invoked=engine.globalObject().property("callback").call();
    expect(invoked.isError(),"Runtime button callback throws");
    err=ScriptErrorFormatting::fromException(invoked,"Embedded_Libraries_Demo.js");
    expect(err.source=="Embedded_Libraries_Demo.js","Callback source preserved");
    expect(err.line==1,"Callback error line retained");
    const QVariantMap embedded{{"Calc.mjs",QStringLiteral("export function square(x) { throw new Error('module failed'); }\n")}};
    ScriptLibraryBridge library(&engine,QString(),embedded);
    expect(library.valid(),"Embedded module materialized");
    auto ns=library.loadModule("Calc.mjs");
    expect(!ns.isError(),"Module loaded");
    auto value=ns.property("square").call(QJSValueList{QJSValue(2)});
    expect(value.isError(),"Error from embedded module");
    auto moduleError=ScriptErrorFormatting::fromException(value,"Embedded_Libraries_Demo.js");
    auto logical=library.displayNameForPath(moduleError.source);
    expect(logical=="Calc.mjs","Module maps to embedded editor name");
    expect(ScriptErrorFormatting::sourceFromQt("file:%3Cpanel-script%3E")=="<panel-script>",
           "Legacy Qt file URL decoded");
    return failures?1:0;
}
