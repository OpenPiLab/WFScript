// v5.6.62 -- native offscreen codec and editable visual dialog smoke.
#include "TestSequenceEditor.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>

static void insist(bool condition,const QString& message) {
    if(!condition)qFatal("TestSequenceEditor: %s",qPrintable(message));
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QString source=QStringLiteral(R"JS(/** Comment to preserve */
export const definition = {
 "schemaVersion":1,
 "id":"sample-one",
 "title":"Test 1",
 "config":{"stopOnFailure":true,"totalTimeoutMs":5000},
 "steps":[{"id":"init","name":"Settle","group":"setup","type":"action",
           "mode":"delay","durationMs":150},
          {"id":"check","name":"Check supply","group":"main","type":"test",
           "mode":"once","handler":"readSupply"}]
};
export function withSettle(ms) { return ms + 1; }
)JS");
    QJsonObject first;
    QString error;
    int from=0,to=0;
    insist(TestSequenceEditor::extractModuleDefinition(source,&first,&from,&to,&error),error);
    insist(first.value(QStringLiteral("steps")).toArray().size()==2,"Expected two steps");
    insist(TestSequenceEditor::validateDefinition(first,&error),error);
    QJsonObject changed=first;
    QJsonArray steps=changed.value(QStringLiteral("steps")).toArray();
    QJsonObject init=steps.at(0).toObject();init.insert(QStringLiteral("durationMs"),350);
    steps.replace(0,init);changed.insert(QStringLiteral("steps"),steps);
    QString replaced;
    insist(TestSequenceEditor::replaceModuleDefinition(source,changed,&replaced,&error),error);
    insist(replaced.contains(QStringLiteral("export function withSettle(ms) { return ms + 1; }")),"Lost trailing library code");
    insist(replaced.startsWith(QStringLiteral("/** Comment to preserve */")),"Lost original JSDoc");
    QJsonObject roundtrip;
    insist(TestSequenceEditor::extractModuleDefinition(replaced,&roundtrip,nullptr,nullptr,&error),error);
    insist(roundtrip==changed,"Roundtrip changed the definition");
    QJsonObject invalid=roundtrip;
    steps=invalid.value(QStringLiteral("steps")).toArray();
    QJsonObject second=steps.at(1).toObject();second.insert(QStringLiteral("id"),QStringLiteral("init"));
    steps.replace(1,second);invalid.insert(QStringLiteral("steps"),steps);
    insist(!TestSequenceEditor::validateDefinition(invalid,&error),"Duplicate ID must be rejected");
    QString untouched=QStringLiteral("unchanged");
    insist(!TestSequenceEditor::replaceModuleDefinition(replaced,invalid,&untouched,&error),"Invalid definition accepted");
    insist(untouched==QStringLiteral("unchanged"),"Rejected replace mutated the buffer");
    QJsonObject ignored;
    insist(!TestSequenceEditor::extractModuleDefinition(QStringLiteral("export const definition = buildTest();"),&ignored,nullptr,nullptr,&error),
           "Expression must not be evaluated");
    TestSequenceEditor dialog(changed,QJsonObject{}, {QStringLiteral("readSupply")});
    auto* table=dialog.findChild<QTableWidget*>(QStringLiteral("testSequenceStepTable"));
    auto* add=dialog.findChild<QPushButton*>(QStringLiteral("addTestStepButton"));
    auto* up=dialog.findChild<QPushButton*>(QStringLiteral("moveTestStepUpButton"));
    auto* group=dialog.findChild<QComboBox*>(QStringLiteral("testSequenceGroupCombo"));
    auto* apply=dialog.findChild<QPushButton*>(QStringLiteral("applyTestStepButton"));
    insist(table && add && up && group && apply,"Missing visual editor controls");
    insist(table->rowCount()==2,"Initial two steps not shown");
    // Adding from Setup must place a new Main step after existing Main steps,
    // never below Cleanup. Move Up must work within Main immediately.
    add->click();
    insist(table->rowCount()==3,"Add step did not insert");
    insist(table->item(2,0)->text()==QStringLiteral("main"),"Added step should belong to Main");
    insist(table->item(2,1)->text()==QStringLiteral("New test step"),"Missing new Main step");
    up->click();
    insist(table->item(1,1)->text()==QStringLiteral("New test step"),"Cannot reorder newly added Main step");
    // Changing the group must relocate the row to the proper group boundary.
    group->setCurrentText(QStringLiteral("cleanup"));
    apply->click();
    insist(table->item(2,0)->text()==QStringLiteral("cleanup"),"Group changes must relocate steps");
    qInfo() << "PASS v5.6.62.2 visual Test Sequence editor: strict JSON, safe replacement, group-aware add/move and group change";
    return 0;
}
