// v5.6.67 native history contract: archive reading, sorting, filtering data,
// immutable source preservation, safe CSV export, malformed/foreign JSON.
#include "TestResultsRepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QDebug>

#define CHECK(condition, description) do { if(!(condition)){qCritical() << "RESULT HISTORY FAILED:" << description;return 1;} } while(false)

static QJsonObject sample(const QString& id,const QString& state,const QString& uut,
                           const QString& finished,const QString& title=QStringLiteral("Power supply functional test")) {
    return QJsonObject{
        {QStringLiteral("schemaVersion"),1},
        {QStringLiteral("executionId"),id},
        {QStringLiteral("title"),title},
        {QStringLiteral("projectName"),QStringLiteral("PowerTest.wfpanel")},
        {QStringLiteral("state"),state},
        {QStringLiteral("startedAtUtc"),QStringLiteral("2026-11-08T13:00:00.000Z")},
        {QStringLiteral("finishedAtUtc"),finished},
        {QStringLiteral("elapsedMs"),163},
        {QStringLiteral("runInfo"),QJsonObject{
            {QStringLiteral("uutId"),uut},
            {QStringLiteral("operatorId"),QStringLiteral("Operator_A")},
            {QStringLiteral("batchNumber"),QStringLiteral("LOT-42")}}},
        {QStringLiteral("summary"),QJsonObject{{QStringLiteral("completed"),3}}},
        {QStringLiteral("steps"),QJsonArray{
            QJsonObject{{QStringLiteral("id"),QStringLiteral("setup")},{QStringLiteral("group"),QStringLiteral("setup")},
                        {QStringLiteral("type"),QStringLiteral("action")},{QStringLiteral("name"),QStringLiteral("Setup")},
                        {QStringLiteral("status"),QStringLiteral("done")},{QStringLiteral("elapsedMs"),5}},
            QJsonObject{{QStringLiteral("id"),QStringLiteral("measure")},{QStringLiteral("group"),QStringLiteral("main")},
                        {QStringLiteral("type"),QStringLiteral("numeric")},{QStringLiteral("name"),QStringLiteral("=HYPERLINK(\"example\")")},
                        {QStringLiteral("status"),state==QStringLiteral("failed")?QStringLiteral("fail"):QStringLiteral("pass")},
                        {QStringLiteral("elapsedMs"),130}},
            QJsonObject{{QStringLiteral("id"),QStringLiteral("cleanup")},{QStringLiteral("group"),QStringLiteral("cleanup")},
                        {QStringLiteral("type"),QStringLiteral("action")},{QStringLiteral("name"),QStringLiteral("Cleanup")},
                        {QStringLiteral("status"),QStringLiteral("done")},{QStringLiteral("elapsedMs"),28}}}},
        {QStringLiteral("measurements"),QJsonArray{
            QJsonObject{{QStringLiteral("name"),QStringLiteral("V- measure")},{QStringLiteral("stepId"),QStringLiteral("measure")},
                        {QStringLiteral("value"),-3.301},{QStringLiteral("low"),-3.4},
                        {QStringLiteral("high"),-3.2},{QStringLiteral("unit"),QStringLiteral("V")},
                        {QStringLiteral("status"),state==QStringLiteral("failed")?QStringLiteral("FAIL"):QStringLiteral("PASS")}}}},
        {QStringLiteral("instrument"),QJsonObject{{QStringLiteral("mode"),QStringLiteral("simulation")}}}
    };
}
static bool writeJson(const QString& path,const QJsonObject& data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(data).toJson())>0;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir tmp;
    CHECK(tmp.isValid(),"temporary folder available");
    const QString uuid1=QStringLiteral("11111111-1111-4111-8111-111111111111");
    const QString uuid2=QStringLiteral("22222222-2222-4222-8222-222222222222");
    const QString uuid3=QStringLiteral("33333333-3333-4333-8333-333333333333");
    const QString older=tmp.filePath(QStringLiteral("archive_1.json"));
    const QString newest=tmp.filePath(QStringLiteral("archive_3.json"));
    const QJsonObject first=sample(uuid1,QStringLiteral("passed"),QStringLiteral("UNIT-001"),QStringLiteral("2026-11-08T13:01:00.000Z"));
    CHECK(writeJson(older,first),"first archive written");
    CHECK(writeJson(tmp.filePath(QStringLiteral("archive_2.json")),
              sample(uuid2,QStringLiteral("failed"),QStringLiteral("=COMMAND()"),QStringLiteral("2026-11-08T13:02:00.000Z"))),"failed archive written");
    CHECK(writeJson(newest,sample(uuid3,QStringLiteral("aborted"),QStringLiteral("UNIT-003"),QStringLiteral("2026-11-08T13:03:00.000Z"))),"aborted archive written");
    CHECK(writeJson(tmp.filePath(QStringLiteral("unrelated.json")),QJsonObject{{QStringLiteral("somethingElse"),true}}),"foreign JSON written");
    QFile invalid(tmp.filePath(QStringLiteral("corrupt.json")));
    CHECK(invalid.open(QIODevice::WriteOnly),"corrupt example open");
    invalid.write("{ totally invalid");invalid.close();

    QString error;
    auto all=TestResultsHistory::scan(tmp.path(),5000,&error);
    CHECK(error.isEmpty(),"history scan should complete without errors");
    CHECK(all.size()==3,"ignore unrelated and malformed JSON");
    CHECK(all.at(0).executionId==uuid3 && all.at(1).executionId==uuid2 && all.at(2).executionId==uuid1,
          "sort by completed timestamp descending irrespective of filenames");
    CHECK(all.at(1).searchText.contains(QStringLiteral("LOT-42")),"index includes all configured UUT fields");
    CHECK(all.at(0).state==QStringLiteral("aborted") && all.at(1).state==QStringLiteral("failed"),"preserve terminal states");
    const auto newestOnly=TestResultsHistory::scan(tmp.path(),1,&error);
    CHECK(newestOnly.size()==1,"enforce valid-report history limit");
    CHECK(newestOnly.first().executionId==uuid3,
          "invalid or unrelated JSON cannot displace newest completed report");
    CHECK(!error.isEmpty(),"warn when history is truncated");

    QJsonObject readBack;
    CHECK(TestResultsHistory::read(older,&readBack,&error),"read compatible archive");
    CHECK(readBack==first,"original report loads without modification");
    CHECK(!TestResultsHistory::read(tmp.filePath(QStringLiteral("unrelated.json")),&readBack,&error),"reject foreign schema");
    CHECK(!TestResultsHistory::read(tmp.filePath(QStringLiteral("corrupt.json")),&readBack,&error),"reject malformed JSON");
    CHECK(!TestResultsHistory::read(tmp.filePath(QStringLiteral("missing.json")),&readBack,&error),"missing report handled");

    const QString csv=tmp.filePath(QStringLiteral("export.csv"));
    CHECK(TestResultsHistory::exportRecord(first,csv,QStringLiteral("CSV"),&error),"CSV export");
    QFile csvFile(csv);
    CHECK(csvFile.open(QIODevice::ReadOnly),"read generated CSV");
    const QByteArray csvContent=csvFile.readAll();
    CHECK(csvContent.contains("\"STEP\"") && csvContent.contains("\"MEASUREMENT\""),"steps and measurements exported");
    CHECK(csvContent.contains("\"'=HYPERLINK"),"spreadsheet formula in step title neutralized");
    CHECK(csvContent.contains("\"-3.301\""),"negative numeric measurement preserved");
    CHECK(!csvContent.contains("\"=HYPERLINK"),"unsafe formula prefix absent");
    const QString json=tmp.filePath(QStringLiteral("copy.json"));
    CHECK(TestResultsHistory::exportRecord(first,json,QStringLiteral("json"),&error),"JSON copy exported");
    QJsonObject copy;
    CHECK(TestResultsHistory::read(json,&copy,&error) && copy==first,"JSON export is lossless");
    QJsonObject preserved;
    CHECK(TestResultsHistory::read(older,&preserved,&error) && preserved==first,"source archive not modified");
    CHECK(!TestResultsHistory::exportRecord(first,tmp.filePath(QStringLiteral("bad.txt")),QStringLiteral("csv"),&error),
          "mismatched file extension rejected");
    CHECK(!TestResultsHistory::exportRecord(QJsonObject{},csv,QStringLiteral("csv"),&error),"reject incomplete record");
    qInfo()<<"PASS v5.6.67 archived Results History smoke";
    return 0;
}
