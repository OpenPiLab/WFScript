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
    // Regression: QList<QVariant>::append(QVariantList) used to FLATTEN rows.
    // Verify the source/property data survive as 5 rows of 4 columns.
    CHECK(tw->item(0,0)->text()==QStringLiteral("100"));
    CHECK(tw->item(0,1)->text()==QStringLiteral("0.998"));
    CHECK(tw->item(1,0)->text()==QStringLiteral("500"));
    CHECK(tw->item(1,1)->text()==QStringLiteral("0.954"));
    const QVariant initial=table->initialRuntimeValue(sample);
    CHECK(initial.toList().size()==5);
    CHECK(initial.toList().at(0).toList().size()==4);
    CHECK(initial.toList().at(0).toList().at(0).toString()==QStringLiteral("100"));
    // Editing Initial rows in Design must refresh the already-existing widget.
    auto editedDefinition=sample;
    editedDefinition.properties.insert(QStringLiteral("initialRows"),
                                       QStringLiteral("25|0.99||\n250|0.88||"));
    tw->configure(editedDefinition);
    CHECK(tw->rowCount()==2 && tw->item(0,0)->text()==QStringLiteral("25"));
    tw->configure(sample);
    CHECK(tw->rowCount()==5 && tw->item(0,0)->text()==QStringLiteral("100"));
    const QVariantList twoRows{
        QVariant(QVariantList{100,0.99,QStringLiteral("=SUM(1,2)"),QString()}),
        QVariant(QVariantList{1000,0.85,0.82,QStringLiteral("-3.5")})};
    CHECK(tw->setRows(twoRows));
    CHECK(!tw->setRows(QVariantList{QVariantList{1,2}}));
    CHECK(tw->rowCount()==2);
    CHECK(tw->item(0,0)->text()==QStringLiteral("100"));
    CHECK(tw->item(0,1)->text()==QStringLiteral("0.99"));
    CHECK(tw->item(1,0)->text()==QStringLiteral("1000"));
    CHECK(tw->rows().size()==2);
    CHECK(tw->rows().at(0).toList().size()==4);
    // Exercise the exact QJSEngine -> Qt variant conversion of ui.set(id, [[...]])
    // on the script thread, not merely hard-coded C++ QVariantList input.
    QJSEngine js;
    QJSValue jsRows=js.evaluate(QStringLiteral("[[100,0.998,'',''],[500,0.954,'','']]"));
    CHECK(!jsRows.isError());
    const QVariant fromJs=jsRows.toVariant(QJSValue::ConvertJSObjects);
    CHECK(fromJs.toList().size()==2);
    CHECK(fromJs.toList().at(0).toList().size()==4);
    CHECK(tw->setRows(fromJs));
    CHECK(tw->rowCount()==2 && tw->item(0,0)->text()==QStringLiteral("100"));
    CHECK(tw->item(1,1)->text()==QStringLiteral("0.954"));
    CHECK(tw->setRows(twoRows));
    bool signalled=false;
    tw->setChangeSink([&](const QString& name,const QVariant& payload){
        signalled=name==QStringLiteral("valueChanged") && payload.toList().size()==2 &&
            payload.toList().at(0).toList().size()==4 &&
            payload.toList().at(0).toList().at(1).toString()==QStringLiteral("0.97");
    });
    tw->item(0,1)->setText(QStringLiteral("0.97"));
    CHECK(signalled);
    QTemporaryDir tmp;CHECK(tmp.isValid());
    const QString filename=tmp.filePath(QStringLiteral("test.csv"));
    CHECK(tw->exportCsv(filename,&err));
    QFile csv(filename);CHECK(csv.open(QIODevice::ReadOnly));
    const QByteArray bytes=csv.readAll();
    CHECK(bytes.contains("'=SUM(1,2)"));
    CHECK(bytes.contains("\"100\",\"0.97\""));
    CHECK(bytes.contains("\"-3.5\""));
    delete tw;
    PanelCanvas canvas;
    canvas.setDocumentSize(QSize(1250,800));
    CHECK(canvas.addControl(eq));
    CHECK(canvas.addControl(sample));
    QJsonArray encoded=canvas.toJson();
    CHECK(encoded.size()==2);
    CHECK(canvas.fromJson(encoded,&err));
    CHECK(canvas.toJson()==encoded);
    CHECK(canvas.activatePage("p0",encoded,QSize(1250,800),&err));
    CHECK(canvas.activatePage("p1",QJsonArray{},QSize(1250,800),&err));
    CHECK(canvas.activatePage("p0",encoded,QSize(1250,800),&err));
    CHECK(dynamic_cast<PanelEquationWidget*>(canvas.findChild<QWidget*>("eq1")));
    CHECK(dynamic_cast<ExperimentDataTableWidget*>(canvas.findChild<QWidget*>("data1")));
    qInfo()<<"PASS v5.6.70.1 equation renderer, UI preview, bounded table, user edit, safe CSV, Panel JSON/pages";
    return 0;
}
