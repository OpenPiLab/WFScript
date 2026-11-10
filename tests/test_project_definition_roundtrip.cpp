// v5.6.64: testDefinition, testLibrary and UUT field schema save/reopen.
#include "PanelProject.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main(int argc,char** argv) {
    QCoreApplication application(argc,argv);
    PanelProject project=PanelProject::newProject(QStringLiteral("OperatorPanel.js"),
        QStringLiteral("ui.on('run','clicked',function(){TestRunner.run();});"));
    project.libraries.append(QJsonObject{
        {QStringLiteral("name"),QStringLiteral("TestSteps.mjs")},
        {QStringLiteral("source"),QStringLiteral("export function check() {return 3.301;}")}});
    project.testLibrary=QStringLiteral("TestSteps.mjs");
    project.testDefinition=QJsonObject{
        {QStringLiteral("schemaVersion"),1},
        {QStringLiteral("id"),QStringLiteral("voltage")},
        {QStringLiteral("title"),QStringLiteral("Voltage")},
        {QStringLiteral("config"),QJsonObject{
            {QStringLiteral("totalTimeoutMs"),15000},
            {QStringLiteral("stopOnFailure"),true}}},
        {QStringLiteral("steps"),QJsonArray{QJsonObject{
            {QStringLiteral("id"),QStringLiteral("check")},
            {QStringLiteral("name"),QStringLiteral("Voltage")},
            {QStringLiteral("mode"),QStringLiteral("once")},
            {QStringLiteral("type"),QStringLiteral("numeric")},
            {QStringLiteral("handler"),QStringLiteral("check")},
            {QStringLiteral("limits"),QJsonObject{
                {QStringLiteral("low"),3.25},{QStringLiteral("high"),3.35}}}}}}};
    project.runInformation=QJsonObject{{QStringLiteral("fields"),QJsonArray{
        QJsonObject{{QStringLiteral("id"),QStringLiteral("uutId")},
                    {QStringLiteral("label"),QStringLiteral("Serial Number")},
                    {QStringLiteral("type"),QStringLiteral("text")},
                    {QStringLiteral("required"),true}},
        QJsonObject{{QStringLiteral("id"),QStringLiteral("operatorId")},
                    {QStringLiteral("label"),QStringLiteral("Operator")},
                    {QStringLiteral("type"),QStringLiteral("text")}}}}};
    QString error;
    assert(project.validate(&error));
    QTemporaryDir temp;
    assert(temp.isValid());
    const QString file=temp.filePath(QStringLiteral("Voltage.wfpanel"));
    assert(project.saveFile(file,&error));
    PanelProject loaded;
    assert(PanelProject::loadFile(file,&loaded,&error));
    assert(loaded.testLibrary==QStringLiteral("TestSteps.mjs"));
    assert(loaded.testDefinition==project.testDefinition);
    assert(loaded.runInformation==project.runInformation);
    assert(loaded.testDefinition.value(QStringLiteral("config")).toObject()
        .value(QStringLiteral("totalTimeoutMs")).toInt()==15000);
    assert(loaded.libraries==project.libraries);
    assert(loaded.script==project.script);
    loaded.testLibrary=QStringLiteral("Missing.mjs");
    assert(!loaded.validate(&error));
    return 0;
}
