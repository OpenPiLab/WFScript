// RC1.5.10: native editor schema, direct rule editing and resizable window.
#include "TestSequenceEditor.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
#include <QTableWidget>
#include <QTreeWidget>
#include <functional>
#include <QVector>
#include <QTabWidget>
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
    const QJsonObject rules{
        {QStringLiteral("vpp"),QJsonObject{{"low",1.7},{"high",2.3},{"unit","Vpp"},{"name","Amplitude"}}},
        {QStringLiteral("frequency"),QJsonObject{{"low",970},{"high",1030},{"unit","Hz"},{"name","Frequency"}}},
        {QStringLiteral("channels[0]"),QJsonObject{{"low",4.9},{"high",5.1},{"unit","V"},{"name","CH1"}}},
        {QStringLiteral("temperatures[*]"),QJsonObject{{"low",20},{"high",30},{"unit","C"},{"name","Temp"}}}
    };
    const QJsonObject step{{"id","wave"},{"name","Waveform"},{"group","main"},
       {"type","numeric"},{"mode","callback"},{"handler","waveform"},{"measurements",rules}};
    QJsonObject definition{{"schemaVersion",1},{"id","structured-ux"},
       {"title","Structured UI"},{"config",QJsonObject{}},{"steps",QJsonArray{step}}};
    QString error;
    assert(TestSequenceEditor::validateDefinition(definition,&error));
    TestSequenceEditor editor(definition,{},QStringList{QStringLiteral("waveform")},nullptr,{},
       QStringLiteral("Steps.mjs"),
       QStringLiteral("export function waveform(step){ TestRunner.complete(step.token,{vpp:2}); }\n"));
    assert(editor.windowFlags().testFlag(Qt::WindowMinMaxButtonsHint));
    assert(editor.minimumWidth()<=800);
    auto* tabs=editor.findChild<QTabWidget*>(QStringLiteral("testSequenceEditorTabs"));
    auto* table=editor.findChild<QTreeWidget*>(QStringLiteral("testSequenceStepTable"));
    auto* named=editor.findChild<QTableWidget*>(QStringLiteral("testSequenceNamedResultsTable"));
    auto* toggle=editor.findChild<QCheckBox*>(QStringLiteral("testSequenceStructuredResults"));
    auto* edit=editor.findChild<QPushButton*>(QStringLiteral("testSequenceOpenNamedResults"));
    auto* actions=editor.findChild<QDialogButtonBox*>(QStringLiteral("testSequenceEditorActions"));
    assert(tabs && tabs->count()==9 && table && named && toggle && edit && actions);
    assert(toggle->isChecked() && named->rowCount()==4);
    assert(authoredRows(table).at(0)->text(4).contains(QStringLiteral("4 named")));
    edit->click();
    assert(tabs->tabText(tabs->currentIndex())==QStringLiteral("Named Results & Limits"));
    for(int i=0;i<named->rowCount();++i) {
        if(named->item(i,0)->text()==QStringLiteral("vpp"))
            named->item(i,3)->setText(QStringLiteral("2.2"));
    }
    bool committed=false;
    editor.setSaveHandler([&](const QJsonObject& saved,const QJsonObject&,const QJsonObject&,const QString&){
        committed=true;
        const auto doc=saved.value(QStringLiteral("steps")).toArray().at(0).toObject();
        assert(doc.value(QStringLiteral("measurements")).toObject().value(QStringLiteral("vpp"))
            .toObject().value(QStringLiteral("high")).toDouble()==2.2);
        assert(!doc.contains(QStringLiteral("limits")));
        return true;
    });
    editor.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    editor.showNormal();
    editor.resize(800,550);
    QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    // The Save button is in a bottom bar, not beneath overflowing tab content.
    assert(actions->isVisible());
    assert(actions->geometry().bottom()<=editor.height());
    actions->button(QDialogButtonBox::Save)->click();
    assert(committed);
    return 0;
}
