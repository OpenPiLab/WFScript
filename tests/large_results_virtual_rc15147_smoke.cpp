// RC1.5.14.7: virtual live measurement table + archived report browser at scale.
// Offscreen Qt GUI smoke. Hardware-free, does not execute scripts or report code.
#include "TestResultsBrowserDialog.h"
#include "widgets/MeasurementTableWidget.h"
#include "TestSequenceCapacity.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QDebug>

#define CHECK(expr) do { if(!(expr)) {qCritical()<<"RC15147 FAIL:"<<#expr<<"line"<<__LINE__;return 1;} } while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QWidget root;root.resize(1100,700);root.show();
    MeasurementTableWidget live(&root);
    live.setGeometry(10,10,950,600);
    live.show();app.processEvents();
    int resets=0;
    auto* source=live.model();
    QObject::connect(source,&QAbstractItemModel::modelReset,&live,[&]{++resets;});
    for(int chunk=0;chunk<100;++chunk) {
        QVariantList batch;batch.reserve(1000);
        for(int j=0;j<1000;++j) {
            const int n=chunk*1000+j;
            batch.append(QVariantMap{{"name",QStringLiteral("Reading %1").arg(n)},
                       {"value",double(n%100)},{"low",0.0},{"high",100.0},{"unit","V"}});
        }
        CHECK(live.setMeasurements(QVariantMap{{"append",batch}}));
    }
    CHECK(live.rowCount()==TestSequenceCapacity::maxMeasurementsPerRun);
    CHECK(resets==0); // 100 streamed appends must not reset entire GUI model
    CHECK(live.model()->index(0,0).data().toString()==QStringLiteral("Reading 0"));
    CHECK(live.model()->index(99999,0).data().toString()==QStringLiteral("Reading 99999"));
    CHECK(live.horizontalHeader()->sectionResizeMode(1)==QHeaderView::Interactive);
    CHECK(live.horizontalHeader()->sectionResizeMode(2)==QHeaderView::Interactive);
    CHECK(live.horizontalHeader()->sectionResizeMode(3)==QHeaderView::Interactive);
    CHECK(!live.setMeasurements(QVariantMap{{"append",QVariantMap{
              {"name","Overflow"},{"value",1.0},{"low",0.0},{"high",2.0}}}}));
    CHECK(live.rowCount()==100000); // reject overflow atomically
    live.verticalScrollBar()->setValue(0);
    app.processEvents();
    CHECK(live.verticalScrollBar()->value()==0);

    // JSON record large enough to exercise 100k archived rows in QTableView.
    QTemporaryDir tmp;CHECK(tmp.isValid());
    QJsonArray measures;
    for(int n=0;n<100000;++n)
        measures.append(QJsonObject{
            {QStringLiteral("name"),QStringLiteral("Voltage %1").arg(n)},
            {QStringLiteral("stepId"),QStringLiteral("main")},
            {QStringLiteral("value"),double(n%100)},
            {QStringLiteral("low"),0.0},{QStringLiteral("high"),100.0},
            {QStringLiteral("unit"),QStringLiteral("V")},
            {QStringLiteral("status"),QStringLiteral("PASS")}});
    QJsonArray steps;
    for(int n=0;n<10000;++n)
        steps.append(QJsonObject{
            {QStringLiteral("group"),QStringLiteral("main")},
            {QStringLiteral("name"),QStringLiteral("Step %1").arg(n)},
            {QStringLiteral("status"),QStringLiteral("pass")}});
    const QJsonObject report{
        {QStringLiteral("schemaVersion"),1},
        {QStringLiteral("executionId"),QStringLiteral("11111111-1111-4111-8111-111111111111")},
        {QStringLiteral("title"),QStringLiteral("100k virtual measurements regression")},
        {QStringLiteral("projectName"),QStringLiteral("VirtualLab.wfpanel")},
        {QStringLiteral("state"),QStringLiteral("passed")},
        {QStringLiteral("finishedAtUtc"),QStringLiteral("2026-11-20T13:00:00.000Z")},
        {QStringLiteral("runInfo"),QJsonObject{{QStringLiteral("uutId"),QStringLiteral("LARGE-001")}}},
        {QStringLiteral("steps"),steps},
        {QStringLiteral("measurements"),measures}};
    const QString reportPath=tmp.filePath(QStringLiteral("100k_results.json"));
    QFile file(reportPath);
    CHECK(file.open(QIODevice::WriteOnly));
    CHECK(file.write(QJsonDocument(report).toJson(QJsonDocument::Indented))>0);
    file.close();
    CHECK(QFileInfo(reportPath).size()>16LL*1024*1024); // actual previous browser limit
    TestResultsBrowserDialog browser(tmp.path());
    browser.show();app.processEvents();
    auto* view=browser.findChild<QTableView*>(QStringLiteral("historyMeasurements"));
    auto* stepsView=browser.findChild<QTableView*>(QStringLiteral("historySteps"));
    auto* history=browser.findChild<QTableView*>(QStringLiteral("historyTable"));
    auto* json=browser.findChild<QPlainTextEdit*>(QStringLiteral("historyRawJson"));
    auto* tabs=browser.findChild<QTabWidget*>(QStringLiteral("historyDetailTabs"));
    CHECK(view && stepsView && history && json && tabs);
    CHECK(history->model()->rowCount()==1);
    CHECK(view->model()->rowCount()==100000);
    CHECK(stepsView->model()->rowCount()==10000);
    CHECK(view->model()->index(99999,0).data().toString()==QStringLiteral("Voltage 99999"));
    CHECK(stepsView->model()->index(9999,1).data().toString()==QStringLiteral("Step 9999"));
    CHECK(json->toPlainText().isEmpty()); // no eager full-JSON formatting
    view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
    app.processEvents();
    CHECK(view->model()->index(99999,6).data().toString()==QStringLiteral("PASS"));
    tabs->setCurrentWidget(json);app.processEvents();
    CHECK(json->toPlainText().contains(QStringLiteral("Voltage 99999")));
    qInfo()<<"PASS RC1.5.14.7: 100k live measurements; 100k archived measurements; 10k steps; lazy JSON";
    return 0;
}
