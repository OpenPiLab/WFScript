#include "ProfessionalInputFactories.h"
#include "WidgetRegistry.h"
#include "InstrumentButtonWidget.h"
#include "RotaryInputWidget.h"
#include "NumericStepperWidget.h"
#include "OptionSelectorWidget.h"
#include "StatusBannerWidget.h"
#include <QJsonObject>
#include <cmath>
#include <memory>
#include <utility>

namespace {
WidgetProperty setting(QString id,QString label,QString kind,QVariant value,
                       double low=-1.e9,double high=1.e9,int decimals=3) {
    WidgetProperty field;
    field.id=std::move(id);field.label=std::move(label);field.kind=std::move(kind);
    field.defaultValue=std::move(value);field.minimum=low;field.maximum=high;
    field.decimals=decimals;return field;
}
WidgetProperty accentSetting() {
    WidgetProperty p=setting("accent","Accent colour","choice",QStringLiteral("cyan"));
    p.choices={"cyan","green","amber","blue","magenta","red","white"};return p;
}
class ButtonFactory final : public WidgetFactory {
public:
    explicit ButtonFactory(bool latch):m_latch(latch){}
    QString typeId() const override {return m_latch?"switch_button":"momentary_button";}
    QString displayName() const override {return m_latch?"Latching Switch":"Momentary Button";}
    PanelControl defaultDefinition(const QString& id) const override {
        PanelControl d=WidgetFactory::defaultDefinition(id);
        d.geometry.setSize(QSize(235,105));d.text=m_latch?"OUTPUT ENABLE":"TRIGGER";
        d.properties.insert("onText",m_latch?"ENABLED":"PRESSED");
        d.properties.insert("offText",m_latch?"DISABLED":"PRESS");
        d.properties.insert("accent",m_latch?"green":"cyan");
        d.value=0;return d;
    }
    QVariant initialRuntimeValue(const PanelControl& d) const override {return d.value!=0.0;}
    QVector<WidgetProperty> properties() const override {
        return {{"text","Title","string",true},{"value","Default state (0 / 1)","double",true},
            setting("onText","Active caption","string",QStringLiteral("ON")),
            setting("offText","Inactive caption","string",QStringLiteral("OFF")),accentSetting()};
    }
    QVector<PanelEventDefinition> events() const override {
        QVector<PanelEventDefinition> e{
            {"clicked","clicked()","Emitted after a completed click.",QString()},
            {"valueChanged","valueChanged(state: boolean)","True / false state change.","state"}};
        if(m_latch)e.append({"toggled","toggled(checked: boolean)","Switch toggled by user.","checked"});
        else {e.append({"pressed","pressed()","Momentary press began.",QString()});
              e.append({"released","released()","Momentary press ended or was cancelled.",QString()});}
        return e;
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override {
        auto* w=new InstrumentButtonWidget(parent);applyDefinition(w,d);
        QObject::connect(w,&InstrumentButtonWidget::userClicked,w,[sink]{sink("clicked",true);});
        QObject::connect(w,&InstrumentButtonWidget::userValueChanged,w,[sink](bool v){sink("valueChanged",v);});
        if(m_latch)
            QObject::connect(w,&InstrumentButtonWidget::userToggled,w,[sink](bool v){sink("toggled",v);});
        else {
            QObject::connect(w,&InstrumentButtonWidget::userPressed,w,[sink]{sink("pressed",true);});
            QObject::connect(w,&InstrumentButtonWidget::userReleased,w,[sink]{sink("released",false);});
        }
        return w;
    }
    void applyDefinition(QWidget* widget,const PanelControl& d) const override {
        auto* w=qobject_cast<InstrumentButtonWidget*>(widget);if(!w)return;
        const auto p=d.properties;
        w->configure(d.text,p.value("onText").toString(m_latch?"ENABLED":"PRESSED"),
                     p.value("offText").toString(m_latch?"DISABLED":"PRESS"),
                     p.value("accent").toString(m_latch?"green":"cyan"),m_latch,d.value!=0);
    }
    void readDefinition(QWidget*,PanelControl&) const override {} // no runtime snapshot
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override {
        if(auto* b=qobject_cast<InstrumentButtonWidget*>(w))b->setState(v.toBool());
    }
private:
    bool m_latch;
};
class RotaryFactory final : public WidgetFactory {
public:
    QString typeId() const override {return "rotary_input";}
    QString displayName() const override {return "Rotary Setpoint";}
    PanelControl defaultDefinition(const QString& id) const override {
        PanelControl d=WidgetFactory::defaultDefinition(id);d.text="FREQUENCY";
        d.geometry.setSize(QSize(255,285));d.minimum=0;d.maximum=100;
        d.value=25;d.properties.insert("unit","Hz");d.properties.insert("step",1.0);
        d.properties.insert("decimals",1);return d;
    }
    QVector<WidgetProperty> properties() const override {
        return {{"text","Title","string",true},{"value","Default value","double",true},
          {"min","Minimum","double",true},{"max","Maximum","double",true},
          setting("step","Increment","double",1.,0.000001,1.e9,6),
          setting("decimals","Decimal places","integer",1,0,6),
          setting("unit","Engineering unit","string",QStringLiteral("Hz")),accentSetting()};
    }
    QVector<PanelEventDefinition> events() const override {
        return {{"valueChanged","valueChanged(value: number)","Knob moved with mouse, wheel, or keyboard.","value"}};
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override {
        auto* w=new RotaryInputWidget(parent);applyDefinition(w,d);
        QObject::connect(w,&RotaryInputWidget::userValueChanged,w,[sink](double v){sink("valueChanged",v);});
        return w;
    }
    void applyDefinition(QWidget* w,const PanelControl& d) const override {
        auto* dial=qobject_cast<RotaryInputWidget*>(w);if(!dial)return;
        const auto p=d.properties;
        dial->configure(d.text,p.value("unit").toString("Hz"),p.value("accent").toString("cyan"),
                        d.value,d.minimum,d.maximum,p.value("step").toDouble(1.),
                        p.value("decimals").toInt(1));
    }
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override {
        bool ok=false;double n=v.toDouble(&ok);
        if(ok && std::isfinite(n))if(auto* dial=qobject_cast<RotaryInputWidget*>(w))dial->setValue(n);
    }
};
class NumericFactory final : public WidgetFactory {
public:
    QString typeId() const override {return "numeric_stepper";}
    QString displayName() const override {return "Precision Numeric Input";}
    PanelControl defaultDefinition(const QString& id) const override {
        PanelControl d=WidgetFactory::defaultDefinition(id);d.text="VOLTAGE SETPOINT";
        d.geometry.setSize(QSize(320,118));d.value=12.5;d.minimum=-100;d.maximum=100;
        d.properties.insert("unit","V");d.properties.insert("step",0.1);
        d.properties.insert("decimals",2);return d;
    }
    QVector<WidgetProperty> properties() const override {
        return {{"text","Title","string",true},{"value","Default value","double",true},
          {"min","Minimum","double",true},{"max","Maximum","double",true},
          setting("step","Increment","double",0.1,0.00000001,1.e9,8),
          setting("decimals","Decimal places","integer",2,0,8),
          setting("unit","Engineering unit","string",QStringLiteral("V")),accentSetting()};
    }
    QVector<PanelEventDefinition> events() const override {
        return {{"valueChanged","valueChanged(value: number)","User edited number or used step buttons.","value"}};
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override {
        auto* w=new NumericStepperWidget(parent);applyDefinition(w,d);
        QObject::connect(w,&NumericStepperWidget::userValueChanged,w,[sink](double v){sink("valueChanged",v);});
        return w;
    }
    void applyDefinition(QWidget* w,const PanelControl& d) const override {
        auto* input=qobject_cast<NumericStepperWidget*>(w);if(!input)return;
        const auto p=d.properties;
        input->configure(d.text,p.value("unit").toString("V"),p.value("accent").toString("cyan"),
                         d.value,d.minimum,d.maximum,p.value("step").toDouble(.1),
                         p.value("decimals").toInt(2));
    }
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override {
        bool ok=false;double n=v.toDouble(&ok);
        if(ok && std::isfinite(n))if(auto* input=qobject_cast<NumericStepperWidget*>(w))input->setValue(n);
    }
};
class ComboFactory final : public WidgetFactory {
public:
    QString typeId() const override {return "option_selector";}
    QString displayName() const override {return "Option Selector";}
    PanelControl defaultDefinition(const QString& id) const override {
        PanelControl d=WidgetFactory::defaultDefinition(id);d.text="OPERATING MODE";
        d.geometry.setSize(QSize(325,112));d.value=0;
        d.properties.insert("options","Automatic|Manual|Calibration|Standby");
        d.properties.insert("accent","blue");return d;
    }
    QVariant initialRuntimeValue(const PanelControl& d) const override {
        const QStringList items=parse(d.properties.value("options").toString("Automatic|Manual|Calibration|Standby"));
        return items.value(qBound(0,d.properties.value("defaultIndex").toInt(0),items.size()-1));
    }
    QVector<WidgetProperty> properties() const override {
        return {{"text","Title","string",true},
            setting("defaultIndex","Initial index (0-based)","integer",0,0,99),
            setting("options","Options separated by |","string",
                    QStringLiteral("Automatic|Manual|Calibration|Standby")),accentSetting()};
    }
    QVector<PanelEventDefinition> events() const override {
        return {{"selectionChanged","selectionChanged(text: string)",
                 "User selected an option. Passes its label.","text"},
                {"indexChanged","indexChanged(index: number)",
                 "User selected an option. Passes its 0-based index.","index"}};
    }
    static QStringList parse(const QString& text) {
        QStringList result;
        for(const auto& option:text.split('|',Qt::KeepEmptyParts)) {
            const QString trimmed=option.trimmed();
            if(!trimmed.isEmpty() && !result.contains(trimmed))result.append(trimmed.left(100));
            if(result.size()==100)break;
        }
        return result.isEmpty()?QStringList{"Option 1"}:result;
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override {
        auto* w=new OptionSelectorWidget(parent);applyDefinition(w,d);
        QObject::connect(w,&OptionSelectorWidget::userSelectionChanged,w,
                         [sink](const QString& text){sink("selectionChanged",text);});
        QObject::connect(w,&OptionSelectorWidget::userIndexChanged,w,
                         [sink](int index){sink("indexChanged",index);});
        return w;
    }
    void applyDefinition(QWidget* w,const PanelControl& d) const override {
        auto* selector=qobject_cast<OptionSelectorWidget*>(w);if(!selector)return;
        const auto p=d.properties;
        selector->configure(d.text,p.value("accent").toString("blue"),
                            parse(p.value("options").toString("Automatic|Manual|Calibration|Standby")),
                            p.value("defaultIndex").toInt(0));
    }
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override {
        if(auto* selector=qobject_cast<OptionSelectorWidget*>(w))selector->setSelection(v);
    }
};
class BannerFactory final : public WidgetFactory {
public:
    QString typeId() const override {return "status_banner";}
    QString displayName() const override {return "Multi-State Status Banner";}
    PanelControl defaultDefinition(const QString& id) const override {
        PanelControl d=WidgetFactory::defaultDefinition(id);
        d.text="SYSTEM HEALTH";d.geometry.setSize(QSize(330,100));d.value=0;
        return d;
    }
    QVector<WidgetProperty> properties() const override {
        return {{"text","Title","string",true},{"value","Default state (0 / 1 / 2)","double",true},
            setting("okText","Normal label","string",QStringLiteral("NORMAL")),
            setting("warningText","Warning label","string",QStringLiteral("WARNING")),
            setting("alarmText","Alarm label","string",QStringLiteral("ALARM"))};
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink) const override {
        auto* w=new StatusBannerWidget(parent);applyDefinition(w,d);return w;
    }
    void applyDefinition(QWidget* w,const PanelControl& d) const override {
        auto* banner=qobject_cast<StatusBannerWidget*>(w);if(!banner)return;
        const auto p=d.properties;
        banner->configure(d.text,p.value("okText").toString("NORMAL"),
                          p.value("warningText").toString("WARNING"),
                          p.value("alarmText").toString("ALARM"),
                          static_cast<int>(qBound(0.0,d.value,2.0)));
    }
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override {
        bool ok=false;double level=v.toDouble(&ok);
        if(ok && std::isfinite(level))if(auto* banner=qobject_cast<StatusBannerWidget*>(w))
            banner->setLevel(static_cast<int>(qBound(0.0,level,2.0)));
    }
};
}
void registerProfessionalInputWidgets(WidgetRegistry& registry) {
    registry.registerFactory(std::make_unique<ButtonFactory>(false));
    registry.registerFactory(std::make_unique<ButtonFactory>(true));
    registry.registerFactory(std::make_unique<RotaryFactory>());
    registry.registerFactory(std::make_unique<NumericFactory>());
    registry.registerFactory(std::make_unique<ComboFactory>());
    registry.registerFactory(std::make_unique<BannerFactory>());
}
