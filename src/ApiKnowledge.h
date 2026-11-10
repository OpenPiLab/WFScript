#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

// Source-of-truth policy:
// CompletionCatalog::entries() is the authoritative PUBLIC symbol inventory.
// Documentation is attached only to paths validated against that inventory.
// Unknown paths MUST NOT be advertised as supported. The curated documentation
// list is checked by WFScriptKnowledgeSmoke (and tools/check_api_knowledge.py).
namespace ApiKnowledge {
struct Documentation {
    QString path;
    QString symbol;
    QString summary;
    QString signature;
    QString parameters;
    QString unit;
    QString example;
    QString notes;
    bool known=false;
};
struct Snippet {
    QString trigger;
    QString description;
    QString body;  // editable ${1:default} tab stops, ${2:...}, etc.
};
QString canonical(QString path);
bool isDocumentedSymbol(const QString& path);
Documentation lookup(const QString& path);
QString html(const Documentation& doc, bool darkTheme=true);
QString guideHtml(const QString& topic); // non-API conceptual reference pages
QStringList curatedPaths();
QVector<Snippet> snippets();
Snippet snippet(const QString& trigger);
// Lightweight static check: snippets use documented API calls (no JS execution).
QStringList referencedApiPaths(const QString& snippetBody);
}
