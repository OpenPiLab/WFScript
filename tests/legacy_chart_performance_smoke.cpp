// RC1.5.2: optional Windows/Qt6 offscreen check. Four 250k-point legacy XY
// channels, repeated zoom, rendering budget and geometry caching.
#include "plot/PanelChartWidget.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <cmath>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
static void paint(PanelChartWidget& chart){
    QImage image(chart.size(),QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);chart.render(&painter);painter.end();
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    PanelChartWidget chart;chart.resize(920,480);
    PlotCommand config;config.action=PlotCommand::Action::Configure;
    config.options={{"chartType","Line"},{"showCursors",false}};
    chart.applyCommand(config);
    for(int ch=0;ch<4;++ch){
        PlotCommand samples;samples.seriesName=QStringLiteral("LegacyCH%1").arg(ch+1);
        samples.x.reserve(250000);samples.y.reserve(250000);
        double x=0;
        for(int i=0;i<250000;++i){
            x+=(i%7==0)?0.00002:0.000015;
            samples.x.append(x);
            samples.y.append(std::sin(i*.0003+ch*.4)+ch*.12);
        }
        chart.applyCommand(samples);
    }
    QElapsedTimer timer;timer.start();
    for(int j=0;j<8;++j){
        PlotCommand view;view.action=PlotCommand::Action::SetView;
        view.options={{"xMin",0.2+j*.1},{"xMax",2.8-j*.1},
                      {"yMin",-1.5},{"yMax",2}};
        chart.applyCommand(view);
        paint(chart);
        const auto diag=chart.legacyRenderDiagnostics();
        assert(diag.value("lastLegacyPrimitives").toLongLong()>0);
        assert(diag.value("lastLegacyPrimitives").toLongLong()<=4LL*2*920);
    }
    const auto before=chart.legacyRenderDiagnostics().value("cacheHits").toLongLong();
    paint(chart);
    assert(chart.legacyRenderDiagnostics().value("cacheHits").toLongLong()>=before+4);
    // Zoom and visibility are independent.
    auto xBefore=chart.resolvedRanges();
    PlotCommand hide;hide.action=PlotCommand::Action::StyleSeries;
    hide.seriesName="LegacyCH2";hide.options={{"visible",false}};
    chart.applyCommand(hide);
    assert(xBefore==chart.resolvedRanges());
    std::cout<<"Qt legacy 4x250k draw, 8 views, "<<timer.elapsed()<<" ms PASS\n";
    return 0;
}
