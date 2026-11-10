#include "TestSequenceWidgetFactory.h"
#include "TestSequenceWidget.h"
#include <QDebug>
#include <QtGlobal>

QVector<WidgetProperty> TestSequenceWidgetFactory::properties() const {
    WidgetProperty steps{"defaultSteps","Step names (separated by |)","string",true};
    steps.defaultValue=QStringLiteral("Initialize device|Verify power supply|Capture response|Record results");
    steps.category=QStringLiteral("Sequence");
    WidgetProperty maxSteps{"maxSteps","Maximum runtime steps","integer",true,1,100};
    maxSteps.defaultValue=64;maxSteps.category=QStringLiteral("Sequence");
    auto flag=[](const QString& id,const QString& label,bool def){
        WidgetProperty p{id,label,"boolean",true};p.defaultValue=def;
        p.category=QStringLiteral("Appearance");return p;
    };
    return {{"text","Sequence title","string",true},steps,maxSteps,
            flag("showProgress","Show overall progress",true),
            flag("showElapsed","Show elapsed time",true),
            flag("showNumbers","Show step numbers",true),
            flag("autoScroll","Follow running step",true),
            flag("compactMode","Compact row layout",false)};
}
PanelControl TestSequenceWidgetFactory::defaultDefinition(const QString& id) const {
    PanelControl d=WidgetFactory::defaultDefinition(id);
    d.text=QStringLiteral("Controller Functional Test");
    d.geometry.setSize(QSize(540,380));
    d.properties.insert("defaultSteps",QStringLiteral("Initialize device|Verify power supply|Capture response|Record results"));
    d.properties.insert("maxSteps",64);
    d.properties.insert("showProgress",true);
    d.properties.insert("showElapsed",true);
    d.properties.insert("showNumbers",true);
    d.properties.insert("autoScroll",true);
    d.properties.insert("compactMode",false);
    return d;
}
QVariantList TestSequenceWidgetFactory::initialSteps(const PanelControl& d) {
    QVariantList list;
    const QString names=d.properties.value("defaultSteps").toString(
        QStringLiteral("Initialize device|Verify power supply|Capture response|Record results"));
    const int limit=qBound(1,d.properties.value("maxSteps").toInt(64),100);
    int number=0;
    for(const QString& name:names.split('|')) {
        if(list.size()>=limit)break;
        const QString clean=name.trimmed().left(120);
        if(clean.isEmpty())continue;
        list.push_back(QVariantMap{{"id",QStringLiteral("step%1").arg(++number)},
                                   {"name",clean},{"status",QStringLiteral("pending")}});
    }
    return list;
}
QVariant TestSequenceWidgetFactory::initialRuntimeValue(const PanelControl& d) const {
    return initialSteps(d);
}
QWidget* TestSequenceWidgetFactory::create(const PanelControl& d,QWidget* parent,EventSink) const {
    auto* widget=new TestSequenceWidget(parent);
    applyDefinition(widget,d);
    return widget;
}
void TestSequenceWidgetFactory::applyDefinition(QWidget* base,const PanelControl& d) const {
    auto* widget=dynamic_cast<TestSequenceWidget*>(base);
    if(!widget)return;
    const auto value=[&d](const char* key,bool def){
        const QJsonValue v=d.properties.value(QString::fromLatin1(key));
        return v.isUndefined()?def:v.toBool(def);
    };
    widget->configure(d.text,value("showProgress",true),value("showElapsed",true),
                      value("showNumbers",true),value("autoScroll",true),
                      value("compactMode",false),d.properties.value("maxSteps").toInt(64));
    // Only design-time step names are saved. Live statuses/results are transient.
    widget->applySteps(initialSteps(d));
}
void TestSequenceWidgetFactory::readDefinition(QWidget*,PanelControl&) const {
    // Never persist PASS/FAIL, execution timing or live JavaScript updates.
}
void TestSequenceWidgetFactory::applyRuntimeValue(QWidget* base,const PanelControl&,
                                                  const QVariant& data) const {
    if(auto* widget=dynamic_cast<TestSequenceWidget*>(base)) {
        if(!widget->applySteps(data))
            qWarning()<<"WFScript Test Sequence: rejected ui.set payload for"
                      <<widget->objectName()<<"type="<<data.metaType().name();
    }
}
