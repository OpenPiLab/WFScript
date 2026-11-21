#include "UartTerminalWidgetFactory.h"
#include "UartTerminalWidget.h"
#include <utility>

QVector<WidgetProperty> UartTerminalWidgetFactory::properties() const {
    WidgetProperty maxLines{QStringLiteral("maxLines"),QStringLiteral("Maximum visible lines"),QStringLiteral("integer"),true,100,10000};
    maxLines.defaultValue=1000;maxLines.category=QStringLiteral("History");
    WidgetProperty hex{QStringLiteral("hexMode"),QStringLiteral("Display hexadecimal bytes"),QStringLiteral("boolean"),true};
    hex.defaultValue=false;hex.category=QStringLiteral("Display");
    return {{QStringLiteral("text"),QStringLiteral("Title"),QStringLiteral("string"),true},maxLines,hex};
}
PanelControl UartTerminalWidgetFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);
    d.text=QStringLiteral("UART Terminal — native receive");
    d.geometry.setSize(QSize(760,330));
    d.properties.insert(QStringLiteral("maxLines"),1000);
    d.properties.insert(QStringLiteral("hexMode"),false);
    return d;
}
QWidget* UartTerminalWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink sink) const {
    auto* widget=new UartTerminalWidget(parent);
    widget->setSendHandler([sink=std::move(sink)](const QString& message){
        sink(QStringLiteral("sendRequested"),message);
    });
    applyDefinition(widget,d);
    return widget;
}
void UartTerminalWidgetFactory::applyDefinition(QWidget* w,const PanelControl& d) const {
    if(auto* widget=dynamic_cast<UartTerminalWidget*>(w))
        widget->configure(d.text,d.properties.value(QStringLiteral("maxLines")).toInt(1000),
                          d.properties.value(QStringLiteral("hexMode")).toBool(false));
}
void UartTerminalWidgetFactory::applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& value) const {
    if(auto* widget=dynamic_cast<UartTerminalWidget*>(w))widget->applyUpdate(value);
}
