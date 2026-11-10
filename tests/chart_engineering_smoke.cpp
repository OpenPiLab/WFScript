#include "plot/PanelChartWidget.h"
#include "plot/DigitalTimingPlotFactory.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <cmath>
#define CHECK(expr) do{if(!(expr)){qCritical()<<"FAIL"<<#expr<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv){
    QApplication app(argc,argv);
    DigitalTimingPlotFactory factory;
    PanelControl def=factory.defaultDefinition("digital1");
    CHECK(def.type==QStringLiteral("digital_plot"));
    QWidget owner;
    auto* digital=qobject_cast<PanelChartWidget*>(factory.create(def,&owner,{}));
    CHECK(digital && digital->chartType()==QStringLiteral("Digital"));
    digital->resize(620,350);
    PlotCommand lane;lane.seriesName="SCLK";lane.x={0,1,2,3,4};lane.y={0,1,0,1,0};
    factory.deliverPlotCommand(digital,lane);
    lane.seriesName="MOSI";lane.y={1,1,0,0,1};
    factory.deliverPlotCommand(digital,lane);
    CHECK(digital->seriesCount()==2);
    QImage pixels(digital->size(),QImage::Format_ARGB32_Premultiplied);
    pixels.fill(Qt::transparent);{QPainter p(&pixels);digital->render(&p);}
    CHECK(!pixels.isNull());
    PanelChartWidget chart;
    chart.resize(620,350);
    PlotCommand left;left.seriesName="RMS";left.x={0,1,2};left.y={.7,.72,.71};
    chart.applyCommand(left);
    PlotCommand right;right.seriesName="Temp";right.x={0,1,2};right.y={20,22,25};
    right.options={{"axis","right"}};
    chart.applyCommand(right);
    CHECK(chart.seriesStyle("Temp").value("axis").toString()==QStringLiteral("right"));
    PlotCommand cfg;cfg.action=PlotCommand::Action::Configure;
    cfg.options={{"showY2",true},{"y2Auto",false},{"y2Min",18.0},{"y2Max",27.0}};
    chart.applyCommand(cfg);
    const QVariantMap ranges=chart.resolvedRanges();
    CHECK(ranges.value("yMax").toDouble()<2.0); // right series must not inflate left scale
    chart.hide();chart.applyCommand(left);
    CHECK(chart.pointCount("RMS")==3); // no rebuild when hidden
    QImage combined(chart.size(),QImage::Format_ARGB32_Premultiplied);
    combined.fill(Qt::transparent);{QPainter p(&combined);chart.render(&p);}
    CHECK(!combined.isNull());
    qInfo()<<"PASS digital lanes, right Y assignment, hidden updates, offscreen render";
    return 0;
}
