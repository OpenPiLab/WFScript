// RC1.5.8: offscreen contract for three-column step edit, full-width table,
// inline project settings, in-form JS source navigation, and atomic save.
#include "TestSequenceEditor.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QJsonArray>
#include <QDoubleSpinBox>
#include <QDir>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <functional>
#include <QVector>
#include <QComboBox>
#include <QDebug>

static void check(bool condition,const char* reason){
    if(!condition)qFatal("RC1.5.8 editor: %s",reason);
}

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
int main(int argc,char** argv){
    QApplication app(argc,argv);
    QJsonArray steps{
        QJsonObject{{"id","supply"},{"name","Read PSU"},{"description","Set and measure PSU CH1"},
                    {"group","main"},{"type","numeric"},{"mode","poll"},
                    {"intervalMs",80},{"timeoutMs",12000},
                    {"handlers",QJsonObject{{"start","power500"},{"poll","pollValue"}}},
                    {"limits",QJsonObject{{"low",4.9},{"high",5.1}}},{"unit","V"}},
        QJsonObject{{"id","cleanup"},{"name","PSU off"},{"group","cleanup"},
                    {"type","action"},{"mode","once"},{"handler","cleanupPsu"}}
    };
    const QJsonObject original{{"schemaVersion",1},{"id","rigol-test"},
        {"title","Rigol Test"},{"config",QJsonObject{{"stopOnFailure",true},{"totalTimeoutMs",50000}}},
        {"steps",steps}};
    const QJsonObject runInfo{{"fields",QJsonArray{
        QJsonObject{{"id","uut"},{"label","Unit"},{"type","text"},{"required",true},{"default",""}}}}};
    const QString source=QStringLiteral("export function power500() { return 5; }\n"
        "export function pollValue() { return 4.999; }\n"
        "export function cleanupPsu() { return true; }\n");
    TestSequenceEditor editor(original,runInfo,{"power500","pollValue","cleanupPsu"},
                              nullptr,QJsonObject{{"autoSave",true},{"directory",""}},
                              "RigolTestSteps.mjs",source);
    check(editor.windowModality()==Qt::NonModal,"Editor must be modeless");
    auto* tabs=editor.findChild<QTabWidget*>("testSequenceEditorTabs");
    auto* table=editor.findChild<QTreeWidget*>("testSequenceStepTable");
    auto* columns=editor.findChild<QWidget*>("testSequenceDetailsColumns");
    auto* controls=editor.findChild<QGroupBox*>("testSequenceExecutionGroup");
    auto* start=editor.findChild<QComboBox*>("testSequenceStartHandler");
    auto* poll=editor.findChild<QComboBox*>("testSequencePollHandler");
    auto* open=editor.findChild<QPushButton*>("testSequenceOpenProcedureCode");
    auto* library=editor.findChild<QPlainTextEdit*>("testSequenceLibrarySourceEditor");
    auto* report=editor.findChild<QLineEdit*>("testSequenceReportDirectory");
    auto* low=editor.findChild<QDoubleSpinBox*>("testSequenceLimitLow");
    check(tabs && table && columns && controls && start && poll && open && library && report && low,
        "Missing layout or JS edit controls");
    check(tabs->count()==9 && tabs->tabText(0)=="Step Details","Tab hierarchy incorrect");
    check(table->parentWidget()==tabs->widget(0),"Full-width step table should be under details, not in a left pane");
    check(authoredRows(table).size()==2 && table->columnCount()==7,"Step table changed unexpectedly");
    check(authoredRows(table).at(0)->text(1)=="power500()","Start procedure must be displayed");
    check(start->currentText()=="power500" && poll->currentText()=="pollValue",
        "Start and poll handlers must both be immediately visible");
    check(!library->isReadOnly() && library->toPlainText()==source,"Embedded library source not editable");
    open->click();
    check(tabs->currentWidget()==library->parentWidget(),
          "Open procedure did not switch to JS source tab");
    check(library->textCursor().position()==0,"Open procedure did not find named function");
    library->appendPlainText(QStringLiteral("// author edit"));
    tabs->setCurrentIndex(0);
    low->setValue(4.85);
    // The UUT editor is embedded in its own tab and its fields are saved with
    // the sequence rather than through an unrelated popup.
    auto* runTable=editor.findChild<QTableWidget*>("runInformationFieldsTable");
    check(runTable && runTable->rowCount()==1,"Inline UUT fields are missing");
    auto* label=runTable->item(0,1);
    check(label!=nullptr,"Missing editable UUT field");
    label->setText(QStringLiteral("Unit serial"));
    const QString archiveDir=QDir::tempPath()+QStringLiteral("/wfscript-test-reports");
    report->setText(archiveDir);
    bool saved=false;
    editor.setSaveHandler([&](const QJsonObject& def,const QJsonObject& ri,const QJsonObject& policy,const QString& src){
        saved=true;
        check(def.value("steps").toArray().at(0).toObject().value("limits").toObject().value("low").toDouble()==4.85,
              "Edited numeric limit was not carried to the save callback");
        check(def.value("steps").toArray().at(0).toObject().value("handlers").toObject().value("poll").toString()=="pollValue",
              "Completion binding was lost");
        check(ri.value("fields").toArray().at(0).toObject().value("label").toString()=="Unit serial",
              "Edited Run Information was not included");
        check(policy.value("directory").toString()==archiveDir,"Report storage missing");
        check(src.contains("// author edit"),"JS source edit was not included");
        return true;
    });
    auto* buttons=editor.findChild<QDialogButtonBox*>("testSequenceEditorActions");
    check(buttons && buttons->button(QDialogButtonBox::Save),"Missing save action");
    buttons->button(QDialogButtonBox::Save)->click();
    check(saved && editor.result()==QDialog::Accepted,"Atomic save path failed");
    QString error;
    check(TestSequenceEditor::validateDefinition(editor.definition(),&error),"Saved schema invalid");
    qInfo()<<"PASS RC1.5.8: modeless, 3-column full-width table, in-form JS navigation/edit, settings, atomic save";
    return 0;
}
