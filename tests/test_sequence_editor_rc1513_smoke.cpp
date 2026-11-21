// RC1.5.13: actual-expander, read-only execution preview with repetition,
// inherited parameters, resolved limits, selection filtering and no project edits.
#include "TestSequenceEditor.h"
#include "TestSequenceLoops.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTabWidget>
#include <QCoreApplication>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QJsonObject sample() {
    const QJsonObject measure{
        {"id","readPsu"},{"name","Read ${voltage} V, pass ${iteration}"},
        {"type","numeric"},{"mode","callback"},{"handler","readVoltage"},
        {"limits",QJsonObject{
            {"low",QJsonObject{{"param","voltage"},{"offset",-0.1}}},
            {"high",QJsonObject{{"param","voltage"},{"offset",0.1}}}}}
    };
    return QJsonObject{
        {"schemaVersion",1},{"id","preview-test"},{"title","Execution preview test"},
        {"config",QJsonObject{}},{"steps",QJsonArray{
            QJsonObject{{"id","setup"},{"name","Connect"},{"group","setup"},
                        {"type","action"},{"mode","once"},{"handler","setup"}},
            QJsonObject{{"id","voltageSweep"},{"name","Voltage sweep"},{"group","main"},
                        {"loop",QJsonObject{{"parameter","voltage"},{"start",5.0},
                                            {"stop",4.5},{"increment",-0.25}}},
                        {"repeat",QJsonObject{{"count",3},{"index","iteration"}}},
                        {"steps",QJsonArray{measure}}},
            QJsonObject{{"id","cleanup"},{"name","Release"},{"group","cleanup"},
                        {"type","action"},{"mode","once"},{"handler","cleanup"}}
        }}
    };
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const QJsonObject original=sample();
    QString error;
    assert(TestSequenceEditor::validateDefinition(original,&error));
    TestSequenceEditor editor(original,{},QStringList{"setup","readVoltage","cleanup"});
    auto* authored=editor.findChild<QTreeWidget*>(QStringLiteral("testSequenceStepTable"));
    auto* previewButton=editor.findChild<QPushButton*>(QStringLiteral("testSequenceOpenExecutionPreview"));
    auto* resultTable=editor.findChild<QTableWidget*>(QStringLiteral("testSequenceExecutionPreviewTable"));
    auto* scope=editor.findChild<QComboBox*>(QStringLiteral("testSequencePreviewScope"));
    auto* summary=editor.findChild<QLabel*>(QStringLiteral("testSequenceExecutionPreviewSummary"));
    auto* tabs=editor.findChild<QTabWidget*>(QStringLiteral("testSequenceEditorTabs"));
    auto* actions=editor.findChild<QDialogButtonBox*>(QStringLiteral("testSequenceEditorActions"));
    assert(authored && previewButton && resultTable && scope && summary && tabs && actions);
    assert(tabs->count()==9);
    assert(tabs->tabText(1)==QStringLiteral("Execution Preview"));
    assert(resultTable->editTriggers()==QAbstractItemView::NoEditTriggers);
    auto* sweep=authored->topLevelItem(1)->child(0);
    assert(sweep && sweep->text(0)==QStringLiteral("Voltage sweep"));
    authored->setCurrentItem(sweep);
    QCoreApplication::processEvents();
    previewButton->click();
    assert(tabs->currentIndex()==1);
    assert(scope->currentIndex()==1);
    assert(resultTable->rowCount()==9); // three setpoints x three repeats
    assert(resultTable->item(0,0)->text()==QStringLiteral("2")); // Setup still #1
    assert(resultTable->item(0,2)->text()==QStringLiteral("Read 5 V, pass 0"));
    assert(resultTable->item(0,4)->text().contains(QStringLiteral("iteration = 0")));
    assert(resultTable->item(0,4)->text().contains(QStringLiteral("voltage = 5")));
    assert(resultTable->item(0,5)->text().contains(QStringLiteral("4.9")));
    assert(resultTable->item(0,5)->text().contains(QStringLiteral("5.1")));
    assert(resultTable->item(1,4)->text().contains(QStringLiteral("iteration = 1")));
    assert(resultTable->item(2,4)->text().contains(QStringLiteral("iteration = 2")));
    assert(resultTable->item(3,4)->text().contains(QStringLiteral("voltage = 4.75")));
    assert(resultTable->item(8,2)->text()==QStringLiteral("Read 4.5 V, pass 2"));
    assert(summary->text().contains(QStringLiteral("9 of 11")));
    scope->setCurrentIndex(0);
    assert(resultTable->rowCount()==11);
    assert(resultTable->item(0,1)->text()==QStringLiteral("Setup"));
    assert(resultTable->item(10,1)->text()==QStringLiteral("Cleanup"));
    // A preview must not expand and commit or rewrite the authored source.
    bool saved=false;
    editor.setSaveHandler([&](const QJsonObject& definition,const QJsonObject&,
                             const QJsonObject&,const QString&){
        saved=true;
        assert(definition.value(QStringLiteral("steps")).toArray()==
               original.value(QStringLiteral("steps")).toArray());
        return true;
    });
    actions->button(QDialogButtonBox::Save)->click();
    assert(saved);
    return 0;
}
