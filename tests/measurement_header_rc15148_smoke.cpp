// RC1.5.14.8 native smoke: header/title/toolbar and 100k virtual table remain independent.
// Compile/run with Qt 6.9.2 on Windows; hardware-free.
#include "widgets/MeasurementTableWidget.h"
#include <QApplication>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QFile>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTableView>
#include <QVariantList>
#include <QVariantMap>
#include <QDebug>
#define CHECK(x) do{if(!(x)){qCritical()<<"RC15148 FAIL"<<#x<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QWidget root;root.resize(840,620);root.show();
    MeasurementTableWidget table(&root);
    table.setGeometry(0,0,830,580);
    table.configure(3,100,QStringLiteral("Measurement|Actual|Limits|Result"),
                    QStringLiteral("Production Measurements"),true,true,true,true,true,true,true);
    table.show();app.processEvents();
    CHECK(table.tableTitle()==QStringLiteral("Production Measurements"));
    CHECK(table.tableView()!=nullptr && table.tableView()->model()==table.model());
    CHECK(table.findChild<QPushButton*>(QStringLiteral("measurementTableExport")));
    CHECK(table.findChild<QPushButton*>(QStringLiteral("measurementTableClear")));
    auto* source=table.model();
    int resetCount=0;
    QObject::connect(source,&QAbstractItemModel::modelReset,&table,[&]{++resetCount;});
    const int limit=100000;
    for(int i=0;i<limit;i+=1000) {
        QVariantList batch;batch.reserve(1000);
        for(int j=0;j<1000;++j) {
            const int n=i+j;
            batch.append(QVariantMap{{"name",QStringLiteral("Sample %1").arg(n)},
                {"value",double(n%4)},{"low",0.0},{"high",2.0},{"unit","V"}});
        }
        CHECK(table.setMeasurements(QVariantMap{{"append",batch}}));
    }
    CHECK(table.rowCount()==limit && resetCount==0);
    CHECK(table.model()==source);
    CHECK(table.model()->index(99999,0).data().toString()==QStringLiteral("Sample 99999"));
    CHECK(table.horizontalHeader()->sectionResizeMode(1)==QHeaderView::Interactive);
    table.verticalScrollBar()->setValue(0);app.processEvents();
    CHECK(table.verticalScrollBar()->value()==0);
    CHECK(!table.setMeasurements(QVariantMap{{"append",QVariantMap{
        {"name","Beyond capacity"},{"value",1.0},{"low",0.0},{"high",2.0}}}}));
    CHECK(table.rowCount()==limit && resetCount==0);
    // Filtering is an operator-only presentation operation, not a data mutation.
    table.setResultFilter(QStringLiteral("FAIL"));
    app.processEvents();
    CHECK(table.tableView()->model()->rowCount()==25000);
    CHECK(table.model()==source && table.rowCount()==limit);
    CHECK(table.countText().contains(QStringLiteral("25000")));
    table.setResultFilter(QStringLiteral("All"));
    app.processEvents();
    CHECK(table.tableView()->model()==source);
    CHECK(table.tableView()->model()->rowCount()==limit);
    QTemporaryDir dir;CHECK(dir.isValid());
    const QString exportPath=dir.filePath(QStringLiteral("measurements.csv"));
    QString err;
    CHECK(table.exportCsv(exportPath,&err));
    QFile file(exportPath);CHECK(file.open(QIODevice::ReadOnly));
    CHECK(file.readLine().contains("Measurement"));
    CHECK(file.readLine().contains("Sample 0"));
    file.close();
    // Explicit clear resets only widget history and leaves underlying API intact.
    CHECK(table.setMeasurements(QVariantMap{{"reset",true}}));
    CHECK(table.rowCount()==0);
    qInfo()<<"PASS RC1.5.14.8 Measurement header, 100k append, filter, CSV, no hot-path resets";
    return 0;
}
