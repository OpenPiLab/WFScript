#include "../src/plot/PanelChartWidget.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#ifdef NDEBUG
#undef NDEBUG // smoke assertions must run in Windows Release too
#endif
#include <cassert>
#include <algorithm>
#include <cmath>
#include <iostream>
int main(int argc,char** argv){
    QApplication app(argc,argv);
    PanelChartWidget chart;
    chart.resize(900,500);
    PlotCommand config;config.action=PlotCommand::Action::Configure;
    config.options={{"chartType","Line"},{"xLabel","ms"},{"yLabel","V"}};
    chart.applyCommand(config);
    int changes=0;
    QObject::connect(&chart,&PanelChartWidget::viewChanged,[&](const QVariantMap& value){
        assert(value.value("xMax").toDouble()>value.value("xMin").toDouble());++changes;
    });
    for(int ch=1;ch<=4;++ch){
        PlotCommand c;c.seriesName=QStringLiteral("CH%1").arg(ch);
        c.nativeSamples=true;c.nativeStart=-10.0;c.nativeStep=0.00002;
        c.options={{"color",QString::fromLatin1(ch==1?"#ffd835":ch==2?"#50ccea":ch==3?"#d55cc7":"#58c77a")}};
        c.y.reserve(1000000);
        for(int i=0;i<1000000;++i)c.y.append(std::sin(i*0.00017+ch)*0.85+ch*0.12);
        if(ch==2)c.y[587631]=2.5;
        chart.applyCommand(c);
        assert(chart.pointCount(c.seriesName)==1000000);
    }
    const auto full=chart.resolvedRanges();
    assert(full.value("xMin").toDouble()<=-10.0);
    QImage pixels(900,500,QImage::Format_ARGB32_Premultiplied);
    QElapsedTimer time;time.start();
    for(int i=0;i<10;++i){
        PlotCommand view;view.action=PlotCommand::Action::SetView;
        view.options={{"xMin",-10.0+i*0.25},{"xMax",9.9-i*0.25},
                      {"yMin",-2.0},{"yMax",3.0}};
        chart.applyCommand(view);
        pixels.fill(Qt::transparent);
        QPainter painter(&pixels);chart.render(&painter);painter.end();
    }
    assert(changes==10);
    // Dense 4x1M plotting must have a strict screen-pixel primitive budget,
    // not an O(total samples) QPainterPath. This checks the *actual* widget.
    const QVariantMap perf=chart.nativeRenderDiagnostics();
    assert(perf.value("lastNativePrimitives").toLongLong()>0);
    // One envelope and at most one mean-trace connector per pixel column.
    assert(perf.value("lastNativePrimitives").toLongLong()<=4LL*2*900);
    const qlonglong hitsBefore=perf.value("cacheHits").toLongLong();
    QPainter secondPainter(&pixels);
    chart.render(&secondPainter);secondPainter.end();
    assert(chart.nativeRenderDiagnostics().value("cacheHits").toLongLong()>=hitsBefore+4);
    const auto range=chart.resolvedRanges();
    assert(range.value("xMin").toDouble()>-10.0);
    // Programmatic visibility must not change an already zoomed viewport.
    PlotCommand hide;hide.action=PlotCommand::Action::StyleSeries;
    hide.seriesName="CH2";hide.options={{"visible",false}};
    const QVariantMap beforeHide=chart.resolvedRanges();
    chart.applyCommand(hide);
    const QVariantMap afterHide=chart.resolvedRanges();
    assert(beforeHide==afterHide);
    // Reproduce the operator's direct legend-click path, which RC1.5 had
    // incorrectly implemented as m_viewActive=false (an unwanted fit reset).
    const double scale=std::clamp(chart.height()/300.0,0.65,1.4);
    const QPointF legendClick(chart.width()-23.0*scale-120.0,42.0*scale+15.0);
    QMouseEvent click(QEvent::MouseButtonPress,legendClick,legendClick,
                      Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&chart,&click);
    assert(!chart.seriesStyle("CH1").value("visible").toBool());
    assert(chart.resolvedRanges()==beforeHide);
    chart.resetView();assert(changes==11);
    std::cout<<"4x1M native chart, 10 zoom/paint cycles, elapsed "<<time.elapsed()<<" ms PASS\n";
    return 0;
}
