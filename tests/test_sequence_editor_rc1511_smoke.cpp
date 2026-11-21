// RC1.5.11: edit and save a compact bounded sweep in the modeless Qt editor.
#include "TestSequenceEditor.h"
#include "TestSequenceLoops.h"
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <functional>
#include <QVector>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QEventLoop>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>


static QVector<QTreeWidgetItem*> authoredRows(QTreeWidget* table){
    QVector<QTreeWidgetItem*> result;
    std::function<void(QTreeWidgetItem*)> descend=[&](QTreeWidgetItem* p){
        for(int i=0;i<p->childCount();++i){
            QTreeWidgetItem* child=p->child(i);
            if(!child->data(0,Qt::UserRole).toString().isEmpty())result.append(child);
            descend(child);
        }
    };
    for(int i=0;i<table->topLevelItemCount();++i)descend(table->topLevelItem(i));
    return result;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QJsonObject child{
        {"id","measurement"},{"name","Read ${voltage} V"},{"type","numeric"},
        {"mode","callback"},{"handler","readVoltage"},
        {"limits",QJsonObject{{"low",QJsonObject{{"param","voltage"},{"offset",-0.1}}},
                               {"high",QJsonObject{{"param","voltage"},{"offset",0.1}}}}}
    };
    const QJsonObject group{
        {"id","sweep"},{"name","Voltage sweep"},{"group","main"},
        {"loop",QJsonObject{{"parameter","voltage"},{"start",5},{"stop",4.5},{"increment",-0.25}}},
        {"steps",QJsonArray{child}}
    };
    const QJsonObject definition{
        {"schemaVersion",1},{"id","editor-loop"},{"title","Sweep editor"},
        {"config",QJsonObject{}},
        {"steps",QJsonArray{QJsonObject{{"id","setup"},{"name","Setup"},{"group","setup"},
                                      {"type","action"},{"mode","once"},{"handler","ready"}},group}}
    };
    QString error;
    assert(TestSequenceEditor::validateDefinition(definition,&error));
    TestSequenceEditor editor(definition,{},QStringList{QStringLiteral("ready"),QStringLiteral("readVoltage")},
        nullptr,{},QStringLiteral("TestSteps.mjs"),
        QStringLiteral("export function ready(){return true;}\nexport function readVoltage(step){TestRunner.complete(step.token,step.params.voltage);}\n"));
    auto* tabs=editor.findChild<QTabWidget*>(QStringLiteral("testSequenceEditorTabs"));
    auto* table=editor.findChild<QTreeWidget*>(QStringLiteral("testSequenceStepTable"));
    auto* axes=editor.findChild<QTableWidget*>(QStringLiteral("testSequenceSweepAxes"));
    auto* actions=editor.findChild<QDialogButtonBox*>(QStringLiteral("testSequenceEditorActions"));
    assert(tabs&&tabs->count()==9&&table&&axes&&actions);
    assert(authoredRows(table).size()==3); // Setup, Sweep, one child
    auto* groupItem=authoredRows(table).at(1);
    assert(groupItem->text(0)==QStringLiteral("Voltage sweep"));
    assert(groupItem->childCount()==1);
    table->setCurrentItem(groupItem);
    assert(axes->rowCount()==1);
    assert(axes->item(0,4)->text()==QStringLiteral("-0.25"));
    editor.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    axes->item(0,4)->setText(QStringLiteral("-0.5"));
    bool saved=false;
    editor.setSaveHandler([&](const QJsonObject& doc,const QJsonObject&,const QJsonObject&,const QString&){
        saved=true;
        assert(doc.value(QStringLiteral("steps")).toArray().size()==2); // compact group preserved
        const auto loop=doc.value(QStringLiteral("steps")).toArray().at(1).toObject();
        assert(loop.value(QStringLiteral("loop")).toObject().value(QStringLiteral("increment")).toDouble()==-0.5);
        QJsonObject expanded;QString message;
        assert(TestSequenceLoops::expand(doc,&expanded,&message));
        assert(expanded.value(QStringLiteral("steps")).toArray().size()==3);
        return true;
    });
    editor.show();QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    assert(editor.windowFlags().testFlag(Qt::WindowMinMaxButtonsHint));
    actions->button(QDialogButtonBox::Save)->click();
    assert(saved);
    return 0;
}
