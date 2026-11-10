#include "EventLogWidgetFactory.h"
#include "EventLogWidget.h"
#include <QDebug>
#include <QtGlobal>

QVector<WidgetProperty> EventLogWidgetFactory::properties() const {
    WidgetProperty limit{"maxEntries","Maximum log entries","integer",true,10,2000};
    limit.defaultValue=500;limit.category=QStringLiteral("History");
    auto flag=[](const char* id,const char* label,bool def){
        WidgetProperty p{QString::fromLatin1(id),QString::fromLatin1(label),"boolean",true};
        p.defaultValue=def;p.category=QStringLiteral("Display");return p;
    };
    return {{"text","Log title","string",true},limit,
        flag("autoScroll","Auto-scroll when at bottom",true),
        flag("showTimestamp","Show timestamps",true),
        flag("showSource","Show source column",true)};
}
PanelControl EventLogWidgetFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);
    d.text=QStringLiteral("Instrument Event Log");
    d.geometry.setSize(QSize(680,300));
    d.properties.insert("maxEntries",500);
    d.properties.insert("autoScroll",true);
    d.properties.insert("showTimestamp",true);
    d.properties.insert("showSource",true);
    return d;
}
QWidget* EventLogWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* widget=new EventLogWidget(parent);
    applyDefinition(widget,d);
    return widget;
}
void EventLogWidgetFactory::applyDefinition(QWidget* base,const PanelControl& d) const {
    if(auto* widget=dynamic_cast<EventLogWidget*>(base)){
        const auto flag=[&d](const char* key,bool def){
            const QJsonValue v=d.properties.value(QString::fromLatin1(key));
            return v.isUndefined()?def:v.toBool(def);
        };
        widget->configure(d.text,d.properties.value("maxEntries").toInt(500),
                          flag("autoScroll",true),flag("showTimestamp",true),flag("showSource",true));
    }
}
void EventLogWidgetFactory::applyRuntimeValue(QWidget* base,const PanelControl&,const QVariant& data) const {
    if(auto* widget=dynamic_cast<EventLogWidget*>(base)) {
        if(!widget->applyEvents(data))
            qWarning()<<"WFScript Event Log: rejected ui.set payload for"
                      <<widget->objectName()<<"type="<<data.metaType().name();
    }
}
