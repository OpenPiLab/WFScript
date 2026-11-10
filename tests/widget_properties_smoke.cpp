// v5.6.30: metadata-driven inspector + legacy/extended JSON round trip.
// Native Qt smoke: no instrument hardware or QJSEngine required.
#include "PanelCanvas.h"
#include "widgets/WidgetPropertyInspector.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSpinBox>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

#define CHECK(expr) do {if(!(expr)){qCritical()<<"FAIL"<<#expr<<"line"<<__LINE__;return 1;}}while(false)
namespace {
class CustomMeter final : public WidgetFactory {
public:
    QString typeId() const override {return "test.meter";}
    QString displayName() const override {return "Test Meter";}
    QVector<WidgetProperty> properties() const override {
        WidgetProperty precision{"precision","Precision","integer",true,0,6};
        precision.defaultValue=2;
        WidgetProperty units{"units","Units","choice",true};
        units.choices={"V","mV","A"};units.defaultValue="V";
        WidgetProperty alarm{"alarm","Alarm Enabled","boolean",true};
        alarm.defaultValue=false;
        WidgetProperty gain{"gain","Gain","double",true,0,10,2};
        gain.defaultValue=1.0;
        return {{"text","Title","string",true},precision,units,alarm,gain};
    }
    QWidget* create(const PanelControl& c,QWidget* parent,EventSink) const override {
        return new QLabel(c.text,parent);
    }
    void applyDefinition(QWidget* w,const PanelControl& d) const override {
        qobject_cast<QLabel*>(w)->setText(d.text);
    }
    void readDefinition(QWidget* w,PanelControl& d) const override {
        d.text=qobject_cast<QLabel*>(w)->text();
    }
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override {
        qobject_cast<QLabel*>(w)->setText(v.toString());
    }
};
PanelControl make(const QString& id) {
    PanelControl c;c.id=id;c.type="test.meter";c.text="Meter";
    c.geometry=QRect(20,40,170,55);
    return c;
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    CHECK(WidgetRegistry::instance().registerFactory(std::make_unique<CustomMeter>()));
    WidgetPropertyInspector inspector;
    PanelControl def=make("meter1");
    inspector.setControl(def,true);
    auto* precision=inspector.findChild<QSpinBox*>("panelProperty_precision");
    auto* units=inspector.findChild<QComboBox*>("panelProperty_units");
    auto* alarm=inspector.findChild<QCheckBox*>("panelProperty_alarm");
    auto* gain=inspector.findChild<QDoubleSpinBox*>("panelProperty_gain");
    CHECK(precision && units && alarm && gain);
    CHECK(precision->value()==2 && units->currentText()=="V");
    precision->setValue(4);
    units->setCurrentText("mV");
    alarm->setChecked(true);
    gain->setValue(1.25);
    const PanelControl changed=inspector.readControl(def);
    CHECK(changed.properties.value("precision").toInt()==4);
    CHECK(changed.properties.value("units").toString()=="mV");
    CHECK(changed.properties.value("alarm").toBool());
    CHECK(changed.properties.value("gain").toDouble()==1.25);
    const QJsonObject encoded=WidgetDocumentCodec::encode(changed);
    CHECK(encoded.contains("properties"));
    PanelControl decoded;
    QString error;
    CHECK(WidgetDocumentCodec::decode(encoded,&decoded,&error));
    CHECK(decoded.properties==changed.properties);
    QJsonObject invalid=encoded;
    QJsonObject badSettings=changed.properties;
    badSettings.insert("precision","invalid");
    invalid.insert("properties",badSettings);
    CHECK(!WidgetDocumentCodec::decode(invalid,&decoded,&error));
    // A future field is retained, so a project can be reopened without data loss.
    QJsonObject extra=encoded;
    QJsonObject forward=changed.properties;
    forward.insert("futureSetting",QStringLiteral("keep-me"));
    extra.insert("properties",forward);
    CHECK(WidgetDocumentCodec::decode(extra,&decoded,&error));
    CHECK(WidgetDocumentCodec::encode(decoded).value("properties").toObject()==forward);
    // Editor values must not leak between two widgets of the same type.
    inspector.setControl(make("meter2"),true);
    CHECK(precision->value()==2 && units->currentText()=="V" && !alarm->isChecked());
    // Old projects retain their original fields; no new object appears until needed.
    CHECK(!WidgetDocumentCodec::encode(def).contains("properties"));
    PanelCanvas canvas;
    CHECK(canvas.addControl(changed));
    auto* live=canvas.findChild<QWidget*>("meter1");
    CHECK(live);
    const QJsonArray document=canvas.toJson();
    CHECK(document.at(0).toObject().value("properties").toObject()==changed.properties);
    CHECK(canvas.activatePage("main",document,QSize(900,550),&error));
    CHECK(canvas.activatePage("other",QJsonArray{},QSize(900,550),&error));
    CHECK(live->isHidden());
    CHECK(canvas.activatePage("main",document,QSize(900,550),&error));
    CHECK(live==canvas.findChild<QWidget*>("meter1"));
    qInfo()<<"PASS: v5.6.30 metadata inspector, type validation, round trip, persistent widgets";
    return 0;
}
