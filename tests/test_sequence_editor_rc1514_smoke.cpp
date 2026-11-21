// RC1.5.14: inline IF/ELSE editor, variable picker, and preview smoke.
#include "TestSequenceEditor.h"
#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPushButton>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTabWidget>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QJsonObject definition=QJsonDocument::fromJson(R"JSON(
{
 "schemaVersion":1,"id":"condition-editor-test","title":"Condition Editor",
 "config":{},
 "variables":[{"name":"temperature","type":"number","initial":48.5}],
 "steps":[
   {"id":"setup","name":"Setup","group":"setup","type":"action","mode":"once","handler":"setup"},
   {"id":"check","name":"IF temperature > 40","group":"main",
    "condition":{"source":"variable","key":"temperature","operator":">","value":40},
    "steps":[
      {"id":"yes","name":"THEN","type":"action","mode":"once","handler":"yes"},
      {"id":"otherwise","name":"ELSE","otherwise":true,"steps":[
        {"id":"no","name":"ELSE action","type":"action","mode":"once","handler":"no"}
      ]}
    ]},
   {"id":"cleanup","name":"Cleanup","group":"cleanup","type":"action","mode":"once","handler":"cleanup"}
 ]
}
)JSON").object();
    QString error;
    assert(TestSequenceEditor::validateDefinition(definition,&error));
    TestSequenceEditor editor(definition,{},QStringList{QStringLiteral("setup"),QStringLiteral("yes"),
        QStringLiteral("no"),QStringLiteral("cleanup")});
    auto* tree=editor.findChild<QTreeWidget*>(QStringLiteral("testSequenceStepTable"));
    auto* variableTable=editor.findChild<QTableWidget*>(QStringLiteral("testSequenceVariableCatalog"));
    auto* source=editor.findChild<QComboBox*>(QStringLiteral("testSequenceConditionSource"));
    auto* key=editor.findChild<QComboBox*>(QStringLiteral("testSequenceConditionKey"));
    auto* tabs=editor.findChild<QTabWidget*>(QStringLiteral("testSequenceEditorTabs"));
    auto* preview=editor.findChild<QPushButton*>(QStringLiteral("testSequenceOpenExecutionPreview"));
    auto* result=editor.findChild<QTableWidget*>(QStringLiteral("testSequenceExecutionPreviewTable"));
    auto* actions=editor.findChild<QDialogButtonBox*>(QStringLiteral("testSequenceEditorActions"));
    assert(tree && variableTable && source && key && tabs && preview && result && actions);
    assert(tabs->count()==9 && variableTable->rowCount()==1);
    assert(variableTable->item(0,0)->text()==QStringLiteral("temperature"));
    auto* condition=tree->topLevelItem(1)->child(0);
    assert(condition && condition->childCount()==2);
    tree->setCurrentItem(condition);QCoreApplication::processEvents();
    assert(source->currentIndex()==0);
    assert(key->currentText()==QStringLiteral("temperature"));
    preview->click();
    auto* scope=editor.findChild<QComboBox*>(QStringLiteral("testSequencePreviewScope"));
    assert(scope);
    scope->setCurrentIndex(0);
    assert(result->rowCount()==5); // Setup, IF, THEN, ELSE, Cleanup
    assert(result->item(1,5)->text().contains(QStringLiteral("runtime choice")));
    bool saved=false;
    editor.setSaveHandler([&](const QJsonObject& out,const QJsonObject&,const QJsonObject&,const QString&){
        saved=true;
        assert(out.value(QStringLiteral("steps"))==definition.value(QStringLiteral("steps")));
        assert(out.value(QStringLiteral("variables"))==definition.value(QStringLiteral("variables")));
        return true;
    });
    actions->button(QDialogButtonBox::Save)->click();assert(saved);
    return 0;
}
