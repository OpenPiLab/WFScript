#include "MeasurementTableWidgetFactory.h"
#include "MeasurementTableWidget.h"
#include <QDebug>

QVector<WidgetProperty> MeasurementTableWidgetFactory::properties() const {
    WidgetProperty decimals{"decimals","Decimal places","integer",true,0,6};decimals.defaultValue=3;
    WidgetProperty maxRows{"maxRows","Maximum rows","integer",true,1,100};maxRows.defaultValue=100;
    WidgetProperty headers{"headers","Headers (4 fields separated by |)","string",true};
    headers.defaultValue="Measurement|Actual|Limits|Result";
    return {decimals,maxRows,headers};
}
PanelControl MeasurementTableWidgetFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);
    d.text="Measurement results";
    d.geometry.setSize(QSize(660,320));
    d.properties.insert("decimals",3);d.properties.insert("maxRows",100);
    d.properties.insert("headers","Measurement|Actual|Limits|Result");
    return d;
}
QWidget* MeasurementTableWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* w=new MeasurementTableWidget(parent);applyDefinition(w,d);return w;
}
void MeasurementTableWidgetFactory::applyDefinition(QWidget* w,const PanelControl& d) const {
    auto* table=dynamic_cast<MeasurementTableWidget*>(w);if(!table)return;
    table->configure(d.properties.value("decimals").toInt(3),
                     d.properties.value("maxRows").toInt(100),
                     d.properties.value("headers").toString("Measurement|Actual|Limits|Result"));
}
void MeasurementTableWidgetFactory::readDefinition(QWidget*,PanelControl&) const {
    // Live rows are not part of the design-time persisted document.
}
void MeasurementTableWidgetFactory::applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& rows) const {
    if(auto* table=dynamic_cast<MeasurementTableWidget*>(w)) {
        if(!table->setMeasurements(rows))
            qWarning() << "WFScript Measurement Table: rejected ui.set payload for"
                       << table->objectName() << "type=" << rows.metaType().name();
    }
}
