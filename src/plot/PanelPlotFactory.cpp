#include "PanelPlotFactory.h"
#include "PanelChartWidget.h"

PanelControl PanelPlotFactory::defaultDefinition(const QString& id) const {
    PanelControl d=WidgetFactory::defaultDefinition(id);
    d.geometry.setSize(QSize(510,300));
    d.text=QStringLiteral("Data Plot");
    return d;
}
QVector<WidgetProperty> PanelPlotFactory::properties() const {
    QVector<WidgetProperty> definitions={
        {"text","Chart title","string",true},
        {"chartType","Chart type","choice",true,-1e9,1e9,3,
            {"Line","XY","Scatter","Step","Bar","Area"},QStringLiteral("Line")},
        {"xLabel","X-axis label","string",true,-1e9,1e9,3,{},QStringLiteral("X")},
        {"yLabel","Y-axis label","string",true,-1e9,1e9,3,{},QStringLiteral("Y")},
        {"xAuto","Automatic X axis","boolean",true,-1e9,1e9,3,{},true},
        {"xMin","X minimum","double",true,-1e12,1e12,12,{},0.0},
        {"xMax","X maximum","double",true,-1e12,1e12,12,{},1.0},
        {"yAuto","Automatic Y axis","boolean",true,-1e9,1e9,3,{},true},
        {"yMin","Y minimum","double",true,-1e12,1e12,12,{},-1.0},
        {"yMax","Y maximum","double",true,-1e12,1e12,12,{},1.0},
        {"showGrid","Show grid","boolean",true,-1e9,1e9,3,{},true},
        {"showXAxis","Show X-axis labels","boolean",true,-1e9,1e9,3,{},true},
        {"showYAxis","Show left Y-axis labels","boolean",true,-1e9,1e9,3,{},true},
        {"showY2Labels","Show right Y-axis labels","boolean",true,-1e9,1e9,3,{},true},
        {"showLegend","Show legend","boolean",true,-1e9,1e9,3,{},true},
        {"lineWidth","Default line thickness","double",true,0.5,8,2,{},1.8},
        {"showY2","Enable right Y axis","boolean",true,-1e9,1e9,3,{},false},
        {"y2Label","Right Y units","string",true,-1e9,1e9,3,{},QStringLiteral("Right Y")},
        {"y2Auto","Automatic right Y","boolean",true,-1e9,1e9,3,{},true},
        {"y2Min","Right Y minimum","double",true,-1e12,1e12,12,{},0.0},
        {"y2Max","Right Y maximum","double",true,-1e12,1e12,12,{},100.0},
        {"xLog","Logarithmic X axis","boolean",true,-1e9,1e9,3,{},false},
        {"yLog","Logarithmic Y axis","boolean",true,-1e9,1e9,3,{},false},
        {"showCursors","Measurement cursors","boolean",true,-1e9,1e9,3,{},false},
        {"cursorA","Cursor A X coordinate","double",true,-1e9,1e9,5,{},0.0},
        {"cursorB","Cursor B X coordinate","double",true,-1e9,1e9,5,{},1.0}
    };
    // Inspector-only groups. Stable property IDs and project JSON stay intact.
    for(auto& property:definitions) {
        const auto& id=property.id;
        if(id==QLatin1String("text") || id==QLatin1String("chartType") ||
           id==QLatin1String("showGrid") || id==QLatin1String("showLegend") ||
           id==QLatin1String("lineWidth"))property.category=QStringLiteral("Chart Appearance");
        else if(id==QLatin1String("showCursors") || id.startsWith(QLatin1String("cursor")))
            property.category=QStringLiteral("Measurements");
        else if(id.startsWith(QLatin1String("y2")) || id==QLatin1String("showY2") ||
                id==QLatin1String("showY2Labels"))property.category=QStringLiteral("Right Y Axis");
        else if(id.startsWith(QLatin1String("x")) || id==QLatin1String("showXAxis"))
            property.category=QStringLiteral("X Axis");
        else if(id.startsWith(QLatin1String("y")) || id==QLatin1String("showYAxis"))
            property.category=QStringLiteral("Left Y Axis");
    }
    return definitions;
}
QWidget* PanelPlotFactory::create(const PanelControl& def,QWidget* parent,EventSink) const {
    auto* chart=new PanelChartWidget(parent);
    chart->applyDefinition(def);
    return chart;
}
void PanelPlotFactory::applyDefinition(QWidget* widget,const PanelControl& def) const {
    if(auto* c=qobject_cast<PanelChartWidget*>(widget))c->applyDefinition(def);
}
void PanelPlotFactory::readDefinition(QWidget*,PanelControl&) const {
    // Runtime data, zoom/pan and script-driven styling are never serialized.
}
void PanelPlotFactory::deliverPlotCommand(QWidget* widget,const PlotCommand& command) const {
    if(auto* c=qobject_cast<PanelChartWidget*>(widget))c->applyCommand(command);
}
