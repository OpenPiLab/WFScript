#include "PanelCanvas.h"
#include "PanelEventCatalog.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QCheckBox>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QLineEdit>
#include <QStringList>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

namespace {
#define CHECK(test) do { if(!(test)) { qCritical() << "FAIL:" << #test << "at" << __LINE__;return 1; } } while(false)
class MockWidgetFactory : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("test.custom");}
    QString displayName() const override {return QStringLiteral("Test Custom");}
    QVector<WidgetProperty> properties() const override {return {{"text","Text","string",true}};}
    QVector<PanelEventDefinition> events() const override {
        return {{"activated","activated(value: string)","Test event","value"}};
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink) const override {
        return new QLabel(d.text,parent);
    }
    void applyDefinition(QWidget* w,const PanelControl& d) const override {
        qobject_cast<QLabel*>(w)->setText(d.text);
    }
    void readDefinition(QWidget* w,PanelControl& d) const override {
        d.text=qobject_cast<QLabel*>(w)->text();
    }
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& value) const override {
        qobject_cast<QLabel*>(w)->setText(value.toString());
    }
};
PanelControl make(const QString& id,const QString& type,const QString& text="") {
    PanelControl c;c.id=id;c.type=type;c.text=text;c.geometry=QRect(20,20,160,65);return c;
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    auto& registry=WidgetRegistry::instance();
    for(const QString& required : {"button","label","slider","toggle","gauge","numeric","plot",
                                  "digital_display","precision_dial","status_beacon","segmented_bar",
                                  "measurement_result","measurement_table"})
        CHECK(registry.typeIds().contains(required));
    CHECK(registry.registerFactory(std::make_unique<MockWidgetFactory>())); // first registration succeeds
    CHECK(!registry.registerFactory(std::make_unique<MockWidgetFactory>())); // duplicate rejected
    CHECK(PanelEventCatalog::supports("test.custom","activated"));
    CHECK(!registry.properties("test.custom").isEmpty());

    PanelCanvas canvas;
    CHECK(canvas.addControl(make("start","button","Start")));
    CHECK(canvas.addControl(make("wave","plot","")));
    QJsonArray saved=canvas.toJson();
    CHECK(PanelCanvas::validateJson(saved));
    PanelCanvas reopened;
    QString error;
    CHECK(reopened.fromJson(saved,&error));
    CHECK(reopened.toJson()==saved);
    auto button=reopened.findChild<QPushButton*>("start");
    CHECK(button && button->text()==QStringLiteral("Start"));
    int clickCount=0;
    QObject::connect(&reopened,&PanelCanvas::controlEvent,&reopened,
        [&](const QString& id,const QString& name,const QVariant& argument){
            if(id==QStringLiteral("start") && name==QStringLiteral("clicked") && argument.toBool())
                ++clickCount;
        });
    reopened.setDesignMode(false);
    button->click();
    CHECK(clickCount==1);
    CHECK(button->text()==QStringLiteral("Start")); // clicked=true is not a caption
    reopened.setDesignMode(true);

    // runtime updates must change the persistent QWidget, including on hidden Pages.
    QJsonArray original=reopened.toJson();
    CHECK(reopened.activatePage("main",original,QSize(900,550),&error));
    QPointer<QWidget> originalPlot=reopened.findChild<QWidget*>("wave");
    CHECK(originalPlot);
    CHECK(reopened.activatePage("settings",QJsonArray{},QSize(900,550),&error));
    CHECK(originalPlot && originalPlot->isHidden());
    reopened.applyValue("start",QStringLiteral("Run"));
    CHECK(button->text()==QStringLiteral("Run"));
    CHECK(reopened.activatePage("main",original,QSize(900,550),&error));
    CHECK(originalPlot==reopened.findChild<QWidget*>("wave"));
    CHECK(button->text()==QStringLiteral("Run"));
    reopened.restoreDocumentValues(original);
    CHECK(button->text()==QStringLiteral("Start"));

    // Custom type uses the same JSON contract, Toolbox registry and event catalog.
    PanelCanvas custom;
    PanelControl def=make("custom1","test.custom","Value");
    def.events={{"activated",QJsonObject{{"handler","onActivate"},{"enabled",true}}}};
    CHECK(custom.addControl(def));
    QJsonArray customSaved=custom.toJson();
    QVector<PanelControl> decoded;
    CHECK(WidgetDocumentCodec::decodeList(customSaved,&decoded,&error));
    CHECK(customSaved.at(0).toObject().value("type").toString()==QStringLiteral("test.custom"));
    CHECK(customSaved.at(0).toObject().value("events").toObject().contains("activated"));
    // v5.6.65: two Run Information views on different Pages write to ONE
    // PanelCanvas pending model, even when one Page has never been displayed.
    PanelCanvas shared;
    const QJsonObject schema{{"fields",QJsonArray{
        QJsonObject{{"id","uutId"},{"label","UUT Serial"},{"type","text"},{"required",true}},
        QJsonObject{{"id","revision"},{"label","Revision"},{"type","choice"},
                    {"options",QJsonArray{"A","B"}},{"default","A"}}
    }}};
    shared.setRunInformationDefinition(schema);
    PanelControl infoA=make("formA","runInformation");
    infoA.geometry=QRect(20,20,440,320);
    PanelControl infoB=make("formB","runInformation");
    infoB.geometry=QRect(20,20,440,320);
    QJsonArray secondPage{WidgetDocumentCodec::encode(infoB)};
    CHECK(shared.activatePage("pageA",QJsonArray{},QSize(900,550),&error));
    CHECK(shared.addControl(infoA));
    CHECK(shared.runInformationInputValues().value("revision").toString()=="A");
    shared.setDesignMode(false);
    auto* editA=shared.findChild<QLineEdit*>("uut_uutId");
    CHECK(editA);
    editA->setText("UNIT-001");
    CHECK(shared.runInformationInputValues().value("uutId").toString()=="UNIT-001");
    CHECK(shared.activatePage("pageB",secondPage,QSize(900,550),&error));
    auto* widgetB=shared.findChild<QWidget*>("formB");
    CHECK(widgetB);
    auto* editB=widgetB->findChild<QLineEdit*>("uut_uutId");
    CHECK(editB && editB->text()=="UNIT-001");
    editB->setText("UNIT-002");
    CHECK(editA->text()=="UNIT-002"); // hidden form synchronized, not active
    CHECK(shared.runInformationInputValues().value("uutId").toString()=="UNIT-002");
    CHECK(shared.activatePage("pageA",QJsonArray{},QSize(900,550),&error));
    CHECK(editA->text()=="UNIT-002");
    // A third form on the same Page must inherit the same value immediately.
    PanelControl infoC=make("formC","runInformation");
    infoC.geometry=QRect(470,20,440,320);
    CHECK(shared.addControl(infoC));
    auto* widgetC=shared.findChild<QWidget*>("formC");
    CHECK(widgetC && widgetC->findChild<QLineEdit*>("uut_uutId")->text()=="UNIT-002");
    shared.resetPages();
    CHECK(shared.runInformationInputValues().isEmpty()); // no cross-project leak
    qInfo() << "Widget registry smoke OK";
    return 0;
}
