// v0.2-dev.8.3: native Button/Label typography regression and v8.2 paint contracts.
// Runs offscreen with no AD3, socket, database, JavaScript or worker threads.
#include "widgets/WidgetRegistry.h"
#include "widgets/PanelGauge.h"
#include "widgets/PanelLabelWidget.h"
#include "widgets/PanelSliderWidget.h"
#include "widgets/OptionSelectorWidget.h"
#include <QComboBox>
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QFont>
#include <QEvent>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <QCoreApplication>
#include <memory>

#define CHECK(test) do {if(!(test)){qCritical()<<"FAIL"<<#test<<__LINE__;return 1;}} while(false)

static QWidget* make(const QString& type, const QString& id, const QString& caption,
                     QWidget* parent) {
    const WidgetFactory* factory=WidgetRegistry::instance().factory(type);
    if(!factory)return nullptr;
    PanelControl d=factory->defaultDefinition(id);
    d.text=caption;
    QWidget* widget=factory->create(d,parent,[](const QString&,const QVariant&){});
    if(widget)factory->applyDefinition(widget,d);
    return widget;
}

static int pixels(const QWidget* w) {return w->font().pixelSize();}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QWidget host;
    host.resize(1000,800);
    host.setProperty("wfPanelLight",false);
    host.show();
    auto* button=qobject_cast<QPushButton*>(make("button","button","RUN",&host));
    auto* label=qobject_cast<QLabel*>(make("label","label","STATUS",&host));
    auto* toggle=qobject_cast<QCheckBox*>(make("toggle","toggle","ENABLE",&host));
    auto* numeric=qobject_cast<QLabel*>(make("numeric","numeric","0.00",&host));
    auto* gauge=dynamic_cast<PanelGauge*>(make("gauge","gauge","VOLTAGE",&host));
    CHECK(button && label && toggle && numeric && gauge);
    CHECK(dynamic_cast<PanelLabelWidget*>(label)!=nullptr);
    CHECK(numeric->frameShape()==QFrame::NoFrame); // old StyledPanel bevel removed
    // Fresh labels return to the old square-outline default. Explicit
    // rounded corners remain supported through their saved property.
    CHECK(!WidgetRegistry::instance().factory("label")
              ->defaultDefinition("fresh").properties.contains("cornerRadius"));
    for(QWidget* w : {static_cast<QWidget*>(button),static_cast<QWidget*>(label),
                      static_cast<QWidget*>(toggle),static_cast<QWidget*>(numeric)}) {
        w->resize(160,42);
        w->show();
    }
    QCoreApplication::processEvents();
    const QFont buttonFont=button->font(), labelFont=label->font();
    const int t1=pixels(toggle),n1=pixels(numeric);
    for(QWidget* w : {static_cast<QWidget*>(button),static_cast<QWidget*>(label),
                      static_cast<QWidget*>(toggle),static_cast<QWidget*>(numeric)})
        w->resize(350,95);
    QCoreApplication::processEvents();
    CHECK(button->font()==buttonFont); // no auto-resize
    CHECK(label->font()==labelFont);   // no auto-resize, including HTML
    CHECK(pixels(toggle)>t1);
    CHECK(pixels(numeric)>n1);
    CHECK(pixels(toggle)<=62 && pixels(numeric)<=90);
    // Direct runtime updates keep captions and labels responsive, with no
    // synthetic operator event. The legacy Qt widget interfaces remain intact.
    const WidgetFactory* bf=WidgetRegistry::instance().factory("button");
    PanelControl bd=bf->defaultDefinition("button");
    bf->applyRuntimeValue(button,bd,QStringLiteral("RECONFIGURE"));
    CHECK(button->text()=="RECONFIGURE");
    CHECK(button->font()==buttonFont);
    // Rich Text must remain Qt-native HTML markup, including authored styles.
    const WidgetFactory* lf=WidgetRegistry::instance().factory("label");
    PanelControl ld=lf->defaultDefinition("label");
    ld.properties.insert("textFormat",QStringLiteral("Rich Text"));
    ld.properties.insert("wordWrap",true);
    const QString rich=QStringLiteral("<b>READY</b><br><span style=\"color:#36bd83; font-size:18pt\">3.300 V</span>");
    ld.text=rich;
    lf->applyDefinition(label,ld);
    CHECK(label->textFormat()==Qt::RichText && label->wordWrap());
    CHECK(label->text()==rich && label->font()==labelFont);
    label->resize(460,110);
    QCoreApplication::processEvents();
    CHECK(label->font()==labelFont);
    const QImage richPreview=label->grab().toImage();
    CHECK(!richPreview.isNull());
    lf->applyRuntimeValue(label,ld,QStringLiteral("<i>RUNNING</i><br>Line two"));
    CHECK(label->textFormat()==Qt::RichText);
    CHECK(label->font()==labelFont && label->text().contains("<i>RUNNING</i>"));
    ld.properties.insert("textFormat",QStringLiteral("Plain Text"));
    ld.text=QStringLiteral("Literal <b>markup</b>");
    lf->applyDefinition(label,ld);
    CHECK(label->textFormat()==Qt::PlainText && label->text()==ld.text);
    CHECK(label->font()==labelFont);
    const WidgetFactory* nf=WidgetRegistry::instance().factory("numeric");
    nf->applyRuntimeValue(numeric,nf->defaultDefinition("numeric"),24.375);
    CHECK(numeric->text()=="24.375");
    CHECK(pixels(numeric)>=10);

    gauge->resize(220,185);
    gauge->show();
    numeric->resize(220,80);
    QCoreApplication::processEvents();
    const QImage darkGauge=gauge->grab().toImage();
    const QImage darkNumeric=numeric->grab().toImage();
    CHECK(!darkGauge.isNull() && !darkNumeric.isNull());
    CHECK(darkGauge.pixelColor(25,25).lightness()<150);
    CHECK(darkNumeric.pixelColor(25,25).lightness()<150);
    // Both controls now draw an inset frame rather than platform bevels.
    CHECK(darkGauge.pixelColor(110,1)!=darkGauge.pixelColor(110,8));
    CHECK(darkNumeric.pixelColor(110,1)!=darkNumeric.pixelColor(110,8));
    host.setProperty("wfPanelLight",true);
    for(QWidget* w : {static_cast<QWidget*>(gauge),static_cast<QWidget*>(numeric)}) {
        QEvent event(QEvent::ThemeChange);
        QApplication::sendEvent(w,&event);
        w->update();
    }
    QCoreApplication::processEvents();
    const QImage lightGauge=gauge->grab().toImage();
    const QImage lightNumeric=numeric->grab().toImage();
    CHECK(lightGauge.pixelColor(25,25).lightness()>darkGauge.pixelColor(25,25).lightness());
    CHECK(lightNumeric.pixelColor(25,25).lightness()>darkNumeric.pixelColor(25,25).lightness());
    // v0.2-dev.8.7: the original Slider becomes a single themed, titled
    // composite widget while preserving native valueChanged and ui.set() behavior.
    const WidgetFactory* sf=WidgetRegistry::instance().factory("slider");
    PanelControl sd=sf->defaultDefinition("harmonics");
    CHECK(sd.geometry.height()>=62 && sd.text=="SLIDER");
    sd.text="3rd HARMONIC (%)";
    sd.minimum=0;sd.maximum=75;sd.value=35;
    int userChanges=0;
    double lastValue=-1;
    auto* slider=dynamic_cast<PanelSliderWidget*>(sf->create(sd,&host,
        [&](const QString& event,const QVariant& value){
            if(event=="valueChanged") {++userChanges;lastValue=value.toDouble();}
        }));
    CHECK(slider && slider->slider() && slider->title()==sd.text);
    slider->resize(344,62);slider->show();
    QCoreApplication::processEvents();
    CHECK(slider->showsValue() && slider->showsRange());
    CHECK(qAbs(slider->value()-35.0)<0.08);
    CHECK(slider->slider()->geometry().width()>250);
    slider->slider()->setValue(800); // physical user-style event, 60/75
    CHECK(userChanges==1 && qAbs(lastValue-60.)<0.001);
    sf->applyRuntimeValue(slider,sd,30.); // UI/Data binding: silent
    CHECK(userChanges==1 && qAbs(slider->value()-30.)<0.001);
    sf->readDefinition(slider,sd);
    CHECK(qAbs(sd.value-30.)<0.001);
    sd.text="NOISE FLOOR (%)";
    sd.minimum=-10;sd.maximum=10;sd.value=0;
    sd.properties.insert("showRange",false);
    sf->applyDefinition(slider,sd);
    CHECK(slider->title()==sd.text && !slider->showsRange());
    CHECK(userChanges==1 && qAbs(slider->value())<0.001);
    // Rendering should follow the inherited appearance without reconstructing
    // the native QSlider or changing its callbacks.
    const QImage lightSlider=slider->grab().toImage();
    CHECK(!lightSlider.isNull());
    host.setProperty("wfPanelLight",false);
    QEvent sliderTheme(QEvent::ThemeChange);
    QApplication::sendEvent(slider,&sliderTheme);
    slider->update();QCoreApplication::processEvents();
    const QImage darkSlider=slider->grab().toImage();
    CHECK(!darkSlider.isNull());
    CHECK(lightSlider.pixelColor(6,6).lightness()>
          darkSlider.pixelColor(6,6).lightness());
    // Legacy small saved sliders keep working (hide endpoint labels when tight).
    slider->resize(150,38);
    QCoreApplication::processEvents();
    CHECK(slider->slider()->geometry().width()>100);
    sf->applyRuntimeValue(slider,sd,7.5);
    CHECK(userChanges==1 && qAbs(slider->value()-7.5)<0.021);
    // Old Button / rich-text Label are not part of Slider repainting.
    CHECK(button->font()==buttonFont && label->font()==labelFont);

    // Catalog-wide audit: every registered native factory remains constructible
    // v0.2-dev.8.8: styled Option Selector keeps the native popup, while
    // reserving a fully transparent right-side well instead of a square tile.
    const auto* optionFactory=WidgetRegistry::instance().factory("option_selector");
    CHECK(optionFactory);
    PanelControl od=optionFactory->defaultDefinition("mode");
    od.text="OPERATING MODE";
    od.properties.insert("options","Auto|Manual|Calibration");
    auto* option=qobject_cast<OptionSelectorWidget*>(optionFactory->create(od,&host,
        [](const QString&,const QVariant&){}));
    CHECK(option);
    auto* combo=option->findChild<QComboBox*>();
    CHECK(combo && combo->count()==3);
    option->resize(420,114);
    option->show();
    QCoreApplication::processEvents();
    CHECK(combo->styleSheet().contains("QComboBox::drop-down"));
    CHECK(combo->styleSheet().contains("padding-right:36px"));
    CHECK(combo->geometry().right()<option->width());
    CHECK(!option->grab().isNull());
    option->setSelection(QStringLiteral("Calibration"));
    CHECK(option->currentText()==QStringLiteral("Calibration"));
    option->setSelection(1);
    CHECK(option->currentIndex()==1);
    host.setProperty("wfPanelLight",true);
    { QEvent change(QEvent::ThemeChange);QApplication::sendEvent(option,&change); }
    CHECK(!option->grab().isNull());
    host.setProperty("wfPanelLight",false);
    { QEvent change(QEvent::ThemeChange);QApplication::sendEvent(option,&change); }
    option->resize(190,72);
    QCoreApplication::processEvents();
    CHECK(combo->width()>0 && combo->height()>0);
    CHECK(!option->grab().isNull());
    // with its own defaults, and none requires a physical instrument at startup.
    auto& registry=WidgetRegistry::instance();
    CHECK(registry.typeIds().size()>=29); // allow future, intentional additions
    int count=0;
    for(const QString& type:registry.typeIds()) {
        const WidgetFactory* factory=registry.factory(type);
        CHECK(factory && !factory->displayName().isEmpty());
        PanelControl def=factory->defaultDefinition(QStringLiteral("audit_%1").arg(count));
        CHECK(def.type==type && def.geometry.width()>0 && def.geometry.height()>0);
        std::unique_ptr<QWidget> widget(factory->create(def,nullptr,
                                            [](const QString&,const QVariant&){}));
        CHECK(widget!=nullptr);
        widget->resize(def.geometry.size());
        factory->applyDefinition(widget.get(),def);
        ++count;
    }
    CHECK(count==registry.typeIds().size());
    qInfo()<<"PASS:"<<count<<"widget factories, fixed Button/Label + rich HTML, themed Slider with live value/label, Toggle/Numeric/Gauge";
    return 0;
}
