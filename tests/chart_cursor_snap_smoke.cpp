#include "plot/PanelChartWidget.h"
#include <QApplication>
#include <QMouseEvent>
#include <QDebug>
#include <cmath>
#define CHECK(e) do {if(!(e)){qCritical()<<"FAIL"<<#e<<__LINE__;return 1;}}while(false)
static void mouse(PanelChartWidget& w,QEvent::Type type,QPointF at,Qt::KeyboardModifiers mods=Qt::NoModifier){
    const auto button=type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton;
    const auto buttons=type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton;
    // Qt 6.9: use the non-deprecated local/scene/global-position overload.
    QMouseEvent event(type,at,at,at,button,buttons,mods);
    QApplication::sendEvent(&w,&event);
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    PanelChartWidget w;w.resize(640,380);
    PlotCommand series;series.seriesName="CH1";series.x={0,1,3,10};series.y={0,10,30,100};
    w.applyCommand(series);
    PlotCommand cfg;cfg.action=PlotCommand::Action::Configure;
    cfg.options={{"showCursors",true},{"cursorA",0.0},{"cursorB",1.0}};
    w.applyCommand(cfg);
    const auto r=w.resolvedRanges();
    const double left=65*(380.0/300.0),right=640-23*(380.0/300.0);
    const auto pos=[&](double x){return QPointF(left+(x-r.value("xMin").toDouble())/(r.value("xMax").toDouble()-r.value("xMin").toDouble())*(right-left),200);};
    mouse(w,QEvent::MouseMove,pos(2.4));
    CHECK(w.cursorMeasurements().value("cursorA").toDouble()==0.0);
    mouse(w,QEvent::MouseMove,pos(2.4),Qt::ShiftModifier);
    CHECK(w.cursorMeasurements().value("cursorA").toDouble()==3.0);
    mouse(w,QEvent::MouseMove,pos(9.4),Qt::ControlModifier);
    CHECK(w.cursorMeasurements().value("cursorB").toDouble()==10.0);
    CHECK(w.cursorMeasurements().value("cursorA").toDouble()==3.0);
    mouse(w,QEvent::MouseMove,pos(0.1));
    CHECK(w.cursorMeasurements().value("cursorA").toDouble()==3.0);
    const QRectF pane=w.measurementPaneRect();
    mouse(w,QEvent::MouseButtonPress,QPointF(pane.right()-16,pane.top()+12));
    CHECK(!w.measurementPaneVisible());
    CHECK(!w.cursorMeasurements().contains("cursorA"));
    CHECK(!w.cursorMeasurements().contains("cursorB"));
    mouse(w,QEvent::MouseMove,pos(1.1));
    CHECK(!w.measurementPaneVisible());
    mouse(w,QEvent::MouseMove,pos(1.1),Qt::ControlModifier);
    CHECK(w.measurementPaneVisible());
    CHECK(!w.cursorMeasurements().contains("cursorA"));
    CHECK(w.cursorMeasurements().value("cursorB").toDouble()==1.0);
    // Even on an unsorted XY trace we choose original sample coordinates.
    PanelChartWidget xy;xy.resize(640,380);
    series.seriesName="XY";series.x={3,0,10,1};series.y={30,0,100,10};
    xy.applyCommand(series);xy.applyCommand(cfg);
    const auto limits=xy.resolvedRanges();
    const double px=left+(2.4-limits.value("xMin").toDouble())/(limits.value("xMax").toDouble()-limits.value("xMin").toDouble())*(right-left);
    mouse(xy,QEvent::MouseMove,QPointF(px,200),Qt::ShiftModifier);
    CHECK(xy.cursorMeasurements().value("cursorA").toDouble()==3.0);
    qInfo()<<"PASS modifier-gated snap, independent cursors, close/reopen and XY";
    return 0;
}
