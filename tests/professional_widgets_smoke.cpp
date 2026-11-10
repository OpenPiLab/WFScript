// Native Qt6 offscreen smoke test for three professional visual instruments.
// Validates metadata, JSON, rendering, hidden Page updates, object identity,
// invalid inputs, and transient peak state without external hardware.
#include "PanelCanvas.h"
#include "widgets/PrecisionDialWidget.h"
#include "widgets/StatusBeaconWidget.h"
#include "widgets/SegmentedBarWidget.h"
#include "widgets/WidgetPropertyInspector.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QImage>
#include <QPointer>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

#define CHECK(cond) do {if(!(cond)){qCritical()<<"FAIL"<<#cond<<"at"<<__LINE__;return 1;}}while(false)
static bool hasPaintedPixels(QWidget* widget) {
    QImage image(widget->size(),QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget->render(&image);
    return image.pixelColor(image.width()/2,image.height()/2).alpha()>0;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    auto& registry=WidgetRegistry::instance();
    CHECK(registry.factory("precision_dial") && registry.factory("status_beacon") &&
          registry.factory("segmented_bar"));
    PanelControl dial=registry.factory("precision_dial")->defaultDefinition("dial");
    PanelControl beacon=registry.factory("status_beacon")->defaultDefinition("lamp");
    PanelControl bar=registry.factory("segmented_bar")->defaultDefinition("bar");
    dial.geometry.moveTo(30,35);beacon.geometry.moveTo(390,45);
    bar.geometry.moveTo(380,210);
    dial.value=49.125;bar.value=34.5;beacon.value=0;
    dial.properties.insert("alarmEnabled",true);
    dial.properties.insert("alarmHigh",80.);
    bar.properties.insert("segments",32);
    bar.properties.insert("peakHold",true);
    beacon.properties.insert("onText","RUNNING");
    WidgetPropertyInspector inspector;
    inspector.setControl(dial,true);
    CHECK(inspector.findChild<QComboBox*>("panelProperty_accent"));
    CHECK(inspector.findChild<QSpinBox*>("panelProperty_decimals"));
    CHECK(inspector.findChild<QCheckBox*>("panelProperty_showTicks"));
    PanelCanvas canvas;
    CHECK(canvas.addControl(dial) && canvas.addControl(beacon) && canvas.addControl(bar));
    auto* liveDial=canvas.findChild<PrecisionDialWidget*>("dial");
    auto* liveLamp=canvas.findChild<StatusBeaconWidget*>("lamp");
    auto* liveBar=canvas.findChild<SegmentedBarWidget*>("bar");
    CHECK(liveDial && liveLamp && liveBar);
    CHECK(liveDial->value()==dial.value && !liveDial->alarmActive());
    CHECK(!liveLamp->active() && liveBar->value()==bar.value);
    const QJsonArray original=canvas.toJson();
    CHECK(original.size()==3);
    QVector<PanelControl> decoded;
    QString error;
    CHECK(WidgetDocumentCodec::decodeList(original,&decoded,&error));
    CHECK(decoded.size()==3);
    CHECK(decoded[0].properties.value("alarmHigh").toDouble()==80.0);
    CHECK(hasPaintedPixels(liveDial) && hasPaintedPixels(liveLamp) && hasPaintedPixels(liveBar));
    // Test both small and large responsive sizes while preserving values.
    liveDial->resize(175,145);liveLamp->resize(180,65);liveBar->resize(210,85);
    CHECK(hasPaintedPixels(liveDial) && hasPaintedPixels(liveLamp) && hasPaintedPixels(liveBar));
    liveDial->setGeometry(dial.geometry);liveLamp->setGeometry(beacon.geometry);
    liveBar->setGeometry(bar.geometry);
    CHECK(canvas.activatePage("main",original,QSize(900,600),&error));
    QPointer<PrecisionDialWidget> dialPtr=liveDial;
    QPointer<StatusBeaconWidget> lampPtr=liveLamp;
    QPointer<SegmentedBarWidget> barPtr=liveBar;
    CHECK(canvas.activatePage("config",QJsonArray{},QSize(900,600),&error));
    CHECK(dialPtr && lampPtr && barPtr);
    CHECK(dialPtr->isHidden() && lampPtr->isHidden() && barPtr->isHidden());
    canvas.applyValue("dial",92.5);
    canvas.applyValue("lamp",true);
    canvas.applyValue("bar",79.25);
    CHECK(dialPtr->alarmActive() && dialPtr->value()==92.5);
    CHECK(lampPtr->active());
    CHECK(barPtr->value()==79.25 && barPtr->peakValue()==79.25);
    canvas.applyValue("bar",19.0);
    CHECK(barPtr->value()==19.0 && barPtr->peakValue()==79.25);
    canvas.applyValue("dial",QStringLiteral("n/a"));
    CHECK(dialPtr->value()==92.5);
    CHECK(canvas.activatePage("main",original,QSize(900,600),&error));
    CHECK(dialPtr==canvas.findChild<PrecisionDialWidget*>("dial"));
    CHECK(lampPtr==canvas.findChild<StatusBeaconWidget*>("lamp"));
    CHECK(barPtr==canvas.findChild<SegmentedBarWidget*>("bar"));
    CHECK(barPtr->peakValue()==79.25);
    CHECK(canvas.toJson()==original);
    canvas.restoreDocumentValues(original);
    CHECK(dialPtr->value()==49.125 && !dialPtr->alarmActive());
    CHECK(!lampPtr->active() && barPtr->peakValue()==bar.value);
    CHECK(canvas.toJson()==original);
    // Factory metadata type/range validation rejects malformed widget config.
    QJsonObject invalid=original[2].toObject();
    QJsonObject properties=invalid.value("properties").toObject();
    properties["segments"]=99;invalid["properties"]=properties;
    PanelControl unused;
    CHECK(!WidgetDocumentCodec::decode(invalid,&unused,&error));
    PanelCanvas reopened;
    CHECK(reopened.fromJson(original,&error));
    CHECK(reopened.findChild<PrecisionDialWidget*>("dial"));
    CHECK(reopened.findChild<StatusBeaconWidget*>("lamp"));
    CHECK(reopened.findChild<SegmentedBarWidget*>("bar"));
    qInfo()<<"PASS: v5.6.32 professional instruments, factory, metadata, persistence, runtime and paint";
    return 0;
}
