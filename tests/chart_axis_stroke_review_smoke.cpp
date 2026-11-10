#include "plot/PanelChartWidget.h"
#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <cmath>
#define CHECK(e) do {if(!(e)){qCritical()<<"FAIL"<<#e<<__LINE__;return 1;}}while(false)
static bool near(double a,double b) {return std::abs(a-b)<1e-9*std::max(1.0,std::abs(b));}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PanelChartWidget chart;
    chart.resize(760,480);
    PlotCommand series;
    series.seriesName="Voltage";
    series.x={1,10,100,1000};series.y={1,2,4,8};
    chart.applyCommand(series);
    auto configure=[&](const QVariantMap& props) {
        PlotCommand cmd;cmd.action=PlotCommand::Action::Configure;cmd.options=props;
        chart.applyCommand(cmd);
    };
    configure({{"xLog",true},{"xAuto",false},{"xMin",10.0},{"xMax",1000.0},
               {"yLog",true},{"yAuto",false},{"yMin",1.0},{"yMax",10.0},
               {"lineWidth",6.0}});
    auto bounds=chart.resolvedRanges();
    CHECK(near(bounds.value("xMin").toDouble(),10));
    CHECK(near(bounds.value("xMax").toDouble(),1000));
    CHECK(near(bounds.value("yMin").toDouble(),1));
    CHECK(near(bounds.value("yMax").toDouble(),10));
    CHECK(near(chart.effectiveSeriesWidth("Voltage"),6.0));
    // Logarithms of zero are undefined; an old linear default of zero must
    // not silently override the user's valid maximum or create NaN labels.
    configure({{"xMin",0.0},{"xMax",1000.0},{"yMin",0.0},{"yMax",10.0}});
    bounds=chart.resolvedRanges();
    CHECK(near(bounds.value("xMin").toDouble(),1));
    CHECK(near(bounds.value("xMax").toDouble(),1000));
    CHECK(near(bounds.value("yMin").toDouble(),0.01));
    CHECK(near(bounds.value("yMax").toDouble(),10));
    // A per-series width explicitly overrides the chart-wide default.
    PlotCommand style;style.action=PlotCommand::Action::StyleSeries;
    style.seriesName="Voltage";style.options={{"width",2.5}};
    chart.applyCommand(style);
    CHECK(near(chart.effectiveSeriesWidth("Voltage"),2.5));
    configure({{"lineWidth",7.0}});
    CHECK(near(chart.effectiveSeriesWidth("Voltage"),2.5));
    // width=0 means use the chart-wide lineWidth again.
    style.options={{"width",0.0}};chart.applyCommand(style);
    CHECK(near(chart.effectiveSeriesWidth("Voltage"),7.0));
    // Manual microsecond axes must retain their native decimal values.
    configure({{"xLog",false},{"xMin",0.000001},{"xMax",0.00001}});
    bounds=chart.resolvedRanges();
    CHECK(near(bounds.value("xMin").toDouble(),0.000001));
    CHECK(near(bounds.value("xMax").toDouble(),0.00001));
    QImage img(chart.size(),QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);QPainter painter(&img);chart.render(&painter);painter.end();
    CHECK(!img.isNull());
    qInfo()<<"PASS chart manual log ranges, tiny bounds, width inheritance, render";
    return 0;
}
