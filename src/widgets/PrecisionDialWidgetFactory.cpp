#include "PrecisionDialWidgetFactory.h"
#include "PrecisionDialWidget.h"
#include <QJsonObject>
#include <cmath>

QVector<WidgetProperty> PrecisionDialWidgetFactory::properties() const {
    WidgetProperty unit{"unit","Engineering unit","string",true};unit.defaultValue="V";
    WidgetProperty accent{"accent","Accent colour","choice",true};
    accent.choices={"cyan","green","amber","blue","magenta","white"};accent.defaultValue="cyan";
    WidgetProperty decimals{"decimals","Decimal places","integer",true,0,4};decimals.defaultValue=2;
    WidgetProperty alarm{"alarmEnabled","Enable high alarm","boolean",true};alarm.defaultValue=false;
    WidgetProperty alarmHigh{"alarmHigh","High alarm threshold","double",true,-1.e9,1.e9,3};alarmHigh.defaultValue=85.;
    WidgetProperty ticks{"showTicks","Show scale ticks","boolean",true};ticks.defaultValue=true;
    return {{"text","Title","string",true},{"value","Default value","double",true},
            {"min","Scale minimum","double",true},{"max","Scale maximum","double",true},
            unit,accent,decimals,alarm,alarmHigh,ticks};
}
PanelControl PrecisionDialWidgetFactory::defaultDefinition(const QString& id) const {
    PanelControl d=WidgetFactory::defaultDefinition(id);
    d.text="DC Voltage";d.geometry.setSize(QSize(320,260));d.value=48.5;
    return d;
}
QWidget* PrecisionDialWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* w=new PrecisionDialWidget(parent);applyDefinition(w,d);return w;
}
void PrecisionDialWidgetFactory::applyDefinition(QWidget* widget,const PanelControl& d) const {
    auto* w=qobject_cast<PrecisionDialWidget*>(widget);if(!w)return;
    const QJsonObject& p=d.properties;
    w->configure(d.text,d.value,d.minimum,d.maximum,
       p.value("unit").toString("V"),p.value("accent").toString("cyan"),
       p.value("decimals").toInt(2),p.value("alarmEnabled").toBool(false),
       p.value("alarmHigh").toDouble(85.),p.value("showTicks").toBool(true));
}
void PrecisionDialWidgetFactory::readDefinition(QWidget*,PanelControl&) const {
    // Runtime measurement belongs to the persistent widget; not the .wfpanel.
}
void PrecisionDialWidgetFactory::applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& value) const {
    auto* w=qobject_cast<PrecisionDialWidget*>(widget);if(!w)return;
    bool ok=false;const double v=value.toDouble(&ok);
    if(ok && std::isfinite(v))w->setValue(v);
}
