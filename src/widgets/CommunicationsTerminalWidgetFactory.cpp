#include "CommunicationsTerminalWidgetFactory.h"
#include "CommunicationsTerminalWidget.h"
#include <utility>

QVector<WidgetProperty> CommunicationsTerminalWidgetFactory::properties() const {
    WidgetProperty lines{QStringLiteral("maxLines"),QStringLiteral("Maximum visible entries"),QStringLiteral("integer"),true,50,5000};
    lines.defaultValue=500;lines.category=QStringLiteral("History");
    WidgetProperty preview{QStringLiteral("previewBytes"),QStringLiteral("Maximum bytes shown per event"),QStringLiteral("integer"),true,16,1024};
    preview.defaultValue=256;preview.category=QStringLiteral("History");
    WidgetProperty mode{QStringLiteral("displayMode"),QStringLiteral("Initial receive display"),QStringLiteral("choice"),true};
    mode.choices={QStringLiteral("Text"),QStringLiteral("Hex"),QStringLiteral("Both")};
    mode.defaultValue=QStringLiteral("Both");mode.category=QStringLiteral("Display");
    WidgetProperty timestamps{QStringLiteral("showTimestamp"),QStringLiteral("Show receive timestamps"),QStringLiteral("boolean"),true};
    timestamps.defaultValue=true;timestamps.category=QStringLiteral("Display");
    return {{QStringLiteral("text"),QStringLiteral("Terminal title"),QStringLiteral("string"),true},lines,preview,mode,timestamps};
}
PanelControl CommunicationsTerminalWidgetFactory::defaultDefinition(const QString& id) const {
    PanelControl d=WidgetFactory::defaultDefinition(id);
    d.text=QStringLiteral("Communications Terminal — Serial / TCP / UDP");
    d.geometry.setSize(QSize(770,370));
    d.properties.insert(QStringLiteral("maxLines"),500);
    d.properties.insert(QStringLiteral("previewBytes"),256);
    d.properties.insert(QStringLiteral("displayMode"),QStringLiteral("Both"));
    d.properties.insert(QStringLiteral("showTimestamp"),true);
    return d;
}
QWidget* CommunicationsTerminalWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink sink) const {
    auto* widget=new CommunicationsTerminalWidget(parent);
    widget->setSendHandler([sink=std::move(sink)](const QVariantMap& value){sink(QStringLiteral("sendRequested"),value);});
    applyDefinition(widget,d);
    return widget;
}
void CommunicationsTerminalWidgetFactory::applyDefinition(QWidget* w,const PanelControl& d) const {
    if(auto* widget=dynamic_cast<CommunicationsTerminalWidget*>(w))
        widget->configure(d.text,d.properties.value(QStringLiteral("maxLines")).toInt(500),
            d.properties.value(QStringLiteral("previewBytes")).toInt(256),
            d.properties.value(QStringLiteral("displayMode")).toString(QStringLiteral("Both")),
            d.properties.value(QStringLiteral("showTimestamp")).toBool(true));
}
void CommunicationsTerminalWidgetFactory::applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& value) const {
    if(auto* widget=dynamic_cast<CommunicationsTerminalWidget*>(w))widget->applyUpdate(value);
}
