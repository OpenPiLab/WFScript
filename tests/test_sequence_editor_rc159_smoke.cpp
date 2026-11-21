// RC1.5.9: the real ScriptEditor is reused by the project test editor,
// and the native data-only schema admits JSON parameters and callback mode.
#include "TestSequenceEditor.h"
#include "ScriptEditor.h"
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <functional>
#include <QVector>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
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
    QJsonObject step{{"id","v5"},{"name","Read supply"},{"group","main"},
        {"mode","callback"},{"type","numeric"},{"handler","readVoltage"},
        {"parameters",QJsonObject{{"voltage",5},{"channel",1}}},
        {"timeoutMs",10000},{"limits",QJsonObject{{"low",4.9},{"high",5.1}}},
        {"unit","V"}};
    QJsonObject def{{"schemaVersion",1},{"id","callback-demo"},
        {"title","Callback demo"},{"config",QJsonObject{}},
        {"steps",QJsonArray{step}}};
    QString error;
    assert(TestSequenceEditor::validateDefinition(def,&error));
    TestSequenceEditor editor(def,{}, {"readVoltage"},nullptr,{},
        QStringLiteral("CallbackSteps.mjs"),
        QStringLiteral("export function readVoltage(step) {\n  TestRunner.complete(step.token, 5);\n}\n"));
    auto* source=editor.findChild<ScriptEditor*>("testSequenceLibrarySourceEditor");
    auto* raw=editor.findChild<QPlainTextEdit*>("testSequenceParametersJson");
    auto* mode=editor.findChild<QComboBox*>();
    auto* tabs=editor.findChild<QTabWidget*>("testSequenceEditorTabs");
    auto* table=editor.findChild<QTreeWidget*>("testSequenceStepTable");
    auto* onceEdit=editor.findChild<QPushButton*>("testSequenceOpenOnceCode");
    assert(source && raw && mode && tabs && table && onceEdit);
    assert(source->toPlainText().contains(QStringLiteral("readVoltage(step)")));
    assert(authoredRows(table).at(0)->text(1)==QStringLiteral("readVoltage()"));
    assert(raw->toPlainText().contains(QStringLiteral("voltage")));
    // Already defined handlers navigate into the same ScriptEditor class.
    onceEdit->click();
    assert(tabs->currentIndex()==6);
    assert(source->textCursor().position()==0);
    tabs->setCurrentIndex(0);
    raw->setPlainText(QStringLiteral("{\"voltage\": 4.75, \"channel\": 1}"));
    auto* actions=editor.findChild<QDialogButtonBox*>("testSequenceEditorActions");
    assert(actions);
    bool saved=false;
    editor.setSaveHandler([&](const QJsonObject& definition,const QJsonObject&,
                              const QJsonObject&,const QString&){
        saved=true;
        const auto a=definition.value(QStringLiteral("steps")).toArray();
        assert(a.size()==1);
        assert(a.at(0).toObject().value(QStringLiteral("parameters")).toObject()
                   .value(QStringLiteral("voltage")).toDouble()==4.75);
        return true;
    });
    actions->button(QDialogButtonBox::Save)->click();
    assert(saved);
    return 0;
}
