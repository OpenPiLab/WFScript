#include "MeasurementResultWidgetFactory.h"
#include "MeasurementResultWidget.h"
#include <cmath>

QVector<WidgetProperty> MeasurementResultWidgetFactory::properties() const {
    WidgetProperty unit{"unit","Engineering unit","string",true};unit.defaultValue="V";
    WidgetProperty decimals{"decimals","Decimal places","integer",true,0,6};decimals.defaultValue=3;
    return {{"text","Measurement name","string",true},
            {"min","Lower PASS limit (inclusive)","double",true},
            {"max","Upper PASS limit (inclusive)","double",true},
            unit,decimals};
}
PanelControl MeasurementResultWidgetFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);
    d.text="V+ SUPPLY";d.minimum=3.25;d.maximum=3.35;d.value=0;
    d.geometry.setSize(QSize(310,138));d.properties.insert("unit","V");
    d.properties.insert("decimals",3);return d;
}
QWidget* MeasurementResultWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* w=new MeasurementResultWidget(parent);applyDefinition(w,d);return w;
}
void MeasurementResultWidgetFactory::applyDefinition(QWidget* w,const PanelControl& d) const {
    auto* result=dynamic_cast<MeasurementResultWidget*>(w);if(!result)return;
    result->configure(d.text,d.properties.value("unit").toString("V"),d.minimum,d.maximum,
                      d.properties.value("decimals").toInt(3));
}
void MeasurementResultWidgetFactory::readDefinition(QWidget*,PanelControl&) const {
    // Test readings are transient runtime state; never persist them into a Panel.
}
void MeasurementResultWidgetFactory::applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& value) const {
    if(auto* result=dynamic_cast<MeasurementResultWidget*>(w))result->applyMeasurement(value);
}
