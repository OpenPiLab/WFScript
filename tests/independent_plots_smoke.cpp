#include "plot/PanelChartWidget.h"
#include "plot/PanelPlotFactory.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <QPointer>
#include <memory>

#define CHECK(expr) do { if(!(expr)) {qCritical()<<"FAIL:"<<#expr<<__LINE__;return 1;} } while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PanelPlotFactory factory;
    const PanelControl def=factory.defaultDefinition(QStringLiteral("chartA"));
    CHECK(def.type==QStringLiteral("plot"));
    CHECK(factory.properties().size()>=12);
    QWidget owner;
    auto* a=qobject_cast<PanelChartWidget*>(factory.create(def,&owner,{}));
    auto* b=qobject_cast<PanelChartWidget*>(factory.create(def,&owner,{}));
    CHECK(a && b && a!=b);
    a->resize(520,320);b->resize(520,320);
    QPointer<PanelChartWidget> identity=a;
    PlotCommand command;
    command.plotId=QStringLiteral("chartA");
    command.seriesName=QStringLiteral("Voltage");
    command.x={0,0.001,0.002,0.003};
    command.y={1,2,0,-1};
    factory.deliverPlotCommand(a,command);
    CHECK(a->seriesCount()==1 && a->pointCount(QStringLiteral("Voltage"))==4);
    CHECK(b->seriesCount()==0);
    a->hide();
    command.y={4,3,2,1};
    factory.deliverPlotCommand(a,command);
    CHECK(a==identity && a->pointCount(QStringLiteral("Voltage"))==4);
    PlotCommand append;
    append.action=PlotCommand::Action::AppendSeries;
    append.plotId=QStringLiteral("chartA");append.seriesName=QStringLiteral("Voltage");
    append.x={.004,.005};append.y={.5,.25};
    factory.deliverPlotCommand(a,append);
    CHECK(a->pointCount(QStringLiteral("Voltage"))==6);
    CHECK(b->seriesCount()==0);
    PlotCommand config;
    config.action=PlotCommand::Action::Configure;
    config.options={{"chartType","Scatter"},{"xLabel","Time (s)"},
                    {"xAuto",false},{"xMin",0.0},{"xMax",0.003}};
    factory.deliverPlotCommand(a,config);
    CHECK(a->chartType()==QStringLiteral("Scatter"));
    QImage buffer(QSize(520,320),QImage::Format_ARGB32_Premultiplied);
    buffer.fill(Qt::transparent);
    {QPainter p(&buffer);a->render(&p,QPoint(),QRegion(),QWidget::DrawChildren);}
    CHECK(!buffer.isNull());
    PlotCommand remove;remove.action=PlotCommand::Action::RemoveSeries;
    remove.seriesName=QStringLiteral("Voltage");
    factory.deliverPlotCommand(a,remove);
    CHECK(a->seriesCount()==0 && b->seriesCount()==0);
    factory.deliverPlotCommand(a,command);
    PlotCommand clear;clear.action=PlotCommand::Action::Clear;
    factory.deliverPlotCommand(a,clear);
    CHECK(a->seriesCount()==0);
    PanelControl readback=def;
    factory.readDefinition(a,readback);
    CHECK(readback.properties==def.properties); // never persist captured samples
    qInfo()<<"PASS: independent chart instances, explicit series, hidden updates, axes, clear, paint";
    return 0;
}
