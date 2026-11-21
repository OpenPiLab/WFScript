#include "ScriptUtilityBridge.h"
#include "BinaryBridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QVariantList>
#include <QDebug>

// Regression coverage for the actual QJSEngine -> Q_INVOKABLE boundary.
// A C++ QVariantMap alone does not exercise JS object marshalling.
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QJSEngine engine;
    ScriptUtilityBridge b(&engine, QStringLiteral("/virtual/wfscript/test-panel.wfpanel"), &app);
    BinaryBridge binary(&engine);
    QJSEngine::setObjectOwnership(&binary,QJSEngine::CppOwnership);
    engine.globalObject().setProperty(QStringLiteral("Binary"),engine.newQObject(&binary));
    b.setBinaryBridge(&binary);
    engine.globalObject().setProperty(QStringLiteral("fileHost"), engine.newQObject(&b));

    // v0.2-dev.5.1: one predictable directory shared between unrelated
    // .wfpanel paths. Saved, moved and built-in example Panels must agree.
    const QString expectedRoot=QDir::cleanPath(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        +QStringLiteral("/PanelData"));
    ScriptUtilityBridge other(&engine,QStringLiteral("/moved/new-path/other.wfpanel"),&app);
    ScriptUtilityBridge builtin(&engine,QString(),&app);
    if(b.dataDirectory()!=expectedRoot || other.dataDirectory()!=expectedRoot ||
       builtin.dataDirectory()!=expectedRoot || !QDir(expectedRoot).exists()) {
        qCritical()<<"Shared PanelData root mismatch"<<b.dataDirectory()<<expectedRoot;
        return 16;
    }
    const QString shared=b.dataPath(QStringLiteral("shared-cross-panel.txt"));
    if(shared!=other.dataPath(QStringLiteral("shared-cross-panel.txt")) ||
       shared!=builtin.dataPath(QStringLiteral("shared-cross-panel.txt")) ||
       QFileInfo(shared).absolutePath()!=expectedRoot ||
       !b.writeText(shared,QStringLiteral("Shared between panels")) ||
       other.readText(shared)!=QStringLiteral("Shared between panels") ||
       builtin.readText(shared)!=QStringLiteral("Shared between panels")) {
        qCritical()<<"Cross-Panel FileIO storage sharing failed";
        return 17;
    }
    // Do not automatically migrate, replace or delete prior hashed data.
    const QString legacy=expectedRoot+QStringLiteral("/e3b0c44298fc1c14");
    if(!QDir().mkpath(legacy))return 18;
    QFile sentinel(legacy+QStringLiteral("/legacy-preserve.txt"));
    if(!sentinel.open(QIODevice::WriteOnly) || sentinel.write("old")!=3)return 19;
    sentinel.close();
    if(!QFileInfo::exists(sentinel.fileName()))return 20;

    const QString csvPath=b.dataPath(QStringLiteral("fixture.csv"));
    if(csvPath.isEmpty() || !b.dataPath(QStringLiteral("../escape.csv")).isEmpty())return 1;
    const QVariantList sourceRows{
        QVariant(QVariantList{QStringLiteral("name"), QStringLiteral("value")}),
        QVariant(QVariantList{QStringLiteral("a,b"), QStringLiteral("say \"hi\"")}),
        QVariant(QVariantList{QStringLiteral("line\nbreak"), QStringLiteral("42")}),
        QVariant(QVariantList{QStringLiteral(""), QStringLiteral("")})
    };
    if(!b.writeCsv(csvPath,sourceRows)) {qCritical()<<"CSV write"<<b.lastError();return 2;}
    const QVariantList parsed=b.readCsv(csvPath);
    if(parsed.size()!=sourceRows.size() || parsed!=sourceRows) {
        qCritical()<<"CSV row shape or values changed"<<parsed;
        return 3;
    }
    // Test JS array shape as observed by the actual Panel runtime.
    engine.globalObject().setProperty(QStringLiteral("csvPath"), csvPath);
    const QJSValue jsCsv=engine.evaluate(QStringLiteral(
        "(function(){const r=fileHost.readCsvArray(csvPath);"
        "return Array.isArray(r)&&r.length===4&&Array.isArray(r[1])&&"
        "r[1][0]==='a,b'&&r[2][0]==='line\\nbreak'&&r[3][1]==='';})()"));
    if(jsCsv.isError() || !jsCsv.toBool()) {qCritical()<<"JS CSV round-trip"<<jsCsv.toString();return 4;}

    // This is the precise failure reported by the user: a literal JS object
    // passed directly to an invokable must serialize, including nested data.
    const QString jsonPath=b.dataPath(QStringLiteral("cfg.json"));
    engine.globalObject().setProperty(QStringLiteral("jsonPath"),jsonPath);
    const QJSValue writeObject=engine.evaluate(QStringLiteral(
        "fileHost.writeJson(jsonPath,{name:'Synthetic voltage',intervalMs:250,"
        "limits:{low:3.2,high:3.4},enabled:true,channels:[0,1,2]})"));
    if(writeObject.isError() || !writeObject.toBool()) {
        qCritical()<<"JS JSON object write"<<writeObject.toString()<<b.lastError();return 5;
    }
    const QVariantMap config=b.readJson(jsonPath).toMap();
    if(config.value(QStringLiteral("name")).toString()!=QStringLiteral("Synthetic voltage") ||
       config.value(QStringLiteral("intervalMs")).toInt()!=250 ||
       config.value(QStringLiteral("limits")).toMap().value(QStringLiteral("high")).toDouble()!=3.4 ||
       config.value(QStringLiteral("channels")).toList().size()!=3 ||
       !config.value(QStringLiteral("enabled")).toBool()) {
        qCritical()<<"JSON object round-trip failed"<<config;return 6;
    }
    const QJSValue writeArray=engine.evaluate(QStringLiteral(
        "fileHost.writeJson(jsonPath,[{id:'a'},['x',4],null])"));
    if(writeArray.isError() || !writeArray.toBool() || b.readJson(jsonPath).toList().size()!=3) {
        qCritical()<<"JSON top-level array round-trip"<<writeArray.toString()<<b.lastError();return 7;
    }
    const QJSValue invalid=engine.evaluate(QStringLiteral("fileHost.writeJson(jsonPath,42)"));
    if(invalid.isError() || invalid.toBool() || b.lastError().isEmpty())return 8;
    const QJSValue nullInput=engine.evaluate(QStringLiteral("fileHost.writeJson(jsonPath,null)"));
    if(nullInput.isError() || nullInput.toBool())return 9;

    // Invalid JSON writes must not damage the last good file on disk.
    if(b.readJson(jsonPath).toList().size()!=3)return 10;
    const QVariantList bytes{0,127,128,255};
    if(!b.writeBytes(b.dataPath(QStringLiteral("raw.bin")),bytes) ||
       b.readBytes(b.dataPath(QStringLiteral("raw.bin")))!=bytes)return 11;
    if(b.readText(QStringLiteral("/etc/passwd"))!=QString() || b.lastError().isEmpty())return 12;
    // RC1.3: native BinaryBuffer FileIO write/read/append uses the existing
    // shared PanelData permission and 2 MiB cap without JS byte arrays.
    const QString binPath=b.dataPath(QStringLiteral("rc13-roundtrip.bin"));
    engine.globalObject().setProperty(QStringLiteral("binPath"),binPath);
    const QJSValue binaryFile=engine.evaluate(QStringLiteral(R"JS(
        (function(){
            var packet=Binary.create(12);
            if(!packet.writeU32BE(0,0x89abcdef) || !packet.writeF32LE(4,3.125))return false;
            if(!fileHost.writeBuffer(binPath,packet))return false;
            if(!fileHost.appendBuffer(binPath,Binary.fromHex('aabbccdd')))return false;
            var restored=fileHost.readBuffer(binPath);
            return restored && restored.length===16 &&
                restored.readU32BE(0)===0x89abcdef &&
                restored.readF32LE(4)===3.125 &&
                restored.toHex(12,4)==='aabbccdd';
        })()
    )JS"));
    if(binaryFile.isError() || !binaryFile.toBool()){
        qCritical()<<"Native BinaryBuffer FileIO failed"<<binaryFile.toString()<<b.lastError();return 21;
    }
    if(b.readBuffer(QStringLiteral("/etc/passwd"))!=nullptr || b.lastError().isEmpty())return 22;


    engine.globalObject().setProperty(QStringLiteral("counter"),0);
    const QJSValue cb=engine.evaluate(QStringLiteral("(function(r){counter+=r.accepted?1:10;})"));
    const int id=b.request(QStringLiteral("question"),{},cb);
    if(!id)return 13;
    b.complete(id,{{QStringLiteral("accepted"),true},{QStringLiteral("value"),true}});
    if(engine.globalObject().property(QStringLiteral("counter")).toInt()!=1)return 14;
    const int id2=b.request(QStringLiteral("question"),{},cb);
    b.shutdown();
    binary.shutdown();
    b.complete(id2,{{QStringLiteral("accepted"),true}});
    if(engine.globalObject().property(QStringLiteral("counter")).toInt()!=1)return 15;
    qInfo()<<"PASS: shared file identity across Panels, legacy preserved, CSV, JS JSON, binary and dialog lifecycle";
    return 0;
}
