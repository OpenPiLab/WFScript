#include "StatusBeaconWidgetFactory.h"
#include "StatusBeaconWidget.h"
#include <QJsonObject>
#include <cmath>

QVector<WidgetProperty> StatusBeaconWidgetFactory::properties() const {
    WidgetProperty color{"activeColor","Active LED colour","choice",true};
    color.choices={"green","cyan","amber","blue","magenta","red"};color.defaultValue="green";
    WidgetProperty onText{"onText","Active label","string",true};onText.defaultValue="ONLINE";
    WidgetProperty offText{"offText","Inactive label","string",true};offText.defaultValue="OFFLINE";
    WidgetProperty invert{"invert","Invert input","boolean",true};invert.defaultValue=false;
    WidgetProperty glow{"showGlow","Illumination glow","boolean",true};glow.defaultValue=true;
    return {{"text","Title","string",true},{"value","Default state (0 / 1)","double",true},
            color,onText,offText,invert,glow};
}
PanelControl StatusBeaconWidgetFactory::defaultDefinition(const QString& id) const {
    PanelControl d=WidgetFactory::defaultDefinition(id);
    d.text="System Ready";d.value=1.0;d.geometry.setSize(QSize(290,120));return d;
}
QWidget* StatusBeaconWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* w=new StatusBeaconWidget(parent);applyDefinition(w,d);return w;
}
void StatusBeaconWidgetFactory::applyDefinition(QWidget* widget,const PanelControl& d) const {
    auto* w=qobject_cast<StatusBeaconWidget*>(widget);if(!w)return;
    const QJsonObject& p=d.properties;
    w->configure(d.text,d.value,p.value("activeColor").toString("green"),
        p.value("onText").toString("ONLINE"),p.value("offText").toString("OFFLINE"),
        p.value("invert").toBool(false),p.value("showGlow").toBool(true));
}
void StatusBeaconWidgetFactory::readDefinition(QWidget*,PanelControl&) const {
    // Do not persist transient runtime status updates.
}
void StatusBeaconWidgetFactory::applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& value) const {
    auto* w=qobject_cast<StatusBeaconWidget*>(widget);if(!w)return;
    bool ok=false;const double v=value.toDouble(&ok);
    if(ok && std::isfinite(v))w->setValue(v);
}
