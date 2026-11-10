// v5.6.64: generated Qt Run Information form responds to project schema.
#include "widgets/RunInformationWidget.h"
#include "RunInformationSchema.h"
#include <QApplication>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QLabel>
#include <QScrollArea>
#include <QEvent>
#include <QPalette>
#include <QListView>
#define REQUIRE(x) do{if(!(x))qFatal("RunInfo widget failed: %s at line %d",#x,__LINE__);}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QJsonObject schema{{"fields",QJsonArray{
        QJsonObject{{"id","uutId"},{"label","Serial"},{"type","text"},{"required",true}},
        QJsonObject{{"id","revision"},{"label","Revision"},{"type","choice"},
                    {"options",QJsonArray{"A","B"}},{"default","B"}},
        QJsonObject{{"id","inspection"},{"label","Inspection"},{"type","boolean"}}
    }}};
    QWidget canvas;
    canvas.setProperty("wfPanelLight",false);
    RunInformationWidget widget(&canvas);
    widget.setSchema(schema);
    widget.resize(440,320);
    widget.show();
    app.processEvents();
    auto* viewport=widget.findChild<QWidget*>("wfRunInfoViewport");
    auto* content=widget.findChild<QWidget*>("wfRunInfoContent");
    auto* link=widget.findChild<QLabel*>("wfRunInfoLink");
    auto* fieldLabel=widget.findChild<QLabel*>("wfRunInfoFieldLabel");
    REQUIRE(viewport && content && link && fieldLabel);
    REQUIRE(link->text().contains("SHARED UUT INPUTS"));
    REQUIRE(fieldLabel->text().contains("Serial"));
    // Regression: native scroll viewports MUST NOT be white under white ink.
    REQUIRE(viewport->palette().color(QPalette::Window).name()=="#162638");
    REQUIRE(content->palette().color(QPalette::Window).name()=="#162638");
    REQUIRE(fieldLabel->styleSheet().contains("#e5eef7"));
    REQUIRE(!widget.styleSheet().isEmpty());
    canvas.setProperty("wfPanelLight",true);
    QEvent themeChanged(QEvent::ThemeChange);
    QApplication::sendEvent(&widget,&themeChanged);
    app.processEvents();
    REQUIRE(viewport->palette().color(QPalette::Window).name()=="#eef5fa");
    REQUIRE(fieldLabel->styleSheet().contains("#20384c"));
    canvas.setProperty("wfPanelLight",false);
    QApplication::sendEvent(&widget,&themeChanged);
    app.processEvents();
    auto* serial=widget.findChild<QLineEdit*>("uut_uutId");
    auto* rev=widget.findChild<QComboBox*>("uut_revision");
    auto* inspection=widget.findChild<QCheckBox*>("uut_inspection");
    REQUIRE(serial && rev && inspection);
    REQUIRE(serial->styleSheet().contains("#e5eef7"));
    REQUIRE(rev->styleSheet().contains("#e5eef7"));
    REQUIRE(rev->view()->palette().color(QPalette::Text).name()=="#e5eef7");
    int notifications=0;
    QObject::connect(&widget,&RunInformationWidget::valuesChanged,&widget,[&](const QVariantMap&){++notifications;});
    serial->setText("UNIT-142");
    REQUIRE(notifications==1);
    REQUIRE(widget.values().value("uutId").toString()=="UNIT-142");
    REQUIRE(widget.values().value("revision").toString()=="B");
    QVariantMap normalized;
    QString error;
    REQUIRE(RunInformationSchema::validateValues(schema,widget.values(),&normalized,&error));
    REQUIRE(normalized.value("uutId").toString()=="UNIT-142");
    serial->clear();
    REQUIRE(!RunInformationSchema::validateValues(schema,widget.values(),&normalized,&error));
    widget.setValues(QVariantMap{{"uutId","UNIT-B"},{"revision","A"},{"inspection",true}});
    REQUIRE(widget.values().value("inspection").toBool());
    REQUIRE(widget.values().value("uutId").toString()=="UNIT-B");
    qInfo()<<"PASS: native Qt dark/light form contrast, project link, fields, validation";
    return 0;
}
