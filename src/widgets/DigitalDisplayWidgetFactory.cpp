#include "DigitalDisplayWidgetFactory.h"
#include "DigitalDisplayWidget.h"
#include <QJsonObject>
#include <cmath>

QVector<WidgetProperty> DigitalDisplayWidgetFactory::properties() const {
    WidgetProperty decimals{"decimals","Decimal places","integer",true,0,6};
    decimals.defaultValue=3;
    WidgetProperty unit{"unit","Engineering unit","string",true};
    unit.defaultValue=QStringLiteral("V");
    WidgetProperty accent{"accent","Accent colour","choice",true};
    accent.choices={"cyan","green","amber","blue","white"};
    accent.defaultValue=QStringLiteral("cyan");
    WidgetProperty showPlus{"showPlus","Show positive sign","boolean",true};
    showPlus.defaultValue=false;
    WidgetProperty grouping{"groupThousands","Thousands separators","boolean",true};
    grouping.defaultValue=false;
    WidgetProperty highAlarm{"alarmEnabled","Enable high alarm","boolean",true};
    highAlarm.defaultValue=false;
    WidgetProperty threshold{"alarmHigh","High alarm threshold","double",true,-1.0e9,1.0e9,3};
    threshold.defaultValue=90.0;
    return {{"text","Title","string",true},{"value","Default value","double",true},
            decimals,unit,accent,showPlus,grouping,highAlarm,threshold};
}
PanelControl DigitalDisplayWidgetFactory::defaultDefinition(const QString& id) const {
    PanelControl c=WidgetFactory::defaultDefinition(id);
    c.text=QStringLiteral("Voltage");
    c.geometry.setSize(QSize(265,120));
    c.value=0.0;
    return c;
}
QWidget* DigitalDisplayWidgetFactory::create(const PanelControl& definition,
                                              QWidget* parent,EventSink) const {
    auto* widget=new DigitalDisplayWidget(parent);
    applyDefinition(widget,definition);
    return widget;
}
void DigitalDisplayWidgetFactory::applyDefinition(QWidget* widget,const PanelControl& c) const {
    auto* display=dynamic_cast<DigitalDisplayWidget*>(widget);
    if(!display)return;
    const auto& p=c.properties;
    display->configure(c.text,c.value,p.value("decimals").toInt(3),
                       p.value("unit").toString(QStringLiteral("V")),
                       p.value("accent").toString(QStringLiteral("cyan")),
                       p.value("showPlus").toBool(false),
                       p.value("groupThousands").toBool(false),
                       p.value("alarmEnabled").toBool(false),
                       p.value("alarmHigh").toDouble(90.0));
}
void DigitalDisplayWidgetFactory::readDefinition(QWidget*, PanelControl&) const {
    // The saved default value is design metadata, not a snapshot of live data.
    // Do not overwrite the definition with an in-progress runtime measurement.
}
void DigitalDisplayWidgetFactory::applyRuntimeValue(QWidget* widget,const PanelControl&,
                                                      const QVariant& value) const {
    auto* display=dynamic_cast<DigitalDisplayWidget*>(widget);
    if(!display)return;
    bool ok=false;
    const double number=value.toDouble(&ok);
    if(ok && std::isfinite(number))display->setValue(number);
}
