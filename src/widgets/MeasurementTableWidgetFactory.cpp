#include "MeasurementTableWidgetFactory.h"
#include "MeasurementTableWidget.h"
#include <QDebug>
#include <QJsonValue>

QVector<WidgetProperty> MeasurementTableWidgetFactory::properties() const {
    WidgetProperty decimals{"decimals","Decimal places","integer",true,0,6};decimals.defaultValue=3;
    WidgetProperty maxRows{"maxRows","Maximum rows (legacy display setting)","integer",true,1,100};maxRows.defaultValue=100;
    WidgetProperty headers{"headers","Headers (4 fields separated by |)","string",true};
    headers.defaultValue="Measurement|Actual|Limits|Result";
    auto option=[](const char* id,const char* label,bool enabled,const char* category) {
        WidgetProperty p{QString::fromLatin1(id),QString::fromLatin1(label),"boolean",true};
        p.defaultValue=enabled;p.category=QString::fromLatin1(category);return p;
    };
    WidgetProperty title{"text","Table title","string",true};title.category="Header";
    decimals.category="Table";maxRows.category="Table";headers.category="Table";
    return {title,decimals,maxRows,headers,
        option("showToolbar","Show title and toolbar",true,"Header"),
        option("showRowCount","Show result count",true,"Header"),
        option("showFilter","Show result filter",true,"Toolbar"),
        option("showCopyButton","Show Copy button",true,"Toolbar"),
        option("showExportButton","Show Export CSV button",true,"Toolbar"),
        // The native Test Executive owns the authoritative report. Clearing
        // this view must not appear to clear those results by default.
        option("showClearButton","Show Clear View (display only)",false,"Toolbar"),
        option("autoScroll","Follow newest results when at bottom",true,"Display")};
}
PanelControl MeasurementTableWidgetFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);
    d.text="Measurement results";
    d.geometry.setSize(QSize(700,335));
    d.properties.insert("decimals",3);d.properties.insert("maxRows",100);
    d.properties.insert("headers","Measurement|Actual|Limits|Result");
    d.properties.insert("showToolbar",true);d.properties.insert("showRowCount",true);
    d.properties.insert("showFilter",true);d.properties.insert("showCopyButton",true);
    d.properties.insert("showExportButton",true);d.properties.insert("showClearButton",false);
    d.properties.insert("autoScroll",true);
    return d;
}
QWidget* MeasurementTableWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* w=new MeasurementTableWidget(parent);applyDefinition(w,d);return w;
}
void MeasurementTableWidgetFactory::applyDefinition(QWidget* w,const PanelControl& d) const {
    auto* table=dynamic_cast<MeasurementTableWidget*>(w);if(!table)return;
    const auto flag=[&d](const char* key,bool def) {
        const QJsonValue value=d.properties.value(QString::fromLatin1(key));
        return value.isUndefined()?def:value.toBool(def);
    };
    table->configure(d.properties.value("decimals").toInt(3),
                     d.properties.value("maxRows").toInt(100),
                     d.properties.value("headers").toString("Measurement|Actual|Limits|Result"),
                     d.text,flag("showToolbar",true),flag("showRowCount",true),
                     flag("showFilter",true),flag("showCopyButton",true),
                     flag("showExportButton",true),flag("showClearButton",false),
                     flag("autoScroll",true));
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
