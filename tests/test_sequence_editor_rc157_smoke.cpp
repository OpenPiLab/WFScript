// RC1.5.7 — offscreen UI checks for the Test Sequence Editor's readable layout.
// Preserves the schemaVersion:1 definition and all start/poll/finish bindings.
#include "TestSequenceEditor.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <functional>
#include <QVector>
#include <QToolButton>
#include <QDebug>

static void require(bool ok,const char* message) {
    if(!ok)qFatal("RC1.5.7 TestSequenceEditor: %s",message);
}
static QJsonObject sampleDefinition() {
    QJsonArray steps;
    steps.append(QJsonObject{
        {"id","preflight"},{"name","Verify four connections"},{"group","setup"},
        {"type","action"},{"mode","poll"},{"intervalMs",80},{"timeoutMs",40000},
        {"handlers",QJsonObject{{"start","preflight"},{"poll","pollPreflight"}}},
        {"description","Verify all outputs are OFF before doing anything."}
    });
    for(int i=0;i<26;++i) {
        const bool dmm=(i%2==1);
        const int setMv=5000-(i/2)*250;
        const QString name=dmm?QStringLiteral("Read DMM at %1 V").arg(setMv/1000.0):
                               QStringLiteral("PSU CH1 at %1 V").arg(setMv/1000.0);
        const QString start=(dmm?QStringLiteral("dmm"):QStringLiteral("power"))+QString::number(setMv);
        steps.append(QJsonObject{
            {"id",QStringLiteral("step_%1").arg(i)},{"name",name},{"group","main"},
            {"type","numeric"},{"mode","poll"},{"intervalMs",80},{"timeoutMs",35000},
            {"handlers",QJsonObject{{"start",start},{"poll","pollValue"}}},
            {"limits",QJsonObject{{"low",setMv/1000.0-0.1},{"high",setMv/1000.0+0.1}}},
            {"unit","V"},
            {"description",QStringLiteral("Read actual voltage for setpoint %1 V.").arg(setMv/1000.0)}
        });
    }
    for(int i=0;i<32;++i) {
        const bool isCapture=i%4==0;
        const bool isOnce=!isCapture;
        QJsonObject step{
            {"id",QStringLiteral("wave_%1").arg(i)},
            {"name",QStringLiteral("Capture or verify wave %1").arg(i)},
            {"group","main"},{"type",isCapture?"action":"numeric"},
            {"mode",isOnce?"once":"poll"}
        };
        if(isCapture) {
            step.insert(QStringLiteral("intervalMs"),80);
            step.insert(QStringLiteral("handlers"),QJsonObject{
                {"start",QStringLiteral("captureWave%1").arg(i)},
                {"poll","pollDone"}});
        } else {
            step.insert(QStringLiteral("handler"),QStringLiteral("measureWave%1").arg(i));
            step.insert(QStringLiteral("limits"),QJsonObject{{"low",0.5},{"high",1.5}});
            step.insert(QStringLiteral("unit"),QStringLiteral("Vpp"));
        }
        steps.append(step);
    }
    steps.append(QJsonObject{{"id","offgen"},{"name","Generator OFF"},{"group","cleanup"},
        {"type","action"},{"mode","poll"},{"intervalMs",80},
        {"handlers",QJsonObject{{"start","cleanupGen"},{"poll","pollDone"}}}});
    steps.append(QJsonObject{{"id","offpsu"},{"name","PSU OFF"},{"group","cleanup"},
        {"type","action"},{"mode","poll"},{"intervalMs",80},
        {"handlers",QJsonObject{{"start","cleanupPsu"},{"poll","pollDone"}}}});
    return QJsonObject{
        {"schemaVersion",1},{"id","rigol-loopback-065"},
        {"title","Four-Instrument Voltage and Waveform Verification"},
        {"config",QJsonObject{{"stopOnFailure",false},{"totalTimeoutMs",480000}}},
        {"steps",steps}
    };
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
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QJsonObject original=sampleDefinition();
    QString error;
    require(TestSequenceEditor::validateDefinition(original,&error),"Fixture failed schema validation");
    const QStringList exports={"preflight","pollPreflight","power5000","dmm5000","pollValue", "cleanupGen","cleanupPsu","pollDone"};
    TestSequenceEditor editor(original,{},exports);
    auto* table=editor.findChild<QTreeWidget*>("testSequenceStepTable");
    auto* tabs=editor.findChild<QTabWidget*>("testSequenceEditorTabs");
    auto* execution=editor.findChild<QGroupBox*>("testSequenceExecutionGroup");
    auto* columns=editor.findChild<QWidget*>("testSequenceDetailsColumns");
    auto* counts=editor.findChild<QLabel*>("testSequenceStepCounts");
    auto* start=editor.findChild<QComboBox*>("testSequenceStartHandler");
    auto* poll=editor.findChild<QComboBox*>("testSequencePollHandler");
    auto* details=editor.findChild<QPlainTextEdit*>("testSequenceStepDescription");
    auto* low=editor.findChild<QDoubleSpinBox*>("testSequenceLimitLow");
    auto* apply=editor.findChild<QPushButton*>("applyTestStepButton");
    auto* openJson=editor.findChild<QPushButton*>("showTestDefinitionJsonButton");
    require(table && tabs && execution && columns && counts && start && poll && details && low && apply && openJson,
            "Missing editor UI controls");
    require(authoredRows(table).size()==61,"Expected 61 fixture steps"); // 1 + 26 + 32 + 2
    require(table->columnCount()==7,"Readable columns missing");
    require(table->headerItem()->text(1).contains("Procedure"),"Procedure heading missing");
    require(authoredRows(table).at(1)->text(1)==QStringLiteral("power5000()"),"On-start procedure must be prominent");
    require(authoredRows(table).at(1)->text(1)!=QStringLiteral("pollValue"),"Poll implementation leaked into the procedure column");
    require(authoredRows(table).at(1)->text(4).contains("V"),"Measurement limits not visible");
    require(authoredRows(table).at(1)->text(5).contains("actual voltage"),"Description column missing");
    require(authoredRows(table).at(1)->toolTip(1).contains("pollValue"),"Polling details should remain available in tooltip");
    require(authoredRows(table).at(27)->text(1).startsWith("captureWave"),"Waveform capture procedure missing");
    require(authoredRows(table).at(28)->text(1).startsWith("measureWave"),"Once handler not shown");
    require(counts->text().contains("61 executable steps"),"Group counts missing");
    require(tabs->currentIndex()==0,"JSON should not replace the main editing view");
    require(tabs->tabText(0)==QStringLiteral("Step Details"),"Step Details tab should be the default");
    require(tabs->count()==9,"Missing run info, report, settings, JS, or JSON tabs");
    require(execution->parentWidget()==columns,"Execution controls must be in the top details columns");
    require(editor.windowTitle().contains("Visual Test Sequence Editor"),
            "Window must identify the visual test editor");

    table->setCurrentItem(authoredRows(table).at(1),0);
    require(start->currentText()==QStringLiteral("power5000"),"Selected start handler not loaded");
    require(poll->currentText()==QStringLiteral("pollValue"),"Selected completion handler not retained");
    require(low->value()==4.9,"Numeric lower limit not loaded");
    details->setPlainText(QStringLiteral("Read supply back after settling."));
    low->setValue(4.85);
    apply->click();
    require(authoredRows(table).at(1)->text(4).contains("4.85"),"Edited numeric limit not shown in table");
    require(authoredRows(table).at(1)->text(5).contains("settling"),"Edited description not shown in table");
    require(authoredRows(table).at(1)->text(1)==QStringLiteral("power5000()"),"Editing details lost the start handler");
    openJson->click();
    require(tabs->tabText(tabs->currentIndex()).contains(QStringLiteral("JSON Preview")),
            "JSON Preview did not switch to separate tab");
    auto* preview=editor.findChild<QPlainTextEdit*>("testDefinitionJsonPreview");
    require(preview && preview->toPlainText().contains("power5000"),"JSON preview lost original handler names");
    require(preview->toPlainText().contains("4.85"),"JSON preview does not include edited limits");
    require(preview->isReadOnly(),"JSON preview must be read only");
    auto* actions=editor.findChild<QDialogButtonBox*>("testSequenceEditorActions");
    require(actions && actions->button(QDialogButtonBox::Save),"Save action missing");
    actions->button(QDialogButtonBox::Save)->click();
    require(editor.result()==QDialog::Accepted,"Could not accept definition");
    const auto saved=editor.definition();
    require(TestSequenceEditor::validateDefinition(saved,&error),"Saved definition invalid");
    const auto savedSteps=saved.value("steps").toArray();
    require(savedSteps.size()==61,"Save changed step count");
    const auto step=savedSteps.at(1).toObject();
    require(step.value("handlers").toObject().value("start").toString()==QStringLiteral("power5000"),"Save changed start binding");
    require(step.value("handlers").toObject().value("poll").toString()==QStringLiteral("pollValue"),"Save changed polling binding");
    require(step.value("limits").toObject().value("low").toDouble()==4.85,"Save lost numeric edit");
    require(savedSteps.at(27).toObject()==original.value("steps").toArray().at(27).toObject(),"Unedited waveform step mutated");
    qInfo() << "PASS RC1.5.8 editor compatibility: full-size layout, start procedures, limits, description, JSON tab, native schema roundtrip";
    return 0;
}
