// v5.6.33 native Qt smoke: 6 registered types, metadata, native events,
// JSON round-trip, hidden Page updates and non-emitting ui.set() paths.
#include "PanelCanvas.h"
#include "widgets/InstrumentButtonWidget.h"
#include "widgets/RotaryInputWidget.h"
#include "widgets/NumericStepperWidget.h"
#include "widgets/OptionSelectorWidget.h"
#include "widgets/StatusBannerWidget.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPointer>
#include <QDoubleSpinBox>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

#define CHECK(x) do{if(!(x)){qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1;}}while(false)
static void click(QWidget* widget) {
    const QPointF center(widget->width()/2.,widget->height()/2.);
    const QPointF global=QPointF(widget->mapToGlobal(center.toPoint()));
    QMouseEvent down(QEvent::MouseButtonPress,center,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&down);
    QMouseEvent up(QEvent::MouseButtonRelease,center,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&up);
}
static bool painted(QWidget* widget) {
    const QSize s=widget->size();
    QImage image(s,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    widget->render(&image);
    return image.pixelColor(s.width()/2,s.height()/2).alpha()>0;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    auto& reg=WidgetRegistry::instance();
    const QStringList types={"momentary_button","switch_button","rotary_input",
        "numeric_stepper","option_selector","status_banner"};
    for(const auto& type:types)CHECK(reg.factory(type));
    PanelCanvas canvas;
    QVector<PanelControl> defs;
    const QStringList names={"momentary","latch","knob","number","mode","banner"};
    for(int i=0;i<types.size();++i) {
        auto d=reg.factory(types[i])->defaultDefinition(names[i]);
        d.geometry.moveTo(20+(i%3)*360,20+(i/3)*340);
        CHECK(canvas.addControl(d));defs.append(d);
    }
    auto* momentary=canvas.findChild<InstrumentButtonWidget*>("momentary");
    auto* latch=canvas.findChild<InstrumentButtonWidget*>("latch");
    auto* knob=canvas.findChild<RotaryInputWidget*>("knob");
    auto* number=canvas.findChild<NumericStepperWidget*>("number");
    auto* combo=canvas.findChild<OptionSelectorWidget*>("mode");
    auto* banner=canvas.findChild<StatusBannerWidget*>("banner");
    CHECK(momentary && latch && knob && number && combo && banner);
    CHECK(!momentary->isLatching() && latch->isLatching());
    CHECK(number->singleStep()==.1 && combo->options().size()==4);
    for(const auto& d:defs) {
        QString err;PanelControl copy;
        CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(d),&copy,&err));
        CHECK(copy.type==d.type && copy.properties==d.properties);
    }
    CHECK(painted(knob) && painted(banner) && painted(momentary));
    knob->resize(140,150);number->resize(175,83);combo->resize(180,80);
    CHECK(painted(knob) && painted(number) && painted(combo));
    knob->setGeometry(defs[2].geometry);
    number->setGeometry(defs[3].geometry);
    combo->setGeometry(defs[4].geometry);

    const QVariantMap initial=canvas.initialValues();
    CHECK(initial.value("mode").toString()=="Automatic");
    CHECK(initial.value("latch").metaType().id()==QMetaType::Bool);
    CHECK(initial.value("momentary").metaType().id()==QMetaType::Bool);
    const QJsonArray saved=canvas.toJson();
    QString error;
    CHECK(canvas.activatePage("inputs",saved,QSize(1200,760),&error));
    QPointer<InstrumentButtonWidget> savedMomentary=momentary;
    QPointer<RotaryInputWidget> savedKnob=knob;
    CHECK(canvas.activatePage("other",QJsonArray{},QSize(1200,760),&error));
    CHECK(savedMomentary && savedKnob && savedMomentary->isHidden());
    canvas.applyValue("knob",34.25);
    canvas.applyValue("number",22.4);
    canvas.applyValue("mode",QStringLiteral("Calibration"));
    canvas.applyValue("banner",2);
    canvas.applyValue("latch",true);
    CHECK(std::abs(knob->value()-34.25)<.0001);
    CHECK(std::abs(number->value()-22.4)<.0001);
    CHECK(combo->currentText()=="Calibration");
    CHECK(banner->level()==2 && latch->state());
    CHECK(canvas.activatePage("inputs",saved,QSize(1200,760),&error));
    CHECK(momentary==savedMomentary && knob==savedKnob);
    CHECK(canvas.toJson()==saved); // runtime values never persisted
    canvas.restoreDocumentValues(saved);
    CHECK(combo->currentText()=="Automatic" && banner->level()==0 && !latch->state());

    canvas.setDesignMode(false);
    int pressed=0,released=0,toggled=0,changed=0;
    QObject::connect(momentary,&InstrumentButtonWidget::userPressed,[&]{++pressed;});
    QObject::connect(momentary,&InstrumentButtonWidget::userReleased,[&]{++released;});
    QObject::connect(latch,&InstrumentButtonWidget::userToggled,[&](bool){++toggled;});
    QObject::connect(number,&NumericStepperWidget::userValueChanged,[&](double){++changed;});
    click(momentary);
    CHECK(pressed==1 && released==1 && !momentary->state());
    click(latch);
    CHECK(toggled==1 && latch->state());
    latch->setState(false);CHECK(toggled==1); // JS update must not echo
    number->setValue(13.3);CHECK(changed==0); // programmatic update
    auto* spin=number->findChild<QDoubleSpinBox*>();CHECK(spin);
    spin->stepUp();CHECK(changed==1);
    QJsonObject bad=WidgetDocumentCodec::encode(defs[3]);
    QJsonObject properties=bad.value("properties").toObject();
    properties.insert("step",-1.0);bad.insert("properties",properties);
    PanelControl dummy;CHECK(!WidgetDocumentCodec::decode(bad,&dummy,&error));
    qInfo()<<"PASS: v5.6.33 professional native inputs, events, JSON, hidden Pages, rendering";
    return 0;
}
