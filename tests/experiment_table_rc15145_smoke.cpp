// RC1.5.14.5: Qt-native virtual Experiment DataTable/JS worker parity smoke.
#include "PanelUiBridge.h"
#include "widgets/ExperimentTableModel.h"
#include <QApplication>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QVariantMap>
#define CHECK(x) do {if(!(x)){qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QJSEngine js;
    QJsonObject control{{"id","table"},{"type","experiment_table"},
        {"properties",QJsonObject{{"columns","Seq|Time|Voltage"},{"maxRows",50000}}}};
    PanelUiBridge ui(&js,QVariantMap{{"table",QVariantList{}}},{},{},QJsonArray{control});
    ExperimentTableModel model;
    model.configure({"Seq","Time","Voltage"},50000,true);
    QObject::connect(&ui,&PanelUiBridge::updateRequested,&app,
        [&](const QString& id,const QVariant& value){
            if(id!="table")return;
            ExperimentTableData::Mutation m;
            if(!ExperimentTableData::decode(value,3,50000,&m) || !model.mutate(m))
                qFatal("Worker sent an invalid update to GUI model");
        },Qt::DirectConnection);
    js.globalObject().setProperty("ui",js.newQObject(&ui));
    CHECK(js.evaluate("ui.set('table', {append:[0,0,3.3]})").toBool());
    CHECK(ui.tableRowCount("table")==1 && model.rowCount()==1);
    CHECK(js.evaluate("ui.set('table',{title:'Live acquisition'})").toBool());
    CHECK(js.evaluate("ui.set('table',{status:'Recording'})").toBool());
    CHECK(js.evaluate("ui.set('table',{autoScroll:false})").toBool());
    CHECK(js.evaluate("ui.set('table',{copySelection:true})").toBool());
    CHECK(ui.tableRowCount("table")==1);
    CHECK(js.evaluate("ui.set('table', {setCell:{row:0,column:2,value:3.31}})").toBool());
    CHECK(model.data(model.index(0,2)).toString()=="3.31");
    CHECK(js.evaluate("ui.get('table')[0][2]").toString()=="3.31");
    CHECK(js.evaluate("ui.set('table',{setRow:{row:0,values:[0,0,3.32]}})").toBool());
    CHECK(!js.evaluate("ui.set('table',{append:['wrong',1]})").toBool());
    CHECK(ui.tableRowCount("table")==1);
    CHECK(js.evaluate("ui.set('table',{clear:true})").toBool());
    CHECK(model.rowCount()==0);
    // Batch longer than small cap: do not remove non-existent rows.
    ExperimentTableModel small;
    small.configure({"Seq","Time","Voltage"},10,false);
    ExperimentTableData::Mutation smallBatch;
    smallBatch.kind=ExperimentTableData::Kind::Append;
    for(int i=0;i<25;++i)smallBatch.rows.push_back({QString::number(i),"0","0"});
    CHECK(small.mutate(smallBatch));
    CHECK(small.rowCount()==10 && small.data(small.index(0,0)).toString()=="15");
    CHECK(js.evaluate("ui.set('table',[[1,2,3],[4,5,6]])").toBool());
    CHECK(model.rowCount()==2);
    CHECK(js.evaluate("ui.set('table',{clear:true})").toBool());
    const int resetAtStart=model.fullResetCount();
    QElapsedTimer timer;timer.start();
    // 65,000 rows in 130 batches -> 15,000 must be evicted.
    for(int batch=0;batch<130;++batch){
        const int first=batch*500;
        QString script=QStringLiteral("(function(){var rows=[];for(var i=%1;i<%2;i++)rows.push([i,i*10,3.3]);return ui.set('table',{appendRows:rows});})()")
            .arg(first).arg(first+500);
        const QJSValue ok=js.evaluate(script);
        CHECK(!ok.isError() && ok.toBool());
    }
    CHECK(model.rowCount()==50000);
    CHECK(ui.tableRowCount("table")==50000);
    CHECK(model.fullResetCount()==resetAtStart); // No whole-model resets through rollover
    CHECK(model.data(model.index(0,0)).toString()=="15000");
    CHECK(model.data(model.index(49999,0)).toString()=="64999");
    CHECK(js.evaluate("ui.tableSlice('table',0,1)[0][0]").toInt()==15000);
    CHECK(js.evaluate("ui.tableSlice('table',49999,1)[0][0]").toInt()==64999);
    CHECK(js.evaluate("ui.tableRowCount('table')").toInt()==50000);
    CHECK(js.evaluate("Array.isArray(ui.get('table'))").toBool());
    CHECK(js.evaluate("ui.tableSlice('table',0,2049)===null").toBool());
    CHECK(model.setData(model.index(10,2),"3.4",Qt::EditRole));
    CHECK(model.data(model.index(10,2)).toString()=="3.4");
    CHECK(js.evaluate("ui.set('table',{appendRows:[[1,2]]})").toBool()==false);
    CHECK(ui.tableRowCount("table")==50000);
    qInfo()<<"PASS virtual Experiment table: 65k appended, 50k retained, no reset, worker parity"
           <<"elapsed ms"<<timer.elapsed();
    return 0;
}
