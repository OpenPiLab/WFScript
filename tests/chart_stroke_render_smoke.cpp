// v5.6.35.1: ensure open chart lines never fill the region behind them.
// Run with QT_QPA_PLATFORM=offscreen.
#include "plot/PanelChartWidget.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <cmath>

#define CHECK(x) do { if (!(x)) { qCritical() << "FAIL" << #x << __LINE__; return 1; } } while(false)

static QImage snapshot(PanelChartWidget& w) {
    QImage image(w.size(),QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    w.render(&p,QPoint(),QRegion(),QWidget::DrawChildren);
    return image;
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PanelChartWidget chart;
    chart.resize(520,320);
    PlotCommand config;
    config.action=PlotCommand::Action::Configure;
    config.options={{"chartType","Line"},{"showGrid",false},{"showLegend",false},
                    {"xAuto",false},{"yAuto",false},
                    {"xMin",0.0},{"xMax",1.0},{"yMin",0.0},{"yMax",1.0}};
    chart.applyCommand(config);
    const QImage empty=snapshot(chart);
    PlotCommand series;
    series.action=PlotCommand::Action::ReplaceSeries;
    series.seriesName=QStringLiteral("Triangle");
    series.x={0.2,0.5,0.8};
    series.y={0.2,0.8,0.2};
    chart.applyCommand(series);
    const QImage line=snapshot(chart);
    // A pixel inside the triangle but away from the strokes and the centered
    // 'No data' placeholder. A filled open QPainterPath covers this point.
    const double scale=320.0/300.0;
    const QRectF plot=QRectF(0,0,520,320).adjusted(65*scale,42*scale,-23*scale,-52*scale);
    const QPoint inside(qRound(plot.left()+plot.width()*0.42),
                        qRound(plot.bottom()-plot.height()*0.35));
    CHECK(empty.pixel(inside)==line.pixel(inside));
    // The stroke must still render at a point along the left sloping edge.
    const QPoint onLine(qRound(plot.left()+plot.width()*0.35),
                        qRound(plot.bottom()-plot.height()*0.50));
    CHECK(empty.pixel(onLine)!=line.pixel(onLine));
    config.options.insert("chartType","Area");
    chart.applyCommand(config);
    const QImage area=snapshot(chart);
    CHECK(area.pixel(inside)!=line.pixel(inside)); // translucent Area fill preserved
    qInfo() << "PASS: Line has no implicit path fill, stroke remains, Area still fills";
    return 0;
}
