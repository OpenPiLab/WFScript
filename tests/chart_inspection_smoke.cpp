#include "plot/PanelChartWidget.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <cmath>
#define CHECK(e) do {if(!(e)){qCritical()<<"FAIL"<<#e<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PanelChartWidget analog;analog.resize(640,380);
    PlotCommand s;s.seriesName="Voltage";s.x={1,2,3};s.y={10,20,30};
    analog.applyCommand(s);
    PlotCommand c;c.action=PlotCommand::Action::Configure;
    c.options={{"showCursors",true},{"cursorA",1.5},{"cursorB",2.5}};
    analog.applyCommand(c);
    QVariantMap m=analog.cursorMeasurements();
    CHECK(m.value("hasData").toBool());
    CHECK(std::abs(m.value("deltaX").toDouble()-1.0)<1e-9);
    QVariantMap row=m.value("series").toList().first().toMap();
    CHECK(std::abs(row.value("a").toDouble()-15.0)<1e-9);
    CHECK(std::abs(row.value("b").toDouble()-25.0)<1e-9);
    CHECK(std::abs(row.value("deltaY").toDouble()-10.0)<1e-9);
    c.options={{"cursorA",-1.0},{"cursorB",100.0}};analog.applyCommand(c);
    m=analog.cursorMeasurements();
    CHECK(!m.contains("cursorA") && !m.contains("cursorB"));
    CHECK(!m.value("series").toList().first().toMap().contains("a"));
    CHECK(analog.measurementPaneRect().isValid());
    QImage pix(analog.size(),QImage::Format_ARGB32_Premultiplied);pix.fill(Qt::transparent);
    {QPainter p(&pix);analog.render(&p);}CHECK(!pix.isNull());
    PanelChartWidget digital;digital.resize(640,380);
    PanelControl def;def.type="digital_plot";
    def.properties.insert("chartType",QJsonValue("Digital"));
    digital.applyDefinition(def);
    s.seriesName="CLK";s.x={0,1,2,3,4,5,6};s.y={0,1,1,0,1,1,0};
    digital.applyCommand(s);
    c.options={{"showCursors",true},{"cursorA",1.5},{"cursorB",4.5}};
    digital.applyCommand(c);
    m=digital.cursorMeasurements();row=m.value("series").toList().first().toMap();
    CHECK(std::abs(row.value("highWidth").toDouble()-2.0)<1e-9);
    CHECK(std::abs(row.value("period").toDouble()-3.0)<1e-9);
    CHECK(std::abs(row.value("frequency").toDouble()-(1.0/3.0))<1e-9);
    CHECK(std::abs(row.value("dutyPercent").toDouble()-66.6666667)<1e-5);
    CHECK(row.value("a").toDouble()==1.0 && row.value("b").toDouble()==1.0);
    pix=QImage(digital.size(),QImage::Format_ARGB32_Premultiplied);pix.fill(Qt::transparent);
    {QPainter p(&pix);digital.render(&p);}CHECK(!pix.isNull());
    qInfo()<<"PASS cursor interpolation, no extrapolation, digital edges and overlay";
    return 0;
}
