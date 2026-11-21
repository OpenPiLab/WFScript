// RC1.5.12: hierarchical authoring must preserve the physical-test definition.
#include "TestSequenceEditor.h"
#include "TestSequenceLoops.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTabWidget>
#include <QCoreApplication>
#include <QEventLoop>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static QJsonObject example(){
    const QJsonObject psu{{"id","psu"},{"name","Read PSU ${voltage} V"},
        {"type","numeric"},{"mode","callback"},{"handler","measurePsu"},
        {"limits",QJsonObject{
            {"low",QJsonObject{{"param","voltage"},{"offset",-0.1}}},
            {"high",QJsonObject{{"param","voltage"},{"offset",0.1}}}}}};
    const QJsonObject waveform{{"id","wave"},{"name","Capture ${amplitude} Vpp"},
        {"type","numeric"},{"mode","callback"},{"handler","measureWave"},
        {"measurements",QJsonObject{
            {"vpp",QJsonObject{{"name","Vpp at ${amplitude}"},
                {"low",QJsonObject{{"param","amplitude"},{"factor",0.85}}},
                {"high",QJsonObject{{"param","amplitude"},{"factor",1.15}}},
                {"unit","Vpp"}}},
            {"hz",QJsonObject{{"name","Frequency"},{"low",970},{"high",1030},{"unit","Hz"}}}}}};
    return QJsonObject{{"schemaVersion",1},{"id","tree-test"},{"title","Tree editor"},
        {"config",QJsonObject{}},{"steps",QJsonArray{
            QJsonObject{{"id","setup"},{"name","Setup"},{"group","setup"},
                {"type","action"},{"mode","once"},{"handler","setup"}},
            QJsonObject{{"id","voltageSweep"},{"name","PSU voltage sweep"},{"group","main"},
                {"loop",QJsonObject{{"parameter","voltage"},{"start",5},{"stop",4.5},{"increment",-0.25}}},
                {"steps",QJsonArray{psu}}},
            QJsonObject{{"id","waveSweep"},{"name","Waveform sweep"},{"group","main"},
                {"loop",QJsonObject{{"parameter","amplitude"},{"values",QJsonArray{1,2}}}},
                {"steps",QJsonArray{waveform}}},
            QJsonObject{{"id","cleanup"},{"name","Cleanup"},{"group","cleanup"},
                {"type","action"},{"mode","once"},{"handler","cleanup"}}
        }}};
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    const QJsonObject original=example();QString error;
    assert(TestSequenceEditor::validateDefinition(original,&error));
    const QStringList exports={"setup","measurePsu","measureWave","cleanup"};
    {
        TestSequenceEditor editor(original,{},exports);
        auto* tree=editor.findChild<QTreeWidget*>("testSequenceStepTable");
        auto* tabs=editor.findChild<QTabWidget*>("testSequenceEditorTabs");
        auto* rules=editor.findChild<QTableWidget*>("testSequenceNamedResultsTable");
        auto* axes=editor.findChild<QTableWidget*>("testSequenceSweepAxes");
        auto* actions=editor.findChild<QDialogButtonBox*>("testSequenceEditorActions");
        assert(tree && tabs && rules && axes && actions);
        assert(tabs->count()==9 && tabs->tabText(0)==QStringLiteral("Step Details"));
        assert(tree->topLevelItemCount()==3);
        auto* main=tree->topLevelItem(1);
        assert(main->childCount()==2 && main->child(0)->childCount()==1);
        // Inspect nested children: neither param-relative scalar nor named
        // result limits may be rewritten just by navigating the tree.
        tree->setCurrentItem(main->child(0)->child(0));
        auto* relative=editor.findChild<QCheckBox*>("testSequenceRelativeLimits");
        assert(relative && relative->isChecked());
        tree->setCurrentItem(tree->topLevelItem(1)->child(1)->child(0));
        assert(rules->rowCount()==2);
        bool foundExpression=false;
        for(int r=0;r<rules->rowCount();++r)
            if(rules->item(r,0)->text()==QStringLiteral("vpp"))
                foundExpression=rules->item(r,2)->text().contains(QStringLiteral("amplitude *"));
        assert(foundExpression);
        bool saved=false;
        editor.setSaveHandler([&](const QJsonObject& d,const QJsonObject&,const QJsonObject&,const QString&){
            saved=true;assert(d.value("steps").toArray()==original.value("steps").toArray());return true;
        });
        actions->button(QDialogButtonBox::Save)->click();assert(saved);
    }
    {
        TestSequenceEditor editor(original,{},exports);
        auto* tree=editor.findChild<QTreeWidget*>("testSequenceStepTable");
        auto* axes=editor.findChild<QTableWidget*>("testSequenceSweepAxes");
        auto* actions=editor.findChild<QDialogButtonBox*>("testSequenceEditorActions");
        assert(tree && axes && actions);
        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        assert(axes->rowCount()==1 && axes->item(0,4)->text()==QStringLiteral("-0.25"));
        editor.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    axes->item(0,4)->setText(QStringLiteral("-0.5"));
        bool saved=false;
        editor.setSaveHandler([&](const QJsonObject& d,const QJsonObject&,const QJsonObject&,const QString&){
            saved=true;
            const auto steps=d.value("steps").toArray();
            assert(steps.size()==4);
            const auto sweep=steps.at(1).toObject();
            assert(sweep.value("loop").toObject().value("increment").toDouble()==-0.5);
            assert(sweep.value("steps").toArray()==original.value("steps").toArray().at(1).toObject().value("steps").toArray());
            QJsonObject expanded;QString why;
            assert(TestSequenceLoops::expand(d,&expanded,&why));
            assert(expanded.value("steps").toArray().size()==6); // setup + 2 PSU + 2 wave + cleanup
            return true;
        });
        editor.show();QCoreApplication::processEvents(QEventLoop::AllEvents,20);
        editor.showNormal();editor.resize(800,550);
        QCoreApplication::processEvents(QEventLoop::AllEvents,20);
        assert(actions->geometry().bottom()<=editor.height());
        actions->button(QDialogButtonBox::Save)->click();assert(saved);
    }
    return 0;
}
