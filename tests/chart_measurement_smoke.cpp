#include "plot/PanelChartWidget.h"
#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>

#define CHECK(expr) do {if(!(expr)){qCritical()<<"FAIL"<<#expr<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv){
    QApplication app(argc,argv);
    PanelChartWidget chart;
    chart.resize(640,420);
    PlotCommand a;
    a.action=PlotCommand::Action::ReplaceSeries;
    a.seriesName="Response";
    a.x={1,10,100,1000};a.y={100,50,10,1};
    a.options={{"color","#47cce4"},{"width",2.5},{"markerSize",3.5}};
    chart.applyCommand(a);
    CHECK(chart.seriesCount()==1);
    CHECK(chart.pointCount("Response")==4);
    CHECK(chart.seriesStyle("Response").value("width").toDouble()==2.5);
    PlotCommand style;
    style.action=PlotCommand::Action::StyleSeries;
    style.seriesName="Response";
    style.options={{"visible",false},{"width",3.25}};
    chart.applyCommand(style);
    CHECK(chart.pointCount("Response")==4);
    CHECK(!chart.seriesStyle("Response").value("visible").toBool());
    style.options={{"visible",true},{"markerSize",4.0}};
    chart.applyCommand(style);
    CHECK(chart.seriesStyle("Response").value("visible").toBool());
    PlotCommand options;
    options.action=PlotCommand::Action::Configure;
    options.options={{"xLog",true},{"yLog",true},{"showCursors",true},
                     {"cursorA",10.0},{"cursorB",100.0}};
    chart.applyCommand(options);
    CHECK(chart.cursorsEnabled());
    for(int mode=0;mode<2;++mode){
        QImage img(chart.size(),QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);chart.render(&p);p.end();
        CHECK(!img.isNull());
        CHECK(img.pixelColor(10,10).alpha()>0);
        if(mode==0){options.options={{"yLog",false}};chart.applyCommand(options);}
    }
    chart.hide();
    CHECK(chart.pointCount("Response")==4);
    qInfo()<<"PASS: styling, hidden series persistence, log axis rendering and cursors";
    return 0;
}
