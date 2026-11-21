// RC1.5.14.1: insert Sweep/Repeat directly under IF/ELSE, including an IF
// nested inside an outer Sweep. The edit tree must match the native expander.
#include "TestSequenceEditor.h"
#include "TestSequenceLoops.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTreeWidget>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QTreeWidgetItem* findStep(QTreeWidget* tree,const QString& id) {
    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> walk=[&](QTreeWidgetItem* item)->QTreeWidgetItem* {
        if(item->text(6)==id)return item;
        for(int i=0;i<item->childCount();++i)
            if(auto* found=walk(item->child(i)))return found;
        return nullptr;
    };
    for(int i=0;i<tree->topLevelItemCount();++i)
        if(auto* found=walk(tree->topLevelItem(i)))return found;
    return nullptr;
}
static void select(QTreeWidget* tree,QTreeWidgetItem* item) {
    assert(item);tree->setCurrentItem(item);QCoreApplication::processEvents();
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QJsonObject definition=QJsonDocument::fromJson(R"JSON(
    {"schemaVersion":1,"id":"nested-editor-test","title":"Nested conditional sweep editor",
     "config":{},"variables":[{"name":"temperature","type":"number","initial":48.5}],
     "steps":[
       {"id":"setup","name":"Setup","group":"setup","type":"action","mode":"once","handler":"setup"},
       {"id":"top_if","name":"IF temperature > 40","group":"main",
        "condition":{"source":"variable","key":"temperature","operator":">","value":40},
        "steps":[
          {"id":"top_then","name":"Top THEN","type":"action","mode":"once","handler":"thenAction"},
          {"id":"top_else","name":"Top ELSE","otherwise":true,"steps":[
            {"id":"top_else_action","name":"Top else action","type":"action","mode":"once","handler":"elseAction"}
          ]}
        ]},
       {"id":"outer_sweep","name":"Outer voltage sweep","group":"main",
        "loop":{"parameter":"voltage","start":5,"stop":4.75,"increment":-0.25},
        "steps":[
          {"id":"nested_if","name":"IF voltage > 4.8",
           "condition":{"source":"parameter","key":"voltage","operator":">","value":4.8},
           "steps":[
             {"id":"nested_then","name":"Nested THEN","type":"action","mode":"once","handler":"thenAction"},
             {"id":"nested_else","name":"Nested ELSE","otherwise":true,"steps":[
               {"id":"nested_else_action","name":"Nested else action","type":"action","mode":"once","handler":"elseAction"}
             ]}
           ]}
        ]},
       {"id":"cleanup","name":"Cleanup","group":"cleanup","type":"action","mode":"once","handler":"cleanup"}
     ]}
    )JSON").object();
    QString error;
    assert(TestSequenceEditor::validateDefinition(definition,&error));
    TestSequenceEditor editor(definition,{},QStringList{
        QStringLiteral("setup"),QStringLiteral("thenAction"),QStringLiteral("elseAction"),QStringLiteral("cleanup")});
    auto* tree=editor.findChild<QTreeWidget*>(QStringLiteral("testSequenceStepTable"));
    auto* sweep=editor.findChild<QPushButton*>(QStringLiteral("addTestSweepButton"));
    auto* repeat=editor.findChild<QPushButton*>(QStringLiteral("addTestRepeatButton"));
    auto* actions=editor.findChild<QDialogButtonBox*>(QStringLiteral("testSequenceEditorActions"));
    assert(tree && sweep && repeat && actions);

    // Selecting IF creates a child in THEN, BEFORE the terminal ELSE.
    select(tree,findStep(tree,QStringLiteral("top_if")));
    sweep->click();QCoreApplication::processEvents();
    auto* topIf=findStep(tree,QStringLiteral("top_if"));
    assert(topIf && topIf->childCount()==3);
    assert(topIf->child(1)->text(2)==QStringLiteral("Sweep"));
    assert(topIf->child(2)->text(6)==QStringLiteral("top_else"));

    // Selecting ELSE adds a Repeat INSIDE ELSE, not at the top level.
    select(tree,findStep(tree,QStringLiteral("top_else")));
    repeat->click();QCoreApplication::processEvents();
    auto* topElse=findStep(tree,QStringLiteral("top_else"));
    assert(topElse && topElse->childCount()==2);
    assert(topElse->child(1)->text(2)==QStringLiteral("Repeat"));

    // The screenshot's more deeply nested case: an ELSE inside a Sweep's IF.
    select(tree,findStep(tree,QStringLiteral("nested_else")));
    sweep->click();QCoreApplication::processEvents();
    auto* nestedElse=findStep(tree,QStringLiteral("nested_else"));
    assert(nestedElse && nestedElse->childCount()==2);
    assert(nestedElse->child(1)->text(2)==QStringLiteral("Sweep"));

    // All three newly inserted parent nodes have their own example children,
    // with no duplicate IDs or changed existing IF/ELSE ordering.
    bool saved=false;
    editor.setSaveHandler([&](const QJsonObject& out,const QJsonObject&,const QJsonObject&,const QString&){
        saved=true;
        QJsonObject expanded;QString reason;
        assert(TestSequenceLoops::expand(out,&expanded,&reason));
        const QJsonArray rows=expanded.value(QStringLiteral("steps")).toArray();
        assert(rows.size()>12 && rows.size()<100);
        bool foundInherited=false,foundGuard=false;
        for(const auto& value:rows) {
            const QJsonObject row=value.toObject();
            const QJsonObject params=row.value(QStringLiteral("parameters")).toObject();
            if(params.contains(QStringLiteral("voltage")) && params.contains(QStringLiteral("value"))) {
                foundInherited=true;
                if(!row.value(QStringLiteral("guards")).toArray().isEmpty())foundGuard=true;
            }
        }
        assert(foundInherited && foundGuard);
        const auto testSteps=out.value(QStringLiteral("steps")).toArray();
        assert(testSteps.at(1).toObject().value(QStringLiteral("steps")).toArray().last().toObject()
            .value(QStringLiteral("otherwise")).toBool());
        return true;
    });
    actions->button(QDialogButtonBox::Save)->click();
    assert(saved);
    return 0;
}
