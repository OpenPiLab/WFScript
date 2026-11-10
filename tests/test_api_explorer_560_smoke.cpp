// Offscreen QWidget test of the actual Explorer navigation/presentation.
#include "ApiExplorer.h"
#include "ApiKnowledge.h"
#include <QApplication>
#include <QDebug>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#define CHECK(test,why) do { if(!(test)){qCritical()<<"FAIL:"<<why;return 1;} }while(0)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    ApiExplorer explorer;
    CHECK(explorer.containsPath("TestRunner"),"TestRunner root visible");
    for(const auto& name:{"configure","add","run","onFinished","measure","terminate","abort",
        "setRunInfo","variable","report","saveReport","exportReport"})
        CHECK(explorer.containsPath("TestRunner."+QString::fromLatin1(name)),name);
    CHECK(explorer.containsPath("modules.load"),"module libraries visible");
    CHECK(explorer.containsPath("plots.configure"),"plot API visible");
    auto* tree=explorer.findChild<QTreeWidget*>("apiExplorerTree");
    auto* search=explorer.findChild<QLineEdit*>("apiExplorerSearch");
    auto* details=explorer.findChild<QTextBrowser*>("apiExplorerDetails");
    auto* insert=explorer.findChild<QPushButton*>("apiExplorerInsert");
    CHECK(tree&&search&&details&&insert,"Explorer widgets");
    const auto items=tree->findItems("exportReport",Qt::MatchExactly|Qt::MatchRecursive);
    CHECK(items.size()==1,"exportReport appears once");
    tree->setCurrentItem(items.constFirst());
    CHECK(details->toPlainText().contains("Documents/WFScript Reports"),"export API help");
    CHECK(insert->isEnabled(),"methods can be inserted");
    QTreeWidgetItem* guide=nullptr;
    for(int i=0;i<tree->topLevelItemCount();++i) {
        auto* root=tree->topLevelItem(i);
        if(root->text(0)!="TestRunner")continue;
        for(int j=0;j<root->childCount();++j)
            if(root->child(j)->text(0)=="Test Executive Guide") guide=root->child(j);
    }
    // The Libraries guide topic was added after this test was authored.
    CHECK(guide && guide->childCount()==8,"eight guided topics");
    CHECK(guide->child(7)->data(0,Qt::UserRole+2).toString()==QStringLiteral("libraries"),
          "named libraries guide remains available");
    tree->setCurrentItem(guide->child(3)); // Once / Delay / Poll topic
    CHECK(!insert->isEnabled(),"guide topics must not insert fake API paths");
    CHECK(details->toPlainText().contains("poll"),"once/delay/poll reference");
    search->setText("TestRunner.exportReport");
    CHECK(!items.constFirst()->isHidden(),"filter finds supported exported method");
    CHECK(!ApiKnowledge::guideHtml("reports").isEmpty(),"report reference exists");
    CHECK(ApiKnowledge::guideHtml("unknown").isEmpty(),"unknown topics are not advertised");
    qInfo()<<"PASS: actual Explorer TestRunner tree, concepts, searchable docs and insertion safety";
    return 0;
}
