// Native offscreen test: no hardware, no JavaScript thread.
#include "PanelProject.h"
#include "PanelDesigner.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/PrecisionDialWidget.h"
#include <QApplication>
#include <QDebug>
#include <QPointer>
#include <QJsonArray>
#include <QJsonObject>
#define CHECK(ok) do {if(!(ok)){qCritical()<<"FAIL"<<#ok<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PanelProject project;
    project.appearance.themeMode=QStringLiteral("dark");
    project.appearance.background=QStringLiteral("#14253A");
    project.appearance.showDesignGrid=false;
    QString error;
    CHECK(project.addPage(QStringLiteral("status"),QStringLiteral("Status"),QSize(900,550),&error));
    project.pages[1].background=QStringLiteral("#203246");
    QJsonObject encoded;
    CHECK(project.toJson(&encoded,&error));
    CHECK(encoded.value("version").toInt()==2);
    PanelProject decoded;
    CHECK(PanelProject::fromJson(encoded,QStringLiteral("demo.wfpanel"),&decoded,&error));
    CHECK(decoded.appearance.themeMode==QStringLiteral("dark"));
    CHECK(decoded.appearance.background==QStringLiteral("#14253A"));
    CHECK(!decoded.appearance.showDesignGrid);
    CHECK(decoded.pages[1].background==QStringLiteral("#203246"));
    PanelProject legacy;
    QJsonObject oldJson;
    CHECK(legacy.toJson(&oldJson,&error));
    CHECK(!oldJson.contains(QStringLiteral("appearance")));
    CHECK(PanelProject::fromJson(oldJson,QStringLiteral("old.wfpanel"),&legacy,&error));
    CHECK(legacy.appearance.themeMode==QStringLiteral("follow"));
    PanelDesigner designer;
    designer.setPageAppearance(decoded.appearance,QString(),QStringLiteral("Main"));
    CHECK(designer.canvas()->pageBackground()==QColor(QStringLiteral("#14253A")));
    auto* factory=WidgetRegistry::instance().factory(QStringLiteral("precision_dial"));
    CHECK(factory);
    PanelControl def=factory->defaultDefinition(QStringLiteral("precision"));
    CHECK(designer.canvas()->addControl(def));
    QPointer<PrecisionDialWidget> instrument=designer.canvas()->findChild<PrecisionDialWidget*>(QStringLiteral("precision"));
    CHECK(instrument);
    designer.canvas()->applyValue(QStringLiteral("precision"),37.5);
    CHECK(instrument->value()==37.5);
    PanelAppearance light=decoded.appearance;
    light.themeMode=QStringLiteral("light");light.background.clear();
    designer.setPageAppearance(light,QStringLiteral("#EAF2F8"),QStringLiteral("Main"));
    CHECK(designer.canvas()->pageBackground()==QColor(QStringLiteral("#EAF2F8")));
    CHECK(instrument==designer.canvas()->findChild<PrecisionDialWidget*>(QStringLiteral("precision")));
    CHECK(instrument->value()==37.5);
    CHECK(instrument->property("wfPanelLight").isValid()==false); // inherited, not cached per widget
    const QJsonArray controls=designer.canvas()->toJson();
    CHECK(designer.canvas()->activatePage(QStringLiteral("main"),controls,QSize(900,550),&error));
    CHECK(designer.canvas()->activatePage(QStringLiteral("status"),{},QSize(900,550),&error));
    CHECK(instrument && instrument->isHidden());
    CHECK(designer.canvas()->activatePage(QStringLiteral("main"),controls,QSize(900,550),&error));
    CHECK(instrument && instrument->value()==37.5);
    qInfo()<<"PASS: v5.6.34 Panel themes, round-trip persistence and persistent widgets";
    return 0;
}
