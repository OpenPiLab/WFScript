#include "PanelProject.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

namespace {
int failures=0;
void check(bool condition, const QString& message) {
    if(!condition) { ++failures; qCritical().noquote()<<"FAIL:"<<message; }
}
QJsonObject sampleControl(const QString& id) {
    return QJsonObject{
        {"id",id},{"type","button"},{"text","Press"},{"x",24},{"y",32},
        {"width",120},{"height",40},{"value",0},{"min",0},{"max",100}};
}
QJsonObject legacyPanel() {
    return QJsonObject{{"format","WFScriptPanel"},{"version",1},
        {"canvasWidth",900},{"canvasHeight",550},
        {"controls",QJsonArray{sampleControl("go")}},
        {"script",QStringLiteral("var shared = 123;\n")},
        {"scriptName","Application.js"},
        {"description","My test project"},
        {"libraries",QJsonArray{QJsonObject{
             {"name","utils/Math.mjs"},
             {"source","export const add = (a,b) => a+b;"}}}},
        {"vendorNote",QStringLiteral("keep me")}};
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QString error;
    PanelProject project;
    check(PanelProject::fromJson(legacyPanel(),QStringLiteral("C:/Projects/App.wfpanel"),&project,&error),
          QStringLiteral("load legacy schema v1: ")+error);
    check(project.pages.size()==1 && project.mainPage().id==QStringLiteral("main") &&
          project.mainPage().name==QStringLiteral("Main") &&
          project.mainPage().size==QSize(900,550),QStringLiteral("migrate legacy controls to Main page"));
    check(project.mainPage().controls.size()==1 && project.script==QStringLiteral("var shared = 123;\n"),
          QStringLiteral("keep JavaScript and controls"));
    check(project.libraries.size()==1 && !project.lifecycle.initializeEnabled &&
          !project.lifecycle.shutdownEnabled,QStringLiteral("keep libraries and default lifecycle disabled"));
    QJsonObject saved;
    check(project.toJson(&saved,&error),QStringLiteral("write original project: ")+error);
    check(saved.value("version").toInt()==1 && saved.value("controls").toArray().size()==1 &&
          saved.value("vendorNote").toString()==QStringLiteral("keep me"),
          QStringLiteral("legacy schema plus unknown fields round trip"));
    check(!saved.contains("lifecycle"),QStringLiteral("default lifecycle omitted from legacy panels"));
    project.lifecycle.initializeEnabled=true;
    project.lifecycle.shutdownEnabled=true;
    check(project.toJson(&saved,&error),QStringLiteral("serialize lifecycle settings: ")+error);
    check(saved.value("lifecycle").toObject().value("initializeEnabled").toBool() &&
          saved.value("lifecycle").toObject().value("shutdownEnabled").toBool(),
          QStringLiteral("lifecycle flags stored in the project"));
    PanelProject reloaded;
    check(PanelProject::fromJson(saved,QStringLiteral("App.wfpanel"),&reloaded,&error) &&
          reloaded.lifecycle.initializeEnabled && reloaded.lifecycle.shutdownEnabled,
          QStringLiteral("lifecycle settings reload"));
    QTemporaryDir temporary;
    check(temporary.isValid(),QStringLiteral("temporary test dir"));
    const QString path=temporary.filePath(QStringLiteral("TestProject.wfpanel"));
    check(reloaded.saveFile(path,&error),QStringLiteral("atomic save to disk: ")+error);
    PanelProject reread;
    check(PanelProject::loadFile(path,&reread,&error) && reread.scriptName==QStringLiteral("Application.js") &&
          reread.libraries.size()==1 && reread.mainPage().controls.size()==1,
          QStringLiteral("reopen saved file: ")+error);
    // The future page model already owns widget collections and destroys them
    // on page deletion, but the v1 exporter refuses silent data loss.
    check(reread.addPage(QStringLiteral("settings"),QStringLiteral("Settings"),QSize(1024,600),&error),
          QStringLiteral("add second page: ")+error);
    auto* page=reread.findPage(QStringLiteral("settings"));
    check(page!=nullptr,QStringLiteral("find second page by stable ID"));
    if(page)page->controls=QJsonArray{sampleControl(QStringLiteral("configButton"))};
    check(reread.validate(&error),QStringLiteral("multiple-page model validates: ")+error);
    check(reread.toJson(&saved,&error) && saved.value("version").toInt()==2 &&
          saved.value("pages").toArray().size()==2,
          QStringLiteral("multi-page serializer writes schema v2"));
    PanelProject second;
    check(PanelProject::fromJson(saved,QStringLiteral("two-pages.wfpanel"),&second,&error) &&
          second.pages.size()==2 && second.findPage(QStringLiteral("settings")) &&
          second.findPage(QStringLiteral("settings"))->controls.size()==1,
          QStringLiteral("v2 round trip retains second Page widgets"));
    const QString duplicateError=QStringLiteral("Duplicate page must be rejected");
    check(!reread.addPage(QStringLiteral("settings"),QStringLiteral("Duplicate"),QSize(900,550)),duplicateError);
    check(reread.deletePage(QStringLiteral("settings"),&error) &&
          reread.findPage(QStringLiteral("settings"))==nullptr,
          QStringLiteral("delete page removes its controls"));
    check(!reread.deletePage(QStringLiteral("main")),QStringLiteral("cannot delete last page"));
    check(reread.toJson(&saved,&error),QStringLiteral("remaining single page saves"));
    PanelProject invalid;
    QJsonObject bad=legacyPanel();
    bad.insert(QStringLiteral("libraries"),QJsonArray{QJsonObject{{"name","../unsafe.js"},{"source",""}}});
    check(!PanelProject::fromJson(bad,QStringLiteral("bad.wfpanel"),&invalid,&error),
          QStringLiteral("reject unsafe embedded-library traversal"));
    bad=legacyPanel();
    bad.insert(QStringLiteral("controls"),QJsonArray{sampleControl("same"),sampleControl("same")});
    check(!PanelProject::fromJson(bad,QStringLiteral("bad.wfpanel"),&invalid,&error),
          QStringLiteral("reject duplicate widget IDs"));
    bad=legacyPanel();
    bad.insert(QStringLiteral("lifecycle"),QJsonObject{{"initializeEnabled","yes"}});
    check(!PanelProject::fromJson(bad,QStringLiteral("bad.wfpanel"),&invalid,&error),
          QStringLiteral("reject nonboolean lifecycle settings"));
    if(failures==0)qInfo()<<"PASS: PanelProject legacy migration, round trip, pages and lifecycle settings";
    return failures?1:0;
}
