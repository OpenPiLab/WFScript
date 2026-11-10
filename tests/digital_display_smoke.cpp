// v5.6.31: professional Digital Display, factory defaults, metadata,
// stable JSON, runtime-only values, offscreen painting and persistent Pages.
#include "PanelCanvas.h"
#include "widgets/DigitalDisplayWidget.h"
#include "widgets/WidgetPropertyInspector.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QImage>
#include <QLineEdit>
#include <QPointer>
#include <QSpinBox>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

#define CHECK(condition) do {if(!(condition)){qCritical()<<"FAIL"<<#condition<<"line"<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const auto* factory=WidgetRegistry::instance().factory("digital_display");
    CHECK(factory);
    CHECK(factory->displayName()=="Digital Display");
    PanelControl def=factory->defaultDefinition("meter1");
    CHECK(def.type=="digital_display" && def.geometry.size()==QSize(265,120));
    WidgetPropertyInspector inspector;
    inspector.setControl(def,true);
    auto* decimals=inspector.findChild<QSpinBox*>("panelProperty_decimals");
    auto* unit=inspector.findChild<QLineEdit*>("panelProperty_unit");
    auto* accent=inspector.findChild<QComboBox*>("panelProperty_accent");
    auto* alarm=inspector.findChild<QCheckBox*>("panelProperty_alarmEnabled");
    auto* threshold=inspector.findChild<QDoubleSpinBox*>("panelProperty_alarmHigh");
    CHECK(decimals && unit && accent && alarm && threshold);
    CHECK(decimals->value()==3 && unit->text()=="V" && accent->currentText()=="cyan");
    decimals->setValue(2);unit->setText("Hz");accent->setCurrentText("green");
    alarm->setChecked(true);threshold->setValue(75.5);
    def=inspector.readControl(def);
    CHECK(def.properties.value("unit").toString()=="Hz");
    CHECK(def.properties.value("alarmEnabled").toBool());
    // Avoid an IEEE-754 halfway rounding tie (71.235 is not exactly binary).
    // 71.236 unambiguously rounds to 71.24 at two decimal places.
    def.value=71.236;
    PanelCanvas canvas;
    CHECK(canvas.addControl(def));
    auto* live=canvas.findChild<DigitalDisplayWidget*>("meter1");
    CHECK(live);
    CHECK(live->formattedValue()=="71.24");
    CHECK(!live->alarmActive());
    const QJsonArray original=canvas.toJson();
    CHECK(original.size()==1);
    CHECK(original.first().toObject().value("properties").toObject()==def.properties);
    QString error;
    QVector<PanelControl> parsed;
    CHECK(WidgetDocumentCodec::decodeList(original,&parsed,&error));
    CHECK(parsed.size()==1 && parsed.first().value==def.value);

    // Runtime values change the same object without modifying saved defaults.
    canvas.applyValue("meter1",80.25);
    CHECK(std::abs(live->value()-80.25)<1e-12 && live->alarmActive());
    canvas.applyValue("meter1",QStringLiteral("invalid"));
    CHECK(std::abs(live->value()-80.25)<1e-12);
    CHECK(canvas.activatePage("main",original,QSize(900,550),&error));
    QPointer<DigitalDisplayWidget> identity=live;
    CHECK(canvas.activatePage("other",QJsonArray{},QSize(900,550),&error));
    CHECK(identity && identity->isHidden());
    canvas.applyValue("meter1",73.5);
    CHECK(identity->formattedValue()=="73.50" && !identity->alarmActive());
    CHECK(canvas.activatePage("main",original,QSize(900,550),&error));
    CHECK(identity && identity==canvas.findChild<DigitalDisplayWidget*>("meter1"));
    CHECK(identity->formattedValue()=="73.50");
    canvas.restoreDocumentValues(original);
    CHECK(std::abs(identity->value()-71.236)<1e-12);
    CHECK(canvas.toJson()==original);

    // A malformed setting must not be silently accepted on load/edit.
    QJsonObject invalid=original.first().toObject();
    auto props=invalid.value("properties").toObject();
    props["decimals"]=8; invalid["properties"]=props;
    PanelControl dummy;
    CHECK(!WidgetDocumentCodec::decode(invalid,&dummy,&error));

    // Reopening a saved panel restores the factory settings and rendered text.
    PanelCanvas reopened;
    CHECK(reopened.fromJson(original,&error));
    auto* copy=reopened.findChild<DigitalDisplayWidget*>("meter1");
    CHECK(copy && copy->formattedValue()=="71.24" && !copy->alarmActive());
    QImage image(265,120,QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    copy->render(&image);
    CHECK(image.pixelColor(120,60).alpha()>0);
    CHECK(image.pixelColor(120,60)!=QColor(Qt::transparent));
    qInfo()<<"PASS: digital display factory, metadata, JSON, runtime, Page identity, paint";
    return 0;
}
