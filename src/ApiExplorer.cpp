#include "ApiExplorer.h"
#include "CompletionCatalog.h"
#include "ApiKnowledge.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QAbstractItemView>

namespace {
QString kindLabel(CompletionCatalog::Kind kind) {
    using K=CompletionCatalog::Kind;
    switch(kind) {
    case K::Object: return QStringLiteral("Object");
    case K::Function: return QStringLiteral("Method");
    case K::Number: return QStringLiteral("Number");
    case K::String: return QStringLiteral("String");
    case K::Boolean: return QStringLiteral("Boolean");
    case K::Array: return QStringLiteral("Array");
    case K::Keyword: return QStringLiteral("Keyword");
    case K::Snippet: return QStringLiteral("Snippet");
    }
    return {};
}
}
ApiExplorer::ApiExplorer(QWidget* parent):QWidget(parent) {
    auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(5,5,5,5);
    m_search=new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("apiExplorerSearch"));
    m_search->setPlaceholderText(QStringLiteral("Filter API symbols..."));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);
    m_tree=new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("apiExplorerTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setUniformRowHeights(true);
    layout->addWidget(m_tree,3);
    m_details=new QTextBrowser(this);
    m_details->setObjectName(QStringLiteral("apiExplorerDetails"));
    m_details->setOpenExternalLinks(false);
    m_details->setMaximumHeight(240);
    layout->addWidget(m_details,2);
    m_insert=new QPushButton(QStringLiteral("Insert API path"),this);
    m_insert->setObjectName(QStringLiteral("apiExplorerInsert"));
    m_insert->setToolTip(QStringLiteral("Insert the selected API path at the editor caret; does not execute it"));
    m_insert->setEnabled(false);
    layout->addWidget(m_insert);
    fill();
    connect(m_search,&QLineEdit::textChanged,this,[this](const QString& value){
        for(int i=0;i<m_tree->topLevelItemCount();++i) filterItem(m_tree->topLevelItem(i),value.trimmed());
    });
    connect(m_tree,&QTreeWidget::currentItemChanged,this,[this]{updateDetails();});
    connect(m_tree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){
        if(item && !item->data(0,Qt::UserRole).toString().startsWith(QStringLiteral("@guide")))
            emit insertRequested(item->data(0,Qt::UserRole).toString());
    });
    connect(m_insert,&QPushButton::clicked,this,[this]{
        if(auto* item=m_tree->currentItem()) {
            const QString path=item->data(0,Qt::UserRole).toString();
            if(!path.startsWith(QStringLiteral("@guide")))emit insertRequested(path);
        }
    });
    updateDetails();
}
void ApiExplorer::fill() {
    m_tree->clear();
    const QStringList roots={QStringLiteral("Device"),QStringLiteral("Scope"),
        QStringLiteral("Wavegen"),QStringLiteral("Logic"),QStringLiteral("Supplies"),QStringLiteral("StaticIO"),QStringLiteral("Patterns"),QStringLiteral("TestRunner"),QStringLiteral("modules"),QStringLiteral("plots"),QStringLiteral("ui"),
        QStringLiteral("Math"),QStringLiteral("console"),QStringLiteral("include"),QStringLiteral("print"),QStringLiteral("wait")};
    for(const auto& entry:CompletionCatalog::entries({})) {
        if(!roots.contains(entry.name)) continue;
        auto* root=new QTreeWidgetItem(m_tree);
        root->setText(0,entry.name);
        root->setData(0,Qt::UserRole,entry.name);
        root->setData(0,Qt::UserRole+1,entry.description);
        root->setToolTip(0,entry.description);
        if(entry.kind==CompletionCatalog::Kind::Object && entry.name!=QStringLiteral("TestRunner"))
            addChildren(root,entry.name,0);
        if(entry.name==QStringLiteral("TestRunner")) {
            auto* guide=new QTreeWidgetItem(root);
            guide->setText(0,QStringLiteral("Test Executive Guide"));
            guide->setData(0,Qt::UserRole,QStringLiteral("@guide"));
            guide->setData(0,Qt::UserRole+1,QStringLiteral("Execution model, step definitions, timing, reports, libraries"));
            const QList<QPair<QString,QString>> topics={
                {QStringLiteral("overview"),QStringLiteral("Getting started")},
                {QStringLiteral("definitions"),QStringLiteral("Definition Files and Named Handlers")},
                {QStringLiteral("groups"),QStringLiteral("Setup / Main / Cleanup and step types")},
                {QStringLiteral("modes"),QStringLiteral("Once / Delay / Poll and deadlines")},
                {QStringLiteral("lifecycle"),QStringLiteral("Stop, terminate, abort and states")},
                {QStringLiteral("context"),QStringLiteral("UUT details and shared run context")},
                {QStringLiteral("reports"),QStringLiteral("Measurement results and file export")},
                {QStringLiteral("libraries"),QStringLiteral("Reusable JavaScript libraries")}};
            for(const auto& topic:topics) {
                auto* item=new QTreeWidgetItem(guide);
                item->setText(0,topic.second);
                item->setData(0,Qt::UserRole,QStringLiteral("@guide/")+topic.first);
                item->setData(0,Qt::UserRole+1,topic.second);
                item->setData(0,Qt::UserRole+2,topic.first);
                item->setToolTip(0,QStringLiteral("Guide: ")+topic.second);
            }
            addChildren(root,entry.name,0); // guide first, then exact executable API methods
        }
    }
    for(int i=0;i<m_tree->topLevelItemCount();++i) m_tree->topLevelItem(i)->setExpanded(false);
}
void ApiExplorer::addChildren(QTreeWidgetItem* parent,const QString& path,int depth) {
    if(depth>=10) return;
    for(const auto& entry:CompletionCatalog::entries(path)) {
        if(entry.kind==CompletionCatalog::Kind::Keyword) continue;
        const QString full=path+QLatin1Char('.')+entry.name;
        auto* item=new QTreeWidgetItem(parent);
        item->setText(0,entry.name);
        item->setData(0,Qt::UserRole,full);
        item->setData(0,Qt::UserRole+1,entry.description);
        item->setToolTip(0,kindLabel(entry.kind)+QStringLiteral(" · ")+entry.description);
        if(entry.kind==CompletionCatalog::Kind::Object) addChildren(item,full,depth+1);
    }
}
bool ApiExplorer::filterItem(QTreeWidgetItem* item,const QString& term) {
    bool descendant=false;
    for(int i=0;i<item->childCount();++i) descendant=filterItem(item->child(i),term)||descendant;
    const bool match=term.isEmpty() ||
        item->data(0,Qt::UserRole).toString().contains(term,Qt::CaseInsensitive) ||
        item->data(0,Qt::UserRole+1).toString().contains(term,Qt::CaseInsensitive);
    item->setHidden(!match && !descendant);
    if(!term.isEmpty() && descendant) item->setExpanded(true);
    return match || descendant;
}
void ApiExplorer::updateDetails() {
    const auto* item=m_tree->currentItem();
    m_insert->setEnabled(item && !item->data(0,Qt::UserRole).toString().startsWith(QStringLiteral("@guide")));
    if(!item) {
        m_details->setHtml(QStringLiteral("<b>Instrument &amp; Script API Explorer</b><p>Browse TestRunner (including guided Test Executive topics), modules, plots, Scope, Wavegen, Logic, Supplies, StaticIO, Patterns, Device, and Panel APIs. "
            "Search by member or function name. Double-click a symbol to insert its path into the active script.</p>"));
        return;
    }
    const QString path=item->data(0,Qt::UserRole).toString();
    const QString topic=item->data(0,Qt::UserRole+2).toString();
    if(!topic.isEmpty()) {m_details->setHtml(ApiKnowledge::guideHtml(topic));return;}
    if(path==QStringLiteral("@guide")) {
        m_details->setHtml(QStringLiteral("<h3>Test Executive Guide</h3><p>Select a topic to learn the "
            "native execution lifecycle, then inspect a TestRunner method for its signature and example.</p>"));
        return;
    }
    const auto doc=ApiKnowledge::lookup(path);
    if(doc.known) m_details->setHtml(ApiKnowledge::html(doc,false));
    else m_details->setHtml(QStringLiteral("<h3>%1</h3><p>%2</p>")
        .arg(path.toHtmlEscaped(),item->data(0,Qt::UserRole+1).toString().toHtmlEscaped()));
}
bool ApiExplorer::containsPath(const QString& path) const {
    const auto items=m_tree->findItems(path.section('.',-1),Qt::MatchExactly|Qt::MatchRecursive);
    for(const auto* item:items) if(item->data(0,Qt::UserRole).toString()==path) return true;
    return false;
}
