// Native Qt offscreen smoke for labeled Text and IPv4 fields. No hardware.
#include "PanelCanvas.h"
#include "PanelDataSchema.h"
#include "PanelUiBridge.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include "widgets/LabeledTextInputWidget.h"
#include "widgets/TextInputValidation.h"
#include <QApplication>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QImage>
#include <QDebug>
#include <QMetaObject>
#include <QKeyEvent>

#define CHECK(x) do { if(!(x)) {qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1;} } while(false)
static void enter(QLineEdit* line, const QString& text) {
    line->setFocus();
    line->selectAll();
    for (QChar c:text) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(c));
        QApplication::sendEvent(line, &press);
    }
}
static QJsonObject textVar(const QString& name, const QString& initial) {
    return QJsonObject{{"name", name}, {"type", "text"}, {"initial", initial}};
}
int main(int argc, char** argv) {
    QApplication app(argc,argv);
    auto& reg=WidgetRegistry::instance();
    const WidgetFactory* textFactory=reg.factory(QStringLiteral("text_input"));
    const WidgetFactory* ipFactory=reg.factory(QStringLiteral("ip_address_input"));
    CHECK(textFactory && ipFactory);
    CHECK(PanelDataSchema::bindingType("text_input")==QStringLiteral("text"));
    CHECK(PanelDataSchema::inputEvent("ip_address_input")==QStringLiteral("valueChanged"));
    CHECK(TextInputValidation::ipv4("192.168.6.2"));
    CHECK(TextInputValidation::ipv4("0.0.0.0"));
    CHECK(TextInputValidation::ipv4("255.255.255.255"));
    CHECK(!TextInputValidation::ipv4("256.1.1.1"));
    CHECK(!TextInputValidation::ipv4("192.168.1"));
    CHECK(!TextInputValidation::ipv4("192.168.01.1"));
    CHECK(!TextInputValidation::ipv4("test.example"));
    CHECK(!TextInputValidation::ipv4("2001:db8::1"));

    PanelControl name=textFactory->defaultDefinition(QStringLiteral("myName"));
    PanelControl host=ipFactory->defaultDefinition(QStringLiteral("myIp"));
    name.properties.insert(QStringLiteral("defaultText"),QStringLiteral("Bench Operator"));
    host.properties.insert(QStringLiteral("defaultText"),QStringLiteral("192.168.6.2"));
    QString error;
    PanelControl roundTrip;
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(name),&roundTrip,&error));
    CHECK(roundTrip.properties==name.properties);
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(host),&roundTrip,&error));
    CHECK(roundTrip.properties==host.properties);
    PanelControl invalid=host;
    invalid.properties.insert(QStringLiteral("defaultText"),QStringLiteral("999.168.1.1"));
    CHECK(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(invalid),&roundTrip,&error));
    invalid=host;
    invalid.properties.insert(QStringLiteral("labelPosition"),QStringLiteral("Middle"));
    CHECK(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(invalid),&roundTrip,&error));
    for(const QString& position:{QStringLiteral("Top"),QStringLiteral("Bottom"),
                                 QStringLiteral("Left"),QStringLiteral("Right")}) {
        name.properties.insert(QStringLiteral("labelPosition"),position);
        QWidget root;
        auto* raw=textFactory->create(name,&root,[](const QString&,const QVariant&){});
        auto* field=qobject_cast<LabeledTextInputWidget*>(raw);
        CHECK(field && field->labelPosition()==position);
        field->resize(340,96);
        root.show(); field->show();
        QApplication::processEvents();
        QImage image(field->size(),QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        field->render(&image);
        CHECK(image.pixelColor(image.width()/2,image.height()/2).alpha()>0);
        CHECK(field->editor()->geometry().width()>45);
        CHECK(field->text()==QStringLiteral("Bench Operator"));
    }
    host.properties.insert(QStringLiteral("labelPosition"),QStringLiteral("Right"));
    QWidget hostRoot;
    int accepted=0,committed=0,rejected=0;
    QString current;
    auto* raw=ipFactory->create(host,&hostRoot,[&](const QString& event,const QVariant& value){
        if(event==QStringLiteral("valueChanged")){++accepted;current=value.toString();}
        if(event==QStringLiteral("editingFinished"))++committed;
        if(event==QStringLiteral("validationFailed"))++rejected;
    });
    auto* field=qobject_cast<LabeledTextInputWidget*>(raw);
    CHECK(field);
    hostRoot.show();field->show();
    QApplication::processEvents();
    CHECK(ipFactory->initialRuntimeValue(host).toString()==QStringLiteral("192.168.6.2"));
    ipFactory->applyRuntimeValue(field,host,QStringLiteral("192.168.6.4"));
    CHECK(field->text()==QStringLiteral("192.168.6.4") && accepted==0);
    enter(field->editor(),QStringLiteral("300.2.3.4"));
    CHECK(QMetaObject::invokeMethod(field->editor(),"editingFinished",Qt::DirectConnection));
    CHECK(rejected==1 && accepted==0 && committed==0);
    enter(field->editor(),QStringLiteral("10.20.30.40"));
    CHECK(QMetaObject::invokeMethod(field->editor(),"editingFinished",Qt::DirectConnection));
    CHECK(accepted>=1 && committed==1 && current==QStringLiteral("10.20.30.40"));

    // All controls use factory-owned text state; save should never take a live
    // operator edit and replace the authored default value.
    PanelCanvas canvas;
    name.properties.insert(QStringLiteral("labelPosition"),QStringLiteral("Top"));
    CHECK(canvas.addControl(name));
    CHECK(canvas.addControl(host));
    CHECK(canvas.initialValues().value("myName").toString()==QStringLiteral("Bench Operator"));
    CHECK(canvas.initialValues().value("myIp").toString()==QStringLiteral("192.168.6.2"));
    canvas.applyValue(QStringLiteral("myName"),QStringLiteral("Updated at Run"));
    CHECK(canvas.toJson().at(0).toObject().value("properties").toObject()
        .value("defaultText").toString()==QStringLiteral("Bench Operator"));

    // Worker bridge must reject invalid IP values *before* ui.get / Data.set
    // diverges from the value actually displayed in the Qt input.
    host.properties.insert(QStringLiteral("maxLength"),15);
    host.properties.insert(QStringLiteral("defaultText"),QStringLiteral("192.168.6.2"));
    host.bindings.insert(QStringLiteral("value"),QStringLiteral("Bench.IP"));
    const auto bound=WidgetDocumentCodec::encode(host);
    const QJsonArray vars{textVar("Bench.IP","192.168.6.2")};
    const QJsonArray controls{bound,WidgetDocumentCodec::encode(name)};
    QJSEngine engine;
    PanelUiBridge bridge(&engine,QVariantMap{{"myIp",QStringLiteral("192.168.6.2")},
                                             {"myName",QStringLiteral("Bench Operator")}},
                         {},vars,controls);
    CHECK(!bridge.set("myIp",QJSValue(QStringLiteral("127.0.0.1")))); // bound
    CHECK(!bridge.set("myName",QJSValue(42))); // strict text type
    CHECK(bridge.set("myName",QJSValue(QStringLiteral("Changed"))));
    CHECK(bridge.get("myName").toString()==QStringLiteral("Changed"));
    CHECK(!bridge.setData("Bench.IP",QJSValue(QStringLiteral("256.1.1.1"))));
    CHECK(bridge.getData("Bench.IP").toString()==QStringLiteral("192.168.6.2"));
    CHECK(bridge.setData("Bench.IP",QJSValue(QStringLiteral("192.168.6.4"))));
    CHECK(bridge.getData("Bench.IP").toString()==QStringLiteral("192.168.6.4"));
    CHECK(bridge.setDataFromInput("myIp","valueChanged",QVariant(QStringLiteral("10.20.30.40"))));
    CHECK(bridge.getData("Bench.IP").toString()==QStringLiteral("10.20.30.40"));
    CHECK(!bridge.setDataFromInput("myIp","valueChanged",QVariant(QStringLiteral("10.20.30.999"))));
    CHECK(bridge.getData("Bench.IP").toString()==QStringLiteral("10.20.30.40"));
    qInfo()<<"PASS: label positions, IP validation, user events, ui value, Panel Data, JSON";
    return 0;
}
