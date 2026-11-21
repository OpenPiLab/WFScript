// RC1.5.14.3: GUI model/view smoke for 10k sequence rows + 10k measurements.
// No TCP, DWF SDK, JavaScript or physical instruments required.
#include "widgets/TestSequenceWidget.h"
#include "widgets/MeasurementTableWidget.h"
#include "widgets/EventLogWidget.h"
#include "TestSequenceCapacity.h"
#include <QApplication>
#include <QTableView>
#include <QAbstractItemModel>
#include <QScrollBar>
#include <QVariantList>
#include <QVariantMap>
#include <QDebug>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QWidget root;
    root.resize(1024,640);root.show();
    TestSequenceWidget sequence(&root);
    sequence.setGeometry(0,0,510,580);
    sequence.configure("Long execution",true,true,true,true,true,100);
    QVariantList definitions;
    definitions.reserve(10000);
    for(int i=0;i<10000;++i)
        definitions.push_back(QVariantMap{{"id",QStringLiteral("point%1").arg(i)},
            {"name",QStringLiteral("Frequency point %1").arg(i)},
            {"status",QStringLiteral("pending")}});
    assert(sequence.applySteps(QVariantMap{{"virtualSteps",definitions}}));
    sequence.show();app.processEvents();
    auto* table=sequence.findChild<QTableView*>("virtualSequenceTable");
    assert(table && table->model()->rowCount()==10000);
    assert(table->model()->index(9999,1).data().toString()=="Frequency point 9999");
    assert(sequence.applySteps(QVariantMap{{"update",QVariantMap{
        {"id","point9999"},{"status","running"}}}}));
    app.processEvents();
    assert(table->model()->index(9999,3).data().toString()=="RUNNING");
    table->verticalScrollBar()->setValue(0);app.processEvents();
    assert(table->model()->index(0,1).data().toString()=="Frequency point 0");
    assert(sequence.applySteps(QVariantMap{{"update",QVariantMap{
        {"id","point9999"},{"status","pass"}}}}));
    assert(sequence.statusFor("point9999")=="pass");

    MeasurementTableWidget readings(&root);
    readings.setGeometry(515,0,500,580);
    readings.configure(3,100,"Measurement|Actual|Limits|Result");
    readings.show();app.processEvents();
    for(int chunk=0;chunk<100;++chunk) {
        QVariantList batch;
        for(int j=0;j<100;++j) {
            const int i=chunk*100+j;
            batch.push_back(QVariantMap{{"name",QStringLiteral("Frequency %1").arg(i)},
                {"value",double(i)},{"low",double(i)-.1},
                {"high",double(i)+.1},{"unit","Hz"}});
        }
        assert(readings.setMeasurements(QVariantMap{{"append",batch}}));
    }
    assert(readings.model()->rowCount()==10000);
    assert(readings.model()->index(0,0).data().toString()=="Frequency 0");
    assert(readings.model()->index(9999,0).data().toString()=="Frequency 9999");
    readings.verticalScrollBar()->setValue(0);app.processEvents();
    const int before=readings.verticalScrollBar()->value();
    assert(readings.setMeasurements(QVariantMap{{"append",QVariantMap{
        {"name","Last"},{"value",1.0},{"low",0.0},{"high",2.0}}}}));
    assert(readings.verticalScrollBar()->value()==before); // manual review stays put
    assert(readings.model()->rowCount()==10001);
    assert(readings.model()->index(10000,3).data().toString()=="PASS");
    readings.scrollToBottom();app.processEvents();
    assert(readings.setMeasurements(QVariantMap{{"append",QVariantMap{
        {"name","Follow"},{"value",2.0},{"low",1.0},{"high",3.0}}}}));
    assert(readings.model()->rowCount()==10002);
    assert(readings.verticalScrollBar()->value()>=readings.verticalScrollBar()->maximum()-2);
    // The Event Log also has a virtual filterable model and 100k retention.
    EventLogWidget log(&root);
    log.setGeometry(20,30,800,400);
    log.configure("Long test log",10000,true,true,true);
    log.show();app.processEvents();
    auto* logTable=log.findChild<QTableView*>("eventLogTable");
    assert(logTable);
    QVariantList logBatch;
    for(int i=0;i<5000;++i)
        logBatch.push_back(QVariantMap{{"level",i%5==0?"WARN":"INFO"},
            {"message",QStringLiteral("Event %1").arg(i)}});
    assert(log.applyEvents(QVariantMap{{"append",logBatch}}));
    assert(log.entryCount()==5000 && logTable->model()->rowCount()==5000);
    assert(logTable->model()->index(4999,3).data().toString()=="Event 4999");
    log.setFilter("WARN");
    assert(logTable->model()->rowCount()==1000);
    assert(logTable->model()->index(999,3).data().toString()=="Event 4995");
    log.setFilter("All");
    assert(logTable->model()->rowCount()==5000);
    qInfo()<<"PASS RC1.5.14.3: virtual 10k sequence and measurement rows, random scroll and tail-follow";
    return 0;
}
