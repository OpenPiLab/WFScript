// RC1.5.3: offscreen Qt regression, Digital lane 4x250k, cosmetics retain view,
// screen-bounded painter submissions, and geometry-cache reuse.
#include "plot/PanelChartWidget.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QWheelEvent>
#include <QEventLoop>
#include <QTimer>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
static void draw(PanelChartWidget& chart){
    QImage pixels(chart.size(),QImage::Format_ARGB32_Premultiplied);
    pixels.fill(Qt::transparent);QPainter painter(&pixels);chart.render(&painter);
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    PanelChartWidget d;d.resize(1000,500);
    PlotCommand initial;initial.action=PlotCommand::Action::Configure;
    initial.options={{"chartType","Digital"}};d.applyCommand(initial);
    for(int lane=0;lane<4;++lane){
        PlotCommand c;c.seriesName=QStringLiteral("D%1").arg(lane);
        c.x.reserve(250000);c.y.reserve(250000);
        for(int i=0;i<250000;++i){
            c.x.append(double(i)*1.e-6);
            c.y.append(double((i/(4+lane*2))%2));
        }
        d.applyCommand(c);
    }
    const auto full=d.resolvedRanges();
    assert(full.value("xMax").toDouble()>=.24);
    PlotCommand view;view.action=PlotCommand::Action::SetView;
    view.options={{"xMin",.10},{"xMax",.20},{"yMin",0},{"yMax",1}};
    d.applyCommand(view);draw(d);
    const auto before=d.resolvedRanges();
    assert(before.value("xMin").toDouble()==.10);
    assert(d.digitalRenderDiagnostics().value("lastDigitalPrimitives").toLongLong()>0);
    assert(d.digitalRenderDiagnostics().value("lastDigitalPrimitives").toLongLong()<=4LL*1000);
    const auto hits=d.digitalRenderDiagnostics().value("cacheHits").toLongLong();
    draw(d);
    assert(d.digitalRenderDiagnostics().value("cacheHits").toLongLong()>=hits+4);
    // RC1.5.4: zero geometry rebuilding during rapid mouse-wheel input.
    // The cached transparent waveform layer is transformed during preview,
    // while axes/grid/cursors remain tied to the current viewport.
    const auto pre=d.digitalRenderDiagnostics();
    const qlonglong oldBuilds=pre.value("geometryBuilds").toLongLong();
    assert(pre.value("rasterBuilds").toLongLong()>=4);
    for(int pass=0;pass<5;++pass){
        const QPointF pos(470,255);
        QWheelEvent wheel(pos,pos,QPoint(),QPoint(0,120),Qt::NoButton,
                          Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(&d,&wheel);
        draw(d);
    }
    const auto interaction=d.digitalRenderDiagnostics();
    assert(interaction.value("previewBlits").toLongLong()>=20);
    assert(interaction.value("geometryBuilds").toLongLong()==oldBuilds);
    assert(interaction.value("lastFrameMs").toDouble()>=0);
    // Idle timer reconstructs accurate edge geometry instead of retaining
    // a scaled-up preview or losing subpixel glitch information.
    QEventLoop waitForIdle;
    QTimer::singleShot(115,&waitForIdle,&QEventLoop::quit);
    waitForIdle.exec();draw(d);
    assert(d.digitalRenderDiagnostics().value("geometryBuilds").toLongLong()>=oldBuilds+4);
    // A stroke style change must invalidate the colorized raster.
    const auto beforeStyle=d.digitalRenderDiagnostics().value("rasterBuilds").toLongLong();
    PlotCommand recolor;recolor.action=PlotCommand::Action::StyleSeries;
    recolor.seriesName="D0";recolor.options={{"color","#123456"}};
    d.applyCommand(recolor);draw(d);
    assert(d.digitalRenderDiagnostics().value("rasterBuilds").toLongLong()>beforeStyle);
    // The diagnostic HUD is opt-in, and toggling it preserves the viewport.
    PlotCommand hud;hud.action=PlotCommand::Action::Configure;
    hud.options={{"digitalPerfOverlay",true}};
    const auto currentView=d.resolvedRanges();
    d.applyCommand(hud);draw(d);
    assert(d.resolvedRanges()==currentView);
    assert(d.digitalRenderDiagnostics().contains("lastPaintMs"));
    PlotCommand cosmetic;cosmetic.action=PlotCommand::Action::Configure;
    cosmetic.options={{"showGrid",false},{"title","No reset"}};
    d.applyCommand(cosmetic);
    assert(d.resolvedRanges()==currentView);
    // Digital factory adds chartType/xLog to Configure; unchanged values
    // must not release an active view.
    cosmetic.options={{"chartType","Digital"},{"xLog",false},{"showGrid",true}};
    d.applyCommand(cosmetic);
    assert(d.resolvedRanges()==currentView);
    PlotCommand hide;hide.action=PlotCommand::Action::StyleSeries;
    hide.seriesName="D1";hide.options={{"visible",false}};
    d.applyCommand(hide);
    assert(d.resolvedRanges()==currentView);
    PlotCommand axis;axis.action=PlotCommand::Action::Configure;
    axis.options={{"xAuto",false},{"xMin",0.0},{"xMax",0.25}};
    d.applyCommand(axis);
    assert(d.resolvedRanges().value("xMin").toDouble()==0.0);
    // Switching a populated ordinary plot to Digital lazily builds the edge
    // index; analog charts do not pay this construction cost by default.
    PanelChartWidget dynamic;dynamic.resize(620,360);
    PlotCommand raw;raw.seriesName="logic";raw.x={0,1,2,3,4};
    raw.y={0,0,1,1,0};dynamic.applyCommand(raw);
    PlotCommand digital;digital.action=PlotCommand::Action::Configure;
    digital.options={{"chartType","Digital"}};dynamic.applyCommand(digital);
    draw(dynamic);
    assert(dynamic.digitalRenderDiagnostics().value("lastDigitalPrimitives").toLongLong()>0);
    // Cosmetic envelope changes are also view-preserving for ordinary and
    // native analog chart modes, not just the digital factory.
    PanelChartWidget analog;analog.resize(620,360);
    PlotCommand wave;wave.seriesName="legacy";wave.x={0,1,2,3};
    wave.y={-1,1,-1,1};analog.applyCommand(wave);
    view.options={{"xMin",.5},{"xMax",2.5},{"yMin",-2.},{"yMax",2.}};
    analog.applyCommand(view);
    const auto analogView=analog.resolvedRanges();
    cosmetic.options={{"nativeEnvelopeOpacity",0.1},
                      {"legacyEnvelopeOpacity",0.7},{"showGrid",false}};
    analog.applyCommand(cosmetic);
    assert(analog.resolvedRanges()==analogView);
    std::cout<<"Qt Digital 4x250k, cache, visual configure, manual axis PASS\n";
}
