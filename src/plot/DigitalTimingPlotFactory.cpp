#include "DigitalTimingPlotFactory.h"
#include "PanelChartWidget.h"
#include "PanelPlotFactory.h"

PanelControl DigitalTimingPlotFactory::defaultDefinition(const QString& id) const {
    PanelControl d=WidgetFactory::defaultDefinition(id);
    d.geometry.setSize(QSize(560,320));
    d.text=QStringLiteral("Digital Timing");
    d.properties.insert(QStringLiteral("chartType"),QStringLiteral("Digital"));
    d.properties.insert(QStringLiteral("xLabel"),QStringLiteral("Time (s)"));
    d.properties.insert(QStringLiteral("showLegend"),false);
    return d;
}
QVector<WidgetProperty> DigitalTimingPlotFactory::properties() const {
    QVector<WidgetProperty> props=PanelPlotFactory().properties();
    for(int i=props.size()-1;i>=0;--i){
        if(props.at(i).id==QStringLiteral("chartType") ||
           props.at(i).id.startsWith(QStringLiteral("y")) ||
           props.at(i).id==QStringLiteral("showY2") ||
           props.at(i).id==QStringLiteral("showLegend") ||
           props.at(i).id==QStringLiteral("xLog"))props.removeAt(i);
    }
    return props;
}
QVector<PanelEventDefinition> DigitalTimingPlotFactory::events() const {
    // Same viewport payload and visual event binding as an ordinary Data Plot.
    return PanelPlotFactory().events();
}
QWidget* DigitalTimingPlotFactory::create(const PanelControl& d,QWidget* parent,EventSink onUserEvent) const {
    auto* w=new PanelChartWidget(parent);
    applyDefinition(w,d);
    QObject::connect(w,&PanelChartWidget::viewChanged,w,
        [onUserEvent](const QVariantMap& view){
            if(onUserEvent)onUserEvent(QStringLiteral("viewChanged"),view);
        });
    return w;
}
void DigitalTimingPlotFactory::applyDefinition(QWidget* widget,const PanelControl& def) const {
    auto* chart=qobject_cast<PanelChartWidget*>(widget);
    if(!chart)return;
    PanelControl d=def;
    d.properties.insert(QStringLiteral("chartType"),QStringLiteral("Digital"));
    d.properties.insert(QStringLiteral("xLog"),false);
    d.properties.insert(QStringLiteral("showLegend"),false);
    chart->applyDefinition(d);
}
void DigitalTimingPlotFactory::deliverPlotCommand(QWidget* widget,const PlotCommand& command) const {
    auto* c=qobject_cast<PanelChartWidget*>(widget);
    if(!c)return;
    PlotCommand cmd=command;
    if(cmd.action==PlotCommand::Action::Configure){
        cmd.options.insert(QStringLiteral("chartType"),QStringLiteral("Digital"));
        cmd.options.insert(QStringLiteral("xLog"),false);
    }
    c->applyCommand(cmd);
}
