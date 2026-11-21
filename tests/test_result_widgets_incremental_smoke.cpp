// RC1.5.6: offscreen incremental visual update/regression tests.
// No external instruments, dynamic SCPI drivers, or WaveForms SDK required.
#include "widgets/TestSequenceWidget.h"
#include "widgets/EventLogWidget.h"
#include "widgets/MeasurementTableWidget.h"
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QTableView>
#include <QAbstractItemModel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariantMap>
#include <QWidget>
#include <QVariantList>
#include <QDebug>

#define CHECK(x) do { if(!(x)) { qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1; } } while(false)

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QWidget root;
    root.resize(1500,900);
    root.show();

    // A status/elapsed/message update must never replace any of the 62 Qt rows.
    TestSequenceWidget sequence(&root);
    sequence.setGeometry(0,0,490,460);
    sequence.configure("Rigol Test",true,true,true,true,false,100);
    QVariantList steps;
    for(int i=0;i<62;++i)
        steps.push_back(QVariantMap{{"id",QStringLiteral("s%1").arg(i)},
                                    {"name",QStringLiteral("Step %1").arg(i)}});
    CHECK(sequence.applySteps(steps));
    sequence.show();
    app.processEvents();
    QWidget* content=sequence.findChild<QWidget*>("sequenceRows");
    CHECK(content);
    QWidget* stableRow=sequence.findChild<QWidget*>("sequenceStep_s0");
    QWidget* otherRow=sequence.findChild<QWidget*>("sequenceStep_s61");
    QLabel* stableStatus=sequence.findChild<QLabel*>("sequenceStatus_s0");
    QLabel* otherStatus=sequence.findChild<QLabel*>("sequenceStatus_s61");
    CHECK(stableRow && otherRow && stableStatus && otherStatus);
    const int originalFrames=sequence.findChildren<QFrame*>().size();
    for(int i=0;i<62;++i) {
        const QString id=QStringLiteral("s%1").arg(i);
        CHECK(sequence.applySteps(QVariantMap{{"update",QVariantMap{
            {"id",id},{"status","running"},{"elapsedMs",80},{"message","working"}}}}));
        CHECK(sequence.applySteps(QVariantMap{{"update",QVariantMap{
            {"id",id},{"status","pass"},{"elapsedMs",120},{"message","verified"}}}}));
        CHECK(sequence.findChild<QWidget*>("sequenceRows")==content);
        CHECK(sequence.findChild<QWidget*>("sequenceStep_s0")==stableRow);
        CHECK(sequence.findChild<QWidget*>("sequenceStep_s61")==otherRow);
        CHECK(sequence.findChild<QLabel*>("sequenceStatus_s0")==stableStatus);
    }
    CHECK(sequence.completedSteps()==62);
    CHECK(sequence.percentComplete()==100);
    CHECK(stableStatus->text()==QStringLiteral("PASS"));
    CHECK(otherStatus->text()==QStringLiteral("PASS"));
    CHECK(sequence.findChildren<QFrame*>().size()==originalFrames);
    CHECK(sequence.applySteps(QVariantMap{{"reset",true}}));
    CHECK(sequence.completedSteps()==0);
    CHECK(sequence.findChild<QWidget*>("sequenceRows")==content);
    CHECK(stableStatus->text()==QStringLiteral("PENDING"));
    CHECK(!sequence.applySteps(QVariantMap{{"update",QVariantMap{{"id","s0"},{"status","invalid"}}}}));
    CHECK(sequence.statusFor("s0")==QStringLiteral("pending"));
    // Hidden Page: coalesce updates without repainting; flush on re-show.
    sequence.hide();
    CHECK(sequence.applySteps(QVariantMap{{"update",QVariantMap{{"id","s0"},{"status","fail"}}}}));
    CHECK(sequence.statusFor("s0")==QStringLiteral("fail"));
    CHECK(stableStatus->text()==QStringLiteral("PENDING"));
    sequence.show();
    app.processEvents();
    CHECK(stableStatus->text()==QStringLiteral("FAIL"));
    CHECK(sequence.findChild<QWidget*>("sequenceRows")==content);

    // RC1.5.14.3: retain every native step, virtualize visible Qt rows.
    // The legacy {window} payload is still accepted by existing Panels.
    QVariantList lateWindow;
    for(int i=100;i<112;++i)
        lateWindow.push_back(QVariantMap{{"id",QStringLiteral("s%1").arg(i)},
                                      {"name",QStringLiteral("Step %1").arg(i)}});
    CHECK(sequence.applySteps(QVariantMap{{"window",QVariantMap{
        {"start",100},{"total",112},{"steps",lateWindow}}}}));
    CHECK(sequence.stepCount()==12);
    CHECK(sequence.findChild<QWidget*>("sequenceStep_s100"));
    CHECK(sequence.applySteps(QVariantMap{{"progress",QVariantMap{
        {"total",112},{"completed",111},{"state","running"}}}}));
    CHECK(sequence.percentComplete()==99);
    CHECK(sequence.overallState()==QStringLiteral("RUNNING"));
    CHECK(sequence.applySteps(QVariantMap{{"update",QVariantMap{
        {"id","s111"},{"status","pass"}}}}));
    CHECK(sequence.applySteps(QVariantMap{{"progress",QVariantMap{
        {"total",112},{"completed",112},{"state","passed"}}}}));
    CHECK(sequence.percentComplete()==100);
    CHECK(sequence.overallState()==QStringLiteral("COMPLETE"));

    QVariantList allSteps;
    for(int i=0;i<1120;++i)
        allSteps.append(QVariantMap{{"id",QStringLiteral("v%1").arg(i)},
                                    {"name",QStringLiteral("Virtual step %1").arg(i)}});
    CHECK(sequence.applySteps(QVariantMap{{"virtualSteps",allSteps}}));
    CHECK(sequence.stepCount()==1120);
    auto* virtualView=sequence.findChild<QTableView*>("virtualSequenceTable");
    CHECK(virtualView && virtualView->model()->rowCount()==1120);
    CHECK(virtualView->model()->index(0,1).data().toString()==QStringLiteral("Virtual step 0"));
    CHECK(virtualView->model()->index(1119,1).data().toString()==QStringLiteral("Virtual step 1119"));
    CHECK(sequence.applySteps(QVariantMap{{"update",QVariantMap{
        {"id","v1119"},{"status","pass"},{"elapsedMs",300}}}}));
    CHECK(virtualView->model()->index(1119,3).data().toString()==QStringLiteral("PASS"));
    CHECK(sequence.completedSteps()==1);

    // Model-backed table: no per-cell Qt widgets; changes update the same model.
    MeasurementTableWidget measurements(&root);
    measurements.setGeometry(500,0,480,450);
    measurements.configure(3,100,QStringLiteral("Measurement|Actual|Limits|Result"));
    measurements.show();
    QVariantList measured;
    for(int i=0;i<50;++i)
        measured.push_back(QVariantMap{{"name",QStringLiteral("Voltage %1").arg(i)},
                                       {"value",double(i)},{"low",double(i)-.1},
                                       {"high",double(i)+.1},{"unit","V"}});
    CHECK(measurements.setMeasurements(measured));
    auto* stableModel=measurements.model();
    CHECK(stableModel && stableModel->rowCount()==50);
    for(int i=0;i<50;++i) {
        auto row=measured.at(i).toMap();
        row["value"]=double(i)+0.04;
        measured[i]=row;
        CHECK(measurements.setMeasurements(measured));
        CHECK(measurements.model()==stableModel);
        CHECK(stableModel->rowCount()==50);
    }
    CHECK(stableModel->index(49,3).data().toString()==QStringLiteral("PASS"));
    const QString previous=stableModel->index(0,1).data().toString();
    QVariantList bad=measured;
    bad[3]=QVariantMap{{"name","bad"},{"value",1.0},{"low",9.0},{"high",1.0}};
    CHECK(!measurements.setMeasurements(bad));
    CHECK(stableModel->index(0,1).data().toString()==previous);
    auto failed=measured.at(1).toMap();failed["value"]=100.0;measured[1]=failed;
    CHECK(measurements.setMeasurements(measured));
    CHECK(stableModel->index(1,3).data().toString()==QStringLiteral("FAIL"));
    CHECK(measurements.setMeasurements(QVariantMap{{"reset",true}}));
    CHECK(measurements.rowCount()==0);

    // 1200 streamed measurements remain addressable; no 100-row truncation.
    QVariantList batch;
    for(int i=0;i<1200;++i)
        batch.append(QVariantMap{{"name",QStringLiteral("Voltage %1").arg(i)},
                                 {"value",double(i)},{"low",double(i)-.1},
                                 {"high",double(i)+.1},{"unit","V"}});
    CHECK(measurements.setMeasurements(QVariantMap{{"append",batch}}));
    CHECK(measurements.rowCount()==1200);
    CHECK(measurements.model()==stableModel);
    CHECK(stableModel->index(0,0).data().toString()==QStringLiteral("Voltage 0"));
    CHECK(stableModel->index(1199,0).data().toString()==QStringLiteral("Voltage 1199"));
    CHECK(measurements.setMeasurements(QVariantMap{{"reset",true}}));

    // Virtual Event Log: stable model, filtering, capacity and hidden page updates.
    EventLogWidget log(&root);
    log.setGeometry(990,0,500,460);
    log.configure("Rigol Log",10,true,true,true);
    log.show();
    auto* logTable=log.findChild<QTableView*>("eventLogTable");
    CHECK(logTable);
    auto* logModel=logTable->model();
    CHECK(log.applyEvents(QVariantMap{{"append",QVariantMap{{"message","first"},{"level","PASS"}}}}));
    CHECK(logModel->rowCount()==1);
    CHECK(logModel->index(0,3).data().toString()=="first");
    for(int i=0;i<8;++i)
        CHECK(log.applyEvents(QVariantMap{{"append",QVariantMap{{"message",QStringLiteral("entry %1").arg(i)},
                                                      {"level","INFO"}}}}));
    CHECK(logModel==logTable->model() && logModel->rowCount()==9);
    CHECK(log.applyEvents(QVariantMap{{"append",QVariantMap{{"message","tenth"},{"level","WARN"}}}}));
    CHECK(logModel->rowCount()==10);
    CHECK(log.applyEvents(QVariantMap{{"append",QVariantMap{{"message","eleventh"},{"level","WARN"}}}}));
    CHECK(log.entryCount()==10 && logModel->rowCount()==10);
    CHECK(logModel->index(0,3).data().toString()=="entry 0");
    log.setFilter("WARN");
    CHECK(logModel->rowCount()==2);
    CHECK(log.applyEvents(QVariantMap{{"append",QVariantMap{{"message","no show"},{"level","INFO"}}}}));
    CHECK(logModel->rowCount()==2);
    CHECK(log.applyEvents(QVariantMap{{"append",QVariantMap{{"message","visible"},{"level","WARN"}}}}));
    CHECK(logModel->rowCount()==3 && logModel->index(2,3).data().toString()=="visible");
    log.hide();
    CHECK(log.applyEvents(QVariantMap{{"append",QVariantMap{{"message","hidden"},{"level","WARN"}}}}));
    CHECK(logModel->rowCount()==3);
    log.show();app.processEvents();
    CHECK(logModel->rowCount()==4 && logModel->index(3,3).data().toString()=="hidden");
    CHECK(log.csvText().contains(QStringLiteral("hidden")));
    CHECK(!log.applyEvents(QVariantMap{{"append",QVariantMap{{"level","BAD"},{"message","no"}}}}));
    CHECK(log.entryCount()==10);
    log.setFilter("All");
    CHECK(logModel->rowCount()==10);
    qInfo()<<"PASS RC1.5.6: in-place sequence rows, stable measurement items, incremental logs, hidden-Page catch-up";
    return 0;
}
