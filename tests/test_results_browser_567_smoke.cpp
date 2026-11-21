// v5.6.67 GUI browser smoke, launched with QT_QPA_PLATFORM=offscreen.
#include "TestResultsBrowserDialog.h"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableView>
#include <QAbstractItemModel>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QDebug>

#define CHECK(condition, description) do { if(!(condition)){qCritical() << "RESULTS BROWSER FAILED:" << description;return 1;} } while(false)
static bool put(const QString& path,const QString& id,const QString& state,const QString& uut,const QString& time) {
    const QJsonObject result{
        {QStringLiteral("schemaVersion"),1},
        {QStringLiteral("executionId"),id},
        {QStringLiteral("title"),QStringLiteral("Voltage test")},
        {QStringLiteral("state"),state},
        {QStringLiteral("finishedAtUtc"),time},
        {QStringLiteral("elapsedMs"),120},
        {QStringLiteral("runInfo"),QJsonObject{{QStringLiteral("uutId"),uut},{QStringLiteral("operatorId"),QStringLiteral("Alice")}}},
        {QStringLiteral("steps"),QJsonArray{QJsonObject{{QStringLiteral("id"),QStringLiteral("cleanup")},
                                    {QStringLiteral("name"),QStringLiteral("Cleanup")},
                                    {QStringLiteral("group"),QStringLiteral("cleanup")},
                                    {QStringLiteral("status"),QStringLiteral("done")}}}},
        {QStringLiteral("measurements"),QJsonArray{QJsonObject{{QStringLiteral("name"),QStringLiteral("Voltage")},
                                                {QStringLiteral("stepId"),QStringLiteral("measure")},
                                                {QStringLiteral("value"),3.3},{QStringLiteral("low"),3.2},
                                                {QStringLiteral("high"),3.4},{QStringLiteral("unit"),QStringLiteral("V")},
                                                {QStringLiteral("status"),QStringLiteral("PASS")}}}}
    };
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(result).toJson())>0;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QTemporaryDir tmp;
    CHECK(tmp.isValid(),"temp directory");
    CHECK(put(tmp.filePath(QStringLiteral("report_1.json")),QStringLiteral("11111111-1111-4111-8111-111111111111"),
              QStringLiteral("passed"),QStringLiteral("UNIT-001"),QStringLiteral("2026-11-08T13:01:00.000Z")),"pass fixture");
    CHECK(put(tmp.filePath(QStringLiteral("report_2.json")),QStringLiteral("22222222-2222-4222-8222-222222222222"),
              QStringLiteral("failed"),QStringLiteral("UNIT-002"),QStringLiteral("2026-11-08T13:02:00.000Z")),"fail fixture");
    TestResultsBrowserDialog browser(tmp.path());
    auto* table=browser.findChild<QTableView*>(QStringLiteral("historyTable"));
    auto* search=browser.findChild<QLineEdit*>(QStringLiteral("historySearch"));
    auto* verdict=browser.findChild<QComboBox*>(QStringLiteral("historyVerdict"));
    auto* steps=browser.findChild<QTableView*>(QStringLiteral("historySteps"));
    auto* measurements=browser.findChild<QTableView*>(QStringLiteral("historyMeasurements"));
    auto* runInfo=browser.findChild<QTableView*>(QStringLiteral("historyRunInfo"));
    auto* raw=browser.findChild<QPlainTextEdit*>(QStringLiteral("historyRawJson"));
    CHECK(table && search && verdict && steps && measurements && runInfo && raw,"find controls");
    CHECK(table->model()->rowCount()==2,"two archived runs shown initially");
    CHECK(steps->model()->rowCount()==1 && measurements->model()->rowCount()==1 && runInfo->model()->rowCount()==2,
          "selecting a run populates steps, measurements and UUT metadata");
    CHECK(raw->toPlainText().isEmpty(),"raw JSON is not formatted before opening tab");
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("historyDetailTabs"));
    CHECK(tabs,"detail tab view");
    tabs->setCurrentWidget(raw);
    CHECK(raw->toPlainText().contains(QStringLiteral("executionId")),"raw JSON displays selected report on demand");
    tabs->setCurrentIndex(0);
    search->setText(QStringLiteral("UNIT-002"));
    CHECK(table->model()->rowCount()==1 && table->model()->index(0,1).data().toString()==QStringLiteral("UNIT-002"),"search by UUT");
    search->setText(QString());
    const int failIndex=verdict->findData(QStringLiteral("failed"));
    CHECK(failIndex>=0,"failure filter available");
    verdict->setCurrentIndex(failIndex);
    CHECK(table->model()->rowCount()==1 && table->model()->index(0,2).data().toString()==QStringLiteral("FAIL"),"filter by failed runs");
    verdict->setCurrentIndex(0);
    search->setText(QStringLiteral("not-present"));
    CHECK(table->model()->rowCount()==0 && raw->toPlainText().isEmpty(),"empty search clears outdated detail");
    search->clear();
    CHECK(table->model()->rowCount()==2,"restore results");
    browser.setResultsDirectory(tmp.filePath(QStringLiteral("new-empty-directory")));
    CHECK(table->model()->rowCount()==0,"missing new archive folder is an empty valid history");
    qInfo()<<"PASS v5.6.67 native Results Browser smoke";
    return 0;
}
