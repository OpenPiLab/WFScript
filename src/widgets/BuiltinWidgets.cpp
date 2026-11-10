#include "BuiltinWidgets.h"
#include "WidgetRegistry.h"
#include "PanelGauge.h"
#include "PanelLabelWidget.h"
#include "RunInformationWidget.h"
#include "DigitalDisplayWidgetFactory.h"
#include "MeasurementResultWidgetFactory.h"
#include "MeasurementTableWidgetFactory.h"
#include "TestSequenceWidgetFactory.h"
#include "EventLogWidgetFactory.h"
#include "PrecisionDialWidgetFactory.h"
#include "StatusBeaconWidgetFactory.h"
#include "SegmentedBarWidgetFactory.h"
#include "EducationWidgets.h"
#include "ExperimentWidgets.h"
#include "ProfessionalInputFactories.h"
#include "../plot/PanelPlotFactory.h"
#include "../plot/DigitalTimingPlotFactory.h"
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QtGlobal>
#include <memory>
#include <utility>

namespace {
class BuiltinWidgetFactory final : public WidgetFactory {
public:
    explicit BuiltinWidgetFactory(QString type) : m_type(std::move(type)) {}
    QString typeId() const override { return m_type; }
    QString displayName() const override { auto s=m_type;return s.left(1).toUpper()+s.mid(1); }
    PanelControl defaultDefinition(const QString& id) const override {
        PanelControl d=WidgetFactory::defaultDefinition(id);
        d.text=m_type=="button"?QStringLiteral("Click me"):m_type;
        d.geometry.setSize(m_type=="gauge"?QSize(160,145):QSize(150,38));
        d.value=m_type=="slider"?50:0;
        return d;
    }
    QVariant initialRuntimeValue(const PanelControl& d) const override {
        return (m_type=="label" || m_type=="button") ? QVariant(d.text) : QVariant(d.value);
    }
    QVector<WidgetProperty> properties() const override {
        QVector<WidgetProperty> result;
        if(m_type=="label")
            result.append({"text","Label text / HTML","multiline",true});
        else if(m_type=="button" || m_type=="toggle" || m_type=="gauge")
            result.append({"text","Text/title","string",true});
        if(m_type=="label") {
            WidgetProperty format{"textFormat","Text Format","choice",true};
            format.choices={"Automatic","Plain Text","Rich Text"};
            format.defaultValue="Automatic"; // QLabel legacy AutoText behavior
            format.category="Text";
            result.append(format);
            WidgetProperty wrap{"wordWrap","Word Wrap","boolean",true};
            wrap.defaultValue=false; // QLabel legacy setting
            wrap.category="Text";
            result.append(wrap);
            WidgetProperty border{"borderVisible","Show Border","boolean",true};
            border.defaultValue=true;
            border.category="Appearance";
            result.append(border);
            WidgetProperty radius{"cornerRadius","Corner Radius (px)","integer",true};
            radius.minimum=0;
            radius.maximum=48;
            radius.decimals=0;
            radius.defaultValue=0;
            radius.category="Appearance";
            result.append(radius);
        }
        if(m_type=="slider" || m_type=="toggle" || m_type=="gauge" || m_type=="numeric")
            result.append({"value","Value","double",true});
        if(m_type=="slider" || m_type=="gauge") {
            result.append({"min","Minimum","double",true});
            result.append({"max","Maximum","double",true});
        }
        return result;
    }
    QVector<PanelEventDefinition> events() const override {
        if(m_type=="button")return {{"clicked","clicked","Activated when the user clicks the button.",QString()}};
        if(m_type=="slider")return {{"valueChanged","valueChanged(value: number)",
            "Fires when the user changes the slider. Value is the actual configured numeric range.","value"}};
        if(m_type=="toggle")return {{"toggled","toggled(checked: boolean)",
            "Fires when the checkbox is toggled.","checked"}};
        return {};
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override {
        QWidget* widget=nullptr;
        if(m_type=="button") {
            auto* b=new QPushButton(d.text,parent);
            QObject::connect(b,&QPushButton::clicked,b,[sink=std::move(sink)]{sink("clicked",true);});
            widget=b;
        } else if(m_type=="label") {
            auto* l=new PanelLabelWidget(d.text,parent);
            l->setTextPresentation(d.properties.value("textFormat").toString("Automatic"),
                                   d.properties.value("wordWrap").toBool(false));
            l->setBorderAppearance(d.properties.value("borderVisible").toBool(true),
                                   d.properties.value("cornerRadius").toInt(0));
            widget=l;
        } else if(m_type=="numeric") {
            auto* l=new QLabel(QString::number(d.value,'f',2),parent);
            l->setAlignment(Qt::AlignCenter);
            l->setFrameShape(QFrame::StyledPanel);
            widget=l;
        } else if(m_type=="slider") {
            auto* s=new QSlider(Qt::Horizontal,parent);
            s->setRange(0,1000);
            s->setProperty("wf_minimum",d.minimum);
            s->setProperty("wf_maximum",d.maximum);
            const double span=d.maximum-d.minimum;
            s->setValue(span>0?int(qBound(0.0,(d.value-d.minimum)/span,1.0)*1000):0);
            QObject::connect(s,&QSlider::valueChanged,s,[s,sink=std::move(sink)](int){
                const double minimum=s->property("wf_minimum").toDouble();
                const double maximum=s->property("wf_maximum").toDouble();
                const double value=minimum+(maximum-minimum)*s->value()/1000.0;
                sink("valueChanged",value);
            });
            widget=s;
        } else if(m_type=="toggle") {
            auto* b=new QCheckBox(d.text,parent);
            b->setChecked(d.value!=0);
            QObject::connect(b,&QCheckBox::toggled,b,[sink=std::move(sink)](bool checked){sink("toggled",checked);});
            widget=b;
        } else if(m_type=="gauge") {
            auto* g=new PanelGauge(parent);
            g->configure(d.minimum,d.maximum,d.value,d.text);
            widget=g;
        }
        return widget;
    }
    void applyDefinition(QWidget* w,const PanelControl& d) const override {
        if(auto* b=qobject_cast<QPushButton*>(w))b->setText(d.text);
        else if(auto* l=qobject_cast<QLabel*>(w)) {
            l->setText(m_type=="numeric"?QString::number(d.value,'f',2):d.text);
            if(auto* label=dynamic_cast<PanelLabelWidget*>(l)) {
                label->setTextPresentation(d.properties.value("textFormat").toString("Automatic"),
                                           d.properties.value("wordWrap").toBool(false));
                label->setBorderAppearance(d.properties.value("borderVisible").toBool(true),
                                           d.properties.value("cornerRadius").toInt(0));
            }
        }
        else if(auto* g=dynamic_cast<PanelGauge*>(w))g->configure(d.minimum,d.maximum,d.value,d.text);
        else if(auto* t=qobject_cast<QCheckBox*>(w)) {
            QSignalBlocker block(t);t->setText(d.text);t->setChecked(d.value!=0);
        } else if(auto* s=qobject_cast<QSlider*>(w)) {
            QSignalBlocker block(s);const double span=d.maximum-d.minimum;
            s->setProperty("wf_minimum",d.minimum);
            s->setProperty("wf_maximum",d.maximum);
            s->setValue(span>0?int(qBound(0.0,(d.value-d.minimum)/span,1.0)*1000):0);
        }
    }
    void readDefinition(QWidget* w,PanelControl& d) const override {
        if(auto* s=qobject_cast<QSlider*>(w))
            d.value=d.minimum+(d.maximum-d.minimum)*s->value()/1000.;
        else if(auto* t=qobject_cast<QCheckBox*>(w))d.value=t->isChecked()?1:0;
        else if(auto* b=qobject_cast<QPushButton*>(w))d.text=b->text();
        else if(auto* l=qobject_cast<QLabel*>(w)) {
            if(m_type=="label")d.text=l->text();
            else if(m_type=="numeric")d.value=l->text().toDouble();
        }
    }
    void applyRuntimeValue(QWidget* w,const PanelControl& d,const QVariant& value) const override {
        if(auto* g=dynamic_cast<PanelGauge*>(w))g->setValue(value.toDouble());
        else if(auto* b=qobject_cast<QPushButton*>(w))b->setText(value.toString());
        else if(auto* l=qobject_cast<QLabel*>(w))l->setText(value.toString());
        else if(auto* t=qobject_cast<QCheckBox*>(w)){
            QSignalBlocker block(t);t->setChecked(value.toBool());
        } else if(auto* s=qobject_cast<QSlider*>(w)){
            QSignalBlocker block(s);const double span=d.maximum-d.minimum;
            s->setValue(span>0?int(qBound(0.0,(value.toDouble()-d.minimum)/span,1.0)*1000):0);
        }
    }
private:
    QString m_type;
};
}
namespace {
class RunInformationWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("runInformation");}
    QString displayName() const override {return QStringLiteral("Run Information (UUT)");}
    PanelControl defaultDefinition(const QString& id) const override {
        auto d=WidgetFactory::defaultDefinition(id);
        d.text=QStringLiteral("Run Information");
        d.geometry.setSize(QSize(440,270));
        return d;
    }
    QVector<WidgetProperty> properties() const override {return {};}
    QVariant initialRuntimeValue(const PanelControl&) const override {return QVariantMap{};}
    QWidget* create(const PanelControl&,QWidget* parent,EventSink sink) const override {
        auto* widget=new RunInformationWidget(parent);
        QObject::connect(widget,&RunInformationWidget::valuesChanged,widget,
            [sink=std::move(sink)](const QVariantMap& data){sink(QStringLiteral("runInformationChanged"),data);});
        return widget;
    }
    void applyDefinition(QWidget*,const PanelControl&) const override {}
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* widget,const PanelControl&,const QVariant& value) const override {
        if(auto* form=qobject_cast<RunInformationWidget*>(widget))form->setValues(value.toMap());
    }
};
}

void registerBuiltinWidgets(WidgetRegistry& registry) {
    for(const char* type : {"button","label","slider","toggle","gauge","numeric"})
        registry.registerFactory(std::make_unique<BuiltinWidgetFactory>(QString::fromLatin1(type)));
    registry.registerFactory(std::make_unique<RunInformationWidgetFactory>());
    registry.registerFactory(std::make_unique<PanelImageFactory>());
    registry.registerFactory(std::make_unique<PanelInstructionFactory>());
    registry.registerFactory(std::make_unique<EquationWidgetFactory>());
    registry.registerFactory(std::make_unique<ExperimentDataTableFactory>());
    registry.registerFactory(std::make_unique<PanelPlotFactory>());
    registry.registerFactory(std::make_unique<DigitalTimingPlotFactory>());
    registry.registerFactory(std::make_unique<DigitalDisplayWidgetFactory>());
    registry.registerFactory(std::make_unique<MeasurementResultWidgetFactory>());
    registry.registerFactory(std::make_unique<MeasurementTableWidgetFactory>());
    registry.registerFactory(std::make_unique<TestSequenceWidgetFactory>());
    registry.registerFactory(std::make_unique<EventLogWidgetFactory>());
    registry.registerFactory(std::make_unique<PrecisionDialWidgetFactory>());
    registry.registerFactory(std::make_unique<StatusBeaconWidgetFactory>());
    registry.registerFactory(std::make_unique<SegmentedBarWidgetFactory>());
    registerProfessionalInputWidgets(registry);
}
