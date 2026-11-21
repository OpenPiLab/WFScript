// v5.6.70 native offscreen regression: equation syntax/preview/render, table,
// editable cell events, CSV export and schema-compatible Panel save/reopen.
#include "PanelCanvas.h"
#include "widgets/ExperimentWidgets.h"
#include "widgets/WidgetPropertyInspector.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QTemporaryDir>
#include <QFile>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QJsonArray>
#define CHECK(x) do {if(!(x)){qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const auto* math=WidgetRegistry::instance().factory("equation");
    const auto* table=WidgetRegistry::instance().factory("experiment_table");
    CHECK(math && table);
    QString err;
    CHECK(EquationMath::validSource(QStringLiteral("f_c=\\frac{1}{2\\pi RC}"),&err));
    CHECK(!EquationMath::validSource(QStringLiteral("\\frac{1}{2\\pi RC"),&err));
    CHECK(!EquationMath::render(QStringLiteral("H(j\\omega)=\\frac{1}{1+j\\omega RC}"),25,Qt::white).isNull());
    // Commands from the real RC least-squares Page must paint Greek letters,
    // mathematical arrows and percent signs, not a literal \\Sigma token.
    const QColor white(Qt::white);
    CHECK(EquationMath::render(QStringLiteral("\\Sigma"),25,white)==
          EquationMath::render(QString::fromUtf8("Σ"),25,white));
    CHECK(EquationMath::render(QStringLiteral("\\varepsilon"),25,white)==
          EquationMath::render(QString::fromUtf8("ϵ"),25,white));
    CHECK(EquationMath::render(QStringLiteral("\\Longleftrightarrow"),25,white)==
          EquationMath::render(QString::fromUtf8("⟺"),25,white));
    CHECK(EquationMath::render(QStringLiteral("\\%"),25,white)==
          EquationMath::render(QStringLiteral("%"),25,white));
    auto eq=math->defaultDefinition("eq1");
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(eq),&eq,&err));
    WidgetPropertyInspector inspector;
    inspector.setControl(eq,true);
    CHECK(inspector.editor("source"));
    CHECK(inspector.findChild<QLabel*>("equationLivePreview"));
    CHECK(!inspector.findChild<QLabel*>("equationLivePreview")->pixmap().isNull());
    CHECK(inspector.findChild<QPushButton*>("equationInsert_Fraction"));
    auto old=eq;old.properties.insert("source","f_c={oops");
    PanelControl decoded;
    CHECK(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(old),&decoded,&err));
    auto sample=table->defaultDefinition("data1");
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(sample),&decoded,&err));
    sample.properties.insert("editable",true);
    auto* tw=dynamic_cast<ExperimentDataTableWidget*>(table->create(sample,nullptr,{}));
    CHECK(tw);
    CHECK(tw->columnCount()==4 && tw->rowCount()==5);
    CHECK(tw->findChild<QWidget*>("experimentTableToolbar"));
    CHECK(tw->findChild<QPushButton*>("experimentTableImport"));
    CHECK(tw->findChild<QPushButton*>("experimentTableExport"));
    CHECK(tw->findChild<QPushButton*>("experimentTableCopy"));
    CHECK(tw->findChild<QPushButton*>("experimentTableClear"));
    CHECK(tw->findChild<QCheckBox*>("experimentTableFollow"));
    CHECK(tw->tableView()->editTriggers().testFlag(QAbstractItemView::AnyKeyPressed));
    CHECK(tw->applyValue(QVariantMap{{"title",QStringLiteral("Live voltage")}}));
    CHECK(tw->findChild<QLabel*>("experimentTableTitle")->text()==QStringLiteral("Live voltage"));
    CHECK(tw->applyValue(QVariantMap{{"status",QStringLiteral("Streaming")}}));
    CHECK(tw->applyValue(QVariantMap{{"autoScroll",false}}));
    CHECK(!tw->findChild<QCheckBox*>("experimentTableFollow")->isChecked());
    // Regression: QList<QVariant>::append(QVariantList) used to FLATTEN rows.
    // Verify the source/property data survive as 5 rows of 4 columns.
    CHECK(tw->model()->data(tw->model()->index(0,0)).toString()==QStringLiteral("100"));
    CHECK(tw->model()->data(tw->model()->index(0,1)).toString()==QStringLiteral("0.998"));
    CHECK(tw->model()->data(tw->model()->index(1,0)).toString()==QStringLiteral("500"));
    CHECK(tw->model()->data(tw->model()->index(1,1)).toString()==QStringLiteral("0.954"));
    const QVariant initial=table->initialRuntimeValue(sample);
    CHECK(initial.toList().size()==5);
    CHECK(initial.toList().at(0).toList().size()==4);
    CHECK(initial.toList().at(0).toList().at(0).toString()==QStringLiteral("100"));
    // Editing Initial rows in Design must refresh the already-existing widget.
    auto editedDefinition=sample;
    editedDefinition.properties.insert(QStringLiteral("initialRows"),
                                       QStringLiteral("25|0.99||\n250|0.88||"));
    tw->configure(editedDefinition);
    CHECK(tw->rowCount()==2 && tw->model()->data(tw->model()->index(0,0)).toString()==QStringLiteral("25"));
    tw->configure(sample);
    CHECK(tw->rowCount()==5 && tw->model()->data(tw->model()->index(0,0)).toString()==QStringLiteral("100"));
    const QVariantList twoRows{
        QVariant(QVariantList{100,0.99,QStringLiteral("=SUM(1,2)"),QString()}),
        QVariant(QVariantList{1000,0.85,0.82,QStringLiteral("-3.5")})};
    CHECK(tw->setRows(twoRows));
    CHECK(!tw->setRows(QVariantList{QVariantList{1,2}}));
    CHECK(tw->rowCount()==2);
    CHECK(tw->model()->data(tw->model()->index(0,0)).toString()==QStringLiteral("100"));
    CHECK(tw->model()->data(tw->model()->index(0,1)).toString()==QStringLiteral("0.99"));
    CHECK(tw->model()->data(tw->model()->index(1,0)).toString()==QStringLiteral("1000"));
    CHECK(tw->rows().size()==2);
    CHECK(tw->rows().at(0).toList().size()==4);
    // v0.2-dev.8.9.1: a real AC laboratory needs 11 data columns.
    auto wide=sample;
    QStringList columnNames;
    for(int i=1;i<=11;++i)columnNames<<QStringLiteral("Measurement %1").arg(i);
    wide.properties.insert("columns",columnNames.join('|'));
    wide.properties.insert("initialRows",QStringLiteral("1|2|3|4|5|6|7|8|9|10|11"));
    wide.properties.insert("maxRows",500);
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(wide),&decoded,&err));
    auto* wideWidget=dynamic_cast<ExperimentDataTableWidget*>(table->create(wide,nullptr,{}));
    CHECK(wideWidget && wideWidget->columnCount()==11 && wideWidget->rowCount()==1);
    QVariantList wideRows;
    for(int r=0;r<501;++r) {
        QVariantList cells;
        for(int c=0;c<11;++c)cells<<QStringLiteral("%1-%2").arg(r).arg(c);
        wideRows.append(QVariant(cells));
    }
    CHECK(!wideWidget->setRows(wideRows)); // the 500-row instance cap is enforced
    wide.properties.insert("maxRows",2000);
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(wide),&decoded,&err));
    wideWidget->configure(wide);
    CHECK(wideWidget->setRows(wideRows));
    CHECK(wideWidget->rowCount()==501 && wideWidget->columnCount()==11);
    CHECK(wideWidget->model()->data(wideWidget->model()->index(500,10)).toString()==QStringLiteral("500-10"));
    // The exact upper column limit can be configured and rendered.
    QStringList manyColumns;
    for(int i=1;i<=ExperimentTableLimits::MaxColumns;++i)manyColumns<<QStringLiteral("C%1").arg(i);
    auto maxColumns=sample;
    maxColumns.properties.insert("columns",manyColumns.join('|'));
    maxColumns.properties.insert("initialRows",QString());
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(maxColumns),&decoded,&err));
    manyColumns<<QStringLiteral("extra");
    maxColumns.properties.insert("columns",manyColumns.join('|'));
    CHECK(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(maxColumns),&decoded,&err));
    CHECK(err.contains(QStringLiteral("columns")));
    maxColumns.properties.insert("maxRows",ExperimentTableLimits::MaxRows);
    maxColumns.properties.insert("columns",QStringLiteral("C1|C2"));
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(maxColumns),&decoded,&err));
    maxColumns.properties.insert("maxRows",ExperimentTableLimits::MaxRows+1);
    CHECK(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(maxColumns),&decoded,&err));
    CHECK(err.contains(QStringLiteral("maximum rows")));
    // Larger initial datasets used to fail the global 16 KiB property cap.
    auto largeInitial=sample;
    largeInitial.properties.insert("columns",QStringLiteral("Index|Observation"));
    largeInitial.properties.insert("maxRows",2000);
    QStringList initialRows;
    for(int i=0;i<500;++i)initialRows<<QStringLiteral("%1|%2").arg(i).arg(QString(100,QLatin1Char('x')));
    largeInitial.properties.insert("initialRows",initialRows.join('\n'));
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(largeInitial),&decoded,&err));
    CHECK(initialRows.join('\n').size()>16384);
    delete wideWidget;
    // Exercise the exact QJSEngine -> Qt variant conversion of ui.set(id, [[...]])
    // on the script thread, not merely hard-coded C++ QVariantList input.
    QJSEngine js;
    QJSValue jsRows=js.evaluate(QStringLiteral("[[100,0.998,'',''],[500,0.954,'','']]"));
    CHECK(!jsRows.isError());
    const QVariant fromJs=jsRows.toVariant(QJSValue::ConvertJSObjects);
    CHECK(fromJs.toList().size()==2);
    CHECK(fromJs.toList().at(0).toList().size()==4);
    CHECK(tw->setRows(fromJs));
    CHECK(tw->rowCount()==2 && tw->model()->data(tw->model()->index(0,0)).toString()==QStringLiteral("100"));
    CHECK(tw->model()->data(tw->model()->index(1,1)).toString()==QStringLiteral("0.954"));
    CHECK(tw->setRows(twoRows));
    // New default is lightweight even for an old Panel with no property.
    CHECK(!sample.properties.value("emitFullChangeEvent").toBool());
    int compactEvents=0,legacyEvents=0;
    tw->setChangeSink([&](const QString& event,const QVariant& value){
        if(event=="cellEdited") {
            const QVariantMap v=value.toMap();
            if(v.value("row").toInt()==0 && v.value("column").toInt()==1 &&
               v.value("value").toString()=="0.97")++compactEvents;
        }
        if(event=="valueChanged")++legacyEvents;
    });
    CHECK(tw->model()->setData(tw->model()->index(0,1),QStringLiteral("0.97"),Qt::EditRole));
    CHECK(compactEvents==1 && legacyEvents==0);
    // Opted-in legacy Panels still receive the snapshot, but only when explicitly enabled.
    auto legacy=sample;legacy.properties.insert("emitFullChangeEvent",true);
    tw->configure(legacy);
    bool signalled=false;
    tw->setChangeSink([&](const QString& name,const QVariant& payload){
        if(name=="valueChanged" && payload.toList().size()==2 &&
           payload.toList().at(0).toList().at(1).toString()=="0.96")signalled=true;
    });
    CHECK(tw->model()->setData(tw->model()->index(0,1),QStringLiteral("0.96"),Qt::EditRole));
    CHECK(signalled);
    tw->configure(sample);
    // GUI-only Clear synchronizes worker mirror without publishing valueChanged.
    int mirrorResets=0,mirrorBatches=0,clearEvents=0;
    tw->setChangeSink([&](const QString& event,const QVariant&){
        if(event=="__experimentTableMirrorReset")++mirrorResets;
        if(event=="__experimentTableMirrorAppend")++mirrorBatches;
        if(event=="cleared")++clearEvents;
        if(event=="valueChanged")++legacyEvents;
    });
    tw->clearFromToolbar();
    CHECK(tw->rowCount()==0 && mirrorResets==1 && mirrorBatches==0 &&
          clearEvents==1 && legacyEvents==0);
    CHECK(tw->setRows(twoRows));
    QTemporaryDir tmp;CHECK(tmp.isValid());
    const QString filename=tmp.filePath(QStringLiteral("test.csv"));
    CHECK(tw->exportCsv(filename,&err));
    QFile csv(filename);CHECK(csv.open(QIODevice::ReadOnly));
    const QByteArray bytes=csv.readAll();
    CHECK(bytes.contains("'=SUM(1,2)"));
    CHECK(bytes.contains("\"100\",\"0.99\""));
    CHECK(bytes.contains("\"-3.5\""));
    CHECK(tw->importCsv(filename,&err));
    CHECK(tw->rowCount()==2 && tw->model()->data(tw->model()->index(0,1)).toString()=="0.99");
    CHECK(mirrorResets==2 && mirrorBatches==1 && legacyEvents==0);
    // Large operator import: private mirror packets are always <=1024 rows.
    auto largeDefinition=sample;largeDefinition.properties.insert("maxRows",3000);
    tw->configure(largeDefinition);
    QVariantList largeRows;
    for(int n=0;n<2050;++n)
        largeRows.append(QVariant(QVariantList{n,QStringLiteral("%1").arg(n),
                         QStringLiteral("test"),QStringLiteral("V")}));
    CHECK(tw->setRows(largeRows));
    const QString largeFile=tmp.filePath(QStringLiteral("large.csv"));
    CHECK(tw->exportCsv(largeFile,&err));
    CHECK(tw->applyValue(QVariantMap{{"clear",true}}));
    int importedRows=0,importedChunks=0,resetCount=0,fullEvents=0;
    tw->setChangeSink([&](const QString& event,const QVariant& value){
        if(event=="__experimentTableMirrorReset")++resetCount;
        if(event=="__experimentTableMirrorAppend") {
            const auto rows=value.toMap().value("appendRows").toList();
            if(rows.size()<1 || rows.size()>1024)fullEvents+=1000;
            importedRows+=rows.size();++importedChunks;
        }
        if(event=="valueChanged")++fullEvents;
    });
    CHECK(tw->importCsv(largeFile,&err));
    CHECK(tw->rowCount()==2050 && resetCount==1 && importedChunks==3 &&
          importedRows==2050 && fullEvents==0);
    delete tw;
    PanelCanvas canvas;
    canvas.setDocumentSize(QSize(1250,800));
    CHECK(canvas.addControl(eq));
    CHECK(canvas.addControl(sample));
    QJsonArray encoded=canvas.toJson();
    CHECK(encoded.size()==2);
    CHECK(canvas.fromJson(encoded,&err));
    CHECK(canvas.toJson()==encoded);
    // Fit Page changes presentation only. The same JSON must save at 100%.
    canvas.setDesignMode(false);
    canvas.setPresentationScale(0.5);
    CHECK(canvas.size()==QSize(625,400));
    CHECK(canvas.documentSize()==QSize(1250,800));
    CHECK(canvas.toJson()==encoded);
    canvas.setDesignMode(true);
    CHECK(canvas.size()==QSize(1250,800));
    CHECK(canvas.toJson()==encoded);
    CHECK(canvas.activatePage("p0",encoded,QSize(1250,800),&err));
    CHECK(canvas.activatePage("p1",QJsonArray{},QSize(1250,800),&err));
    CHECK(canvas.activatePage("p0",encoded,QSize(1250,800),&err));
    CHECK(dynamic_cast<PanelEquationWidget*>(canvas.findChild<QWidget*>("eq1")));
    CHECK(dynamic_cast<ExperimentDataTableWidget*>(canvas.findChild<QWidget*>("data1")));
    qInfo()<<"PASS v8.9.1 equation/table: 32-column limit, 2000-row max, wider/ac-lab data, initial rows >16KiB, CSV, Panel JSON/pages";
    return 0;
}
