#include "SegmentedBarWidgetFactory.h"
#include "SegmentedBarWidget.h"
#include <QJsonObject>
#include <cmath>

QVector<WidgetProperty> SegmentedBarWidgetFactory::properties() const {
    WidgetProperty unit{"unit","Engineering unit","string",true};unit.defaultValue="%";
    WidgetProperty accent{"accent","Active colour","choice",true};
    accent.choices={"green","cyan","amber","blue","magenta","white"};accent.defaultValue="green";
    WidgetProperty decimals{"decimals","Decimal places","integer",true,0,4};decimals.defaultValue=1;
    WidgetProperty segments{"segments","Segments","integer",true,5,48};segments.defaultValue=28;
    WidgetProperty warn{"warningPct","Warning (% of range)","integer",true,0,100};warn.defaultValue=70;
    WidgetProperty alarm{"alarmPct","Alarm (% of range)","integer",true,0,100};alarm.defaultValue=90;
    WidgetProperty peak{"peakHold","Hold peak marker","boolean",true};peak.defaultValue=true;
    return {{"text","Title","string",true},{"value","Default value","double",true},
            {"min","Minimum","double",true},{"max","Maximum","double",true},
            unit,accent,decimals,segments,warn,alarm,peak};
}
PanelControl SegmentedBarWidgetFactory::defaultDefinition(const QString& id) const {
    PanelControl d=WidgetFactory::defaultDefinition(id);
    d.text="Output Load";d.value=64.0;d.geometry.setSize(QSize(355,140));return d;
}
QWidget* SegmentedBarWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* w=new SegmentedBarWidget(parent);applyDefinition(w,d);return w;
}
void SegmentedBarWidgetFactory::applyDefinition(QWidget* widget,const PanelControl& d) const {
    auto* w=qobject_cast<SegmentedBarWidget*>(widget);if(!w)return;
    const QJsonObject& p=d.properties;
    w->configure(d.text,d.value,d.minimum,d.maximum,
       p.value("unit").toString("%"),p.value("accent").toString("green"),
       p.value("decimals").toInt(1),p.value("segments").toInt(28),
       p.value("warningPct").toInt(70),p.value("alarmPct").toInt(90),
       p.value("peakHold").toBool(true));
}
void SegmentedBarWidgetFactory::readDefinition(QWidget*,PanelControl&) const {
    // No transient peak/value copy-back to disk.
}
void SegmentedBarWidgetFactory::applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& value) const {
    auto* w=qobject_cast<SegmentedBarWidget*>(widget);if(!w)return;
    bool ok=false;const double v=value.toDouble(&ok);
    if(ok && std::isfinite(v))w->setValue(v);
}
