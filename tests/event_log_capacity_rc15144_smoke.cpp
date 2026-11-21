// RC1.5.14.4: 50,000-entry rolling Event Log regression.
// Must run offscreen in a real Qt6 build; requires no instruments or SDK.
#include "widgets/EventLogWidget.h"
#include <QApplication>
#include <QAbstractItemModel>
#include <QElapsedTimer>
#include <QDebug>
#include <QTableView>
#include <QVariantList>
#include <QVariantMap>
#include <QWidget>
#include <QScrollBar>
#include <QAbstractItemView>

#define CHECK(x) do {if(!(x)){qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1;}}while(false)

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QWidget root;
    root.resize(1100,650);
    EventLogWidget log(&root);
    log.setGeometry(10,10,1050,600);
    log.configure("Rolling capacity benchmark",50000,true,true,true);
    root.show();log.show();app.processEvents();
    auto* table=log.findChild<QTableView*>("eventLogTable");
    CHECK(table && table->model());
    auto* model=table->model();
    int resetCount=0,removeNotifications=0,insertNotifications=0;
    QObject::connect(model,&QAbstractItemModel::modelReset,&root,[&]{++resetCount;});
    QObject::connect(model,&QAbstractItemModel::rowsRemoved,&root,[&]{++removeNotifications;});
    QObject::connect(model,&QAbstractItemModel::rowsInserted,&root,[&]{++insertNotifications;});
    auto append=[&](int first,int last)->bool {
        QVariantList data;
        for(int i=first;i<last;++i) data.push_back(QVariantMap{
            {"message",QStringLiteral("E%1").arg(i)},
            {"source","Timer"},{"level",(i%5==0)?"PASS":"INFO"}});
        return log.applyEvents(QVariantMap{{"append",data}});
    };
    for(int first=0;first<50000;first+=250)CHECK(append(first,first+250));
    CHECK(log.entryCount()==50000);
    CHECK(model->rowCount()==50000);
    CHECK(model->index(0,3).data().toString()==QStringLiteral("E0"));
    CHECK(model->index(49999,3).data().toString()==QStringLiteral("E49999"));
    CHECK(resetCount==0);
    table->scrollTo(model->index(35000,0),QAbstractItemView::PositionAtTop);
    app.processEvents();
    const int startRow=table->rowAt(1);
    const QString anchor=startRow>=0?model->index(startRow,3).data().toString():QString();
    QElapsedTimer clock;clock.start();
    for(int first=50000;first<75000;first+=250)CHECK(append(first,first+250));
    const qint64 rollingMs=clock.elapsed();
    CHECK(log.entryCount()==50000);
    CHECK(model->rowCount()==50000);
    CHECK(model->index(0,3).data().toString()==QStringLiteral("E25000"));
    CHECK(model->index(49999,3).data().toString()==QStringLiteral("E74999"));
    CHECK(resetCount==0); // No full resets in rolling hot path.
    CHECK(removeNotifications==100 && insertNotifications==300);
    // The event at the top of the viewport must remain anchored while old
    // records above it expire, unless the user itself scrolled during the run.
    const int currentTop=table->rowAt(1);
    if(startRow>=0 && currentTop>=0 && !anchor.isEmpty())
        CHECK(model->index(currentTop,3).data().toString()==anchor);
    log.setFilter("PASS");
    CHECK(resetCount==1); // Changing a filter is allowed one full rebuild.
    CHECK(model->rowCount()==10000);
    CHECK(model->index(0,3).data().toString()==QStringLiteral("E25000"));
    for(int first=75000;first<85000;first+=250)CHECK(append(first,first+250));
    CHECK(resetCount==1); // Filtered rolling appends stay incremental too.
    CHECK(model->rowCount()==10000);
    CHECK(model->index(0,3).data().toString()==QStringLiteral("E35000"));
    CHECK(model->index(9999,3).data().toString()==QStringLiteral("E84995"));
    log.hide();
    for(int first=85000;first<87500;first+=250)CHECK(append(first,first+250));
    CHECK(log.entryCount()==50000);
    log.show();app.processEvents();
    CHECK(model->rowCount()==10000);
    CHECK(model->index(0,3).data().toString()==QStringLiteral("E37500"));
    log.setFilter("All");
    CHECK(model->rowCount()==50000);
    CHECK(model->index(0,3).data().toString()==QStringLiteral("E37500"));
    CHECK(log.csvText().contains(QStringLiteral("E87499")));
    CHECK(!log.csvText().contains(QStringLiteral("\"E0\"")));
    qInfo()<<"PASS RC1.5.14.4 rolling 50k entries, 37.5k expired, filter, hidden page, scroll anchor; rolling append elapsed"<<rollingMs<<"ms";
    return 0;
}
