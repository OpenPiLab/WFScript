// v5.6.55: native offscreen Qt event log smoke, no hardware or DWF DLL.
#include "widgets/EventLogWidget.h"
#include "widgets/EventLogWidgetFactory.h"
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <QPalette>
#include <QBrush>
#include <QJSEngine>
#include <QJSValue>
#include <QTableWidget>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QVariantList>
#include <QVariantMap>

#define CHECK(cond) do {if(!(cond)){qCritical()<<"FAIL"<<#cond<<"line"<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    EventLogWidgetFactory factory;
    CHECK(factory.typeId()==QStringLiteral("event_log"));
    PanelControl definition=factory.defaultDefinition("log1");
    CHECK(definition.properties.value("maxEntries").toInt()==500);
    QWidget parent;
    QWidget* base=factory.create(definition,&parent,{});
    auto* log=dynamic_cast<EventLogWidget*>(base);
    CHECK(log && log->entryCount()==0);
    CHECK(log->findChild<QTableWidget*>(QStringLiteral("eventLogTable")));
    auto* filter=log->findChild<QComboBox*>(QStringLiteral("eventLogFilter"));
    CHECK(filter);
    CHECK(filter->count()==7);
    // Regression: all seven unhovered menu entries must be readable against
    // their popup background (even under a dark Panel appearance).
    auto assertReadableFilter=[&](bool light)->bool {
        const QColor expectedInk(light?QStringLiteral("#203347"):QStringLiteral("#e4f0f9"));
        auto* popup=filter->view();
        if(!popup || popup->objectName()!=QStringLiteral("eventLogFilterPopup"))return false;
        if(popup->palette().color(QPalette::Text)!=expectedInk)return false;
        if(popup->palette().color(QPalette::Base)==expectedInk)return false;
        if(popup->palette().color(QPalette::HighlightedText)!=expectedInk)return false;
        if(popup->palette().color(QPalette::Highlight)==expectedInk)return false;
        for(int i=0;i<filter->count();++i) {
            if(filter->itemData(i,Qt::ForegroundRole).value<QBrush>().color()!=expectedInk)return false;
        }
        return true;
    };
    CHECK(assertReadableFilter(false));
    parent.setProperty("wfPanelLight",true);
    log->configure(QStringLiteral("Event Log"),500,true,true,true);
    CHECK(assertReadableFilter(true));
    parent.setProperty("wfPanelLight",false);
    log->configure(QStringLiteral("Event Log"),500,true,true,true);
    CHECK(assertReadableFilter(false));
    CHECK(log->applyEvents(QVariantMap{{"append",QVariantMap{{"level","INFO"},{"message","Started"},{"source","Station"}}}}));
    CHECK(log->entryCount()==1);
    CHECK(log->levelAt(0)==QStringLiteral("INFO"));
    // Real QJSValue conversion path identical to PanelUiBridge::set.
    QJSEngine engine;
    const QJSValue js=engine.evaluate(QStringLiteral("({append:[{level:'PASS',message:'3.301 V',source:'Power'},"
                                                    "{level:'WARN',message:'Something changed'}]})"));
    CHECK(!js.isError());
    CHECK(log->applyEvents(js.toVariant(QJSValue::ConvertJSObjects)));
    CHECK(log->entryCount()==3);
    CHECK(log->levelAt(1)==QStringLiteral("PASS"));
    log->setFilter("PASS");
    CHECK(log->visibleEntryCount()==1);
    log->setFilter("All");
    CHECK(log->visibleEntryCount()==3);
    // Malformed append batch must not partially modify the log.
    QVariantList bad{QVariantMap{{"message","allowed"}},QVariantMap{{"level","INVALID"},{"message","reject"}}};
    CHECK(!log->applyEvents(QVariantMap{{"append",bad}}));
    CHECK(log->entryCount()==3);
    CHECK(!log->applyEvents(QVariantMap{{"append",QVariantMap{{"level","PASS"}}}}));
    CHECK(log->entryCount()==3);
    CHECK(!log->applyEvents(QVariantMap{{"clear",false}}));
    CHECK(log->entryCount()==3);
    CHECK(log->csvText().contains(QStringLiteral("PASS")));
    CHECK(log->csvText().contains(QStringLiteral("3.301 V")));
    CHECK(log->applyEvents(QVariantMap{{"clear",true}}));
    CHECK(log->entryCount()==0);
    definition.properties.insert("maxEntries",10);
    factory.applyDefinition(log,definition);
    for(int i=0;i<15;i++) {
        CHECK(log->applyEvents(QVariantMap{{"append",QVariantMap{{"message",QString::number(i)}}}}));
    }
    CHECK(log->entryCount()==10);
    CHECK(log->messageAt(0)==QStringLiteral("5"));
    CHECK(log->messageAt(9)==QStringLiteral("14"));
    QVariantList valid{QVariantMap{{"level","ERROR"},{"message","Failure"}}};
    CHECK(log->applyEvents(valid));
    CHECK(log->entryCount()==1);
    CHECK(log->levelAt(0)==QStringLiteral("ERROR"));
    CHECK(log->applyEvents(QVariantMap{{"append",QVariantMap{{"message","=1+2"}}}}));
    CHECK(log->csvText().contains(QStringLiteral("'=1+2"))); // neutralized CSV formula
    // Saving the widget definition never persists the live log rows.
    PanelControl saved=definition;
    factory.readDefinition(log,saved);
    CHECK(!saved.properties.contains("entries"));
    CHECK(!saved.properties.contains("history"));
    qInfo()<<"PASS Event Log: JS conversion, filtering, validation, bounded history, CSV and transient data";
    return 0;
}
