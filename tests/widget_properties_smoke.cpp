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
#include <QPlainTextEdit>
#include <QPointer>
#include <QSpinBox>
#include <QDebug>
#include <QJsonArray>
#include <QSet>
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
    auto* typeLabel=inspector.findChild<QLabel*>(QStringLiteral("panelPropertyType"));
    CHECK(typeLabel && typeLabel->text()==QStringLiteral("No widget selected"));
    int editedCount=0;
    QObject::connect(&inspector,&WidgetPropertyInspector::edited,&inspector,
                     [&editedCount]{++editedCount;});
    PanelControl def=make("meter1");
    inspector.setControl(def,true);
    CHECK(typeLabel->text()==QStringLiteral("Test Meter (test.meter)"));
    CHECK(typeLabel->toolTip().contains(QStringLiteral("test.meter")));
    CHECK(editedCount==0); // Selection must not change the project.
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
    const int countBeforeSelection=editedCount;
    inspector.setControl(make("meter2"),true);
    CHECK(editedCount==countBeforeSelection);
    CHECK(typeLabel->text()==QStringLiteral("Test Meter (test.meter)"));
    CHECK(precision->value()==2 && units->currentText()=="V" && !alarm->isChecked());
    // Selection changes must always update the visible read-only factory type.
    const auto* buttonFactory=WidgetRegistry::instance().factory(QStringLiteral("button"));
    CHECK(buttonFactory);
    const PanelControl button=buttonFactory->defaultDefinition(QStringLiteral("button1"));
    inspector.setControl(button,true);
    CHECK(typeLabel->text().contains(buttonFactory->displayName()));
    CHECK(typeLabel->toolTip().contains(QStringLiteral("button")));
    CHECK(inspector.readControl(button).type==button.type);
    CHECK(editedCount==countBeforeSelection);
    // Legacy saved fields are shown, preserved, and never falsely editable.
    PanelControl legacyButton=button;
    legacyButton.properties.insert(QStringLiteral("cornerRadius"),10);
    legacyButton.properties.insert(QStringLiteral("accent"),QStringLiteral("cyan"));
    inspector.setControl(legacyButton,true);
    auto* saved=inspector.findChild<QPlainTextEdit*>(QStringLiteral("panelPropertyLegacySettings"));
    CHECK(saved && saved->isReadOnly());
    CHECK(saved->toPlainText().contains(QStringLiteral("cornerRadius")));
    CHECK(saved->toPlainText().contains(QStringLiteral("accent")));
    CHECK(inspector.readControl(legacyButton).properties.value(QStringLiteral("cornerRadius")).toInt()==10);
    CHECK(inspector.readControl(legacyButton).properties.value(QStringLiteral("accent")).toString()==QStringLiteral("cyan"));
    inspector.setControl(PanelControl{},false);
    CHECK(typeLabel->text()==QStringLiteral("No widget selected"));
    CHECK(typeLabel->toolTip().isEmpty());
    CHECK(editedCount==countBeforeSelection);
    // Old projects retain their original fields; no new object appears until needed.
    CHECK(!WidgetDocumentCodec::encode(def).contains("properties"));
    // Universal metadata is editor-backed for EVERY factory, not just the
    // hardcoded legacy widget types. No unsupported editor kinds are silent.
    auto& registry=WidgetRegistry::instance();
    const QSet<QString> kinds{QStringLiteral("string"),QStringLiteral("multiline"),
        QStringLiteral("choice"),QStringLiteral("boolean"),
        QStringLiteral("integer"),QStringLiteral("double")};
    int audited=0;
    for(const QString& type:registry.typeIds()) {
        const auto* factory=registry.factory(type);
        CHECK(factory);
        PanelControl sample=factory->defaultDefinition(QStringLiteral("audit_%1").arg(audited));
        inspector.setControl(sample,true);
        CHECK(inspector.editor(QStringLiteral("toolTip")));
        CHECK(inspector.editor(QStringLiteral("runEnabled")));
        CHECK(inspector.editor(QStringLiteral("runVisible")));
        QSet<QString> ids;
        for(const WidgetProperty& prop:registry.properties(type)) {
            if(prop.id==QStringLiteral("geometry") || prop.id==QStringLiteral("id") ||
               prop.id==QStringLiteral("type"))continue;
            if(prop.id.isEmpty() || !kinds.contains(prop.kind) || ids.contains(prop.id) ||
               !inspector.editor(prop.id)) {
                qCritical()<<"Factory property not editable:"<<type<<prop.id<<prop.kind;
                return 1;
            }
            ids.insert(prop.id);
        }
        // Factories may retain internal fields that are intentionally not
        // designer-editable (image bytes, fixed digital-chart restrictions).
        for(auto it=sample.properties.constBegin();it!=sample.properties.constEnd();++it) {
            const bool internal=(type==QStringLiteral("image") &&
                                  (it.key()==QStringLiteral("imageData") ||
                                   it.key()==QStringLiteral("imageName"))) ||
                (type==QStringLiteral("digital_plot") &&
                 (it.key()==QStringLiteral("chartType") ||
                  it.key()==QStringLiteral("xLog") ||
                  it.key()==QStringLiteral("showLegend")));
            if(!ids.contains(it.key()) && !internal) {
                qCritical()<<"Default setting omitted from Property Editor:"<<type<<it.key();
                return 1;
            }
        }
        CHECK(inspector.readControl(sample).type==sample.type);
        ++audited;
    }
    CHECK(audited>=29);
    // Shared properties can be edited without changing the specialized
    // factory contract and remain valid after serialization.
    inspector.setControl(def,true);
    auto* hint=inspector.findChild<QLineEdit*>(QStringLiteral("panelProperty_toolTip"));
    auto* enabled=inspector.findChild<QCheckBox*>(QStringLiteral("panelProperty_runEnabled"));
    auto* visible=inspector.findChild<QCheckBox*>(QStringLiteral("panelProperty_runVisible"));
    CHECK(hint && enabled && visible && enabled->isChecked() && visible->isChecked());
    hint->setText(QStringLiteral("Voltage measurement help"));
    enabled->setChecked(false);
    visible->setChecked(false);
    PanelControl common=inspector.readControl(def);
    CHECK(common.properties.value(QStringLiteral("toolTip")).toString()==
          QStringLiteral("Voltage measurement help"));
    CHECK(common.properties.value(QStringLiteral("runEnabled")).toBool(true)==false);
    CHECK(common.properties.value(QStringLiteral("runVisible")).toBool(true)==false);
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(common),&decoded,&error));
    CHECK(decoded.properties==common.properties);
    QJsonObject invalidCommon=WidgetDocumentCodec::encode(common);
    QJsonObject commonProps=common.properties;
    commonProps.insert(QStringLiteral("runVisible"),QStringLiteral("false"));
    invalidCommon.insert(QStringLiteral("properties"),commonProps);
    CHECK(!WidgetDocumentCodec::decode(invalidCommon,&decoded,&error));
    // Clearing shared values to defaults does not bloat old Panels.
    enabled->setChecked(true);
    visible->setChecked(true);
    hint->clear();
    const PanelControl cleared=inspector.readControl(def);
    CHECK(!cleared.properties.contains(QStringLiteral("toolTip")) &&
          !cleared.properties.contains(QStringLiteral("runEnabled")) &&
          !cleared.properties.contains(QStringLiteral("runVisible")));
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
    common.id=QStringLiteral("hiddenMeter");
    CHECK(canvas.addControl(common));
    auto* special=canvas.findChild<QWidget*>(QStringLiteral("hiddenMeter"));
    CHECK(special && special->toolTip()==QStringLiteral("Voltage measurement help"));
    CHECK(special->isEnabled() && !special->isHidden());
    canvas.setDesignMode(false);
    CHECK(!special->isEnabled() && special->isHidden());
    canvas.setDesignMode(true);
    CHECK(special->isEnabled() && !special->isHidden());
    // Ensure runtime visibility is re-applied when switching Pages, not just
    // on the initial Run transition.
    canvas.setDesignMode(false);
    CHECK(canvas.activatePage(QStringLiteral("third"),QJsonArray{},QSize(900,550),&error));
    CHECK(canvas.activatePage(QStringLiteral("main"),document,QSize(900,550),&error));
    CHECK(special->isHidden());
    canvas.setDesignMode(true);
    qInfo()<<"PASS: all factory properties, shared Run fields, validation, round trip, page switching";
    return 0;
}
