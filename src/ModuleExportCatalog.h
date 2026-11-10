#pragma once
#include "CompletionCatalog.h"
#include <QString>
#include <QVector>
#include <functional>
#include <optional>

// Editor-only static help for *named* ES-module exports. No JavaScript is
// evaluated, no modules are imported, and instrument outputs are untouched.
// Deliberately does not pretend to implement a full ECMAScript parser.
namespace ModuleExportCatalog {
struct Export {
    CompletionCatalog::Item item;
    QString signature;
};
// Find a simple `var name = modules.load("Library.mjs")` alias in source text
// preceding the caret. Returns empty for unsupported/ambiguous expressions.
QString moduleForAlias(const QString& sourceBeforeCaret, const QString& alias);
QVector<Export> namedExports(const QString& moduleSource);

// A module export resolved from a completed member expression in source.
// This is the same export index used by autocomplete; it never evaluates JS.
struct ResolvedExport {
    QString alias;
    QString module;
    Export member;
};
// F1 works while the popup is closed, including the caret inside `square`
// in `math.square(5)` and after the member name. Unknown/dynamic expressions
// deliberately return nullopt rather than inventing documentation.
std::optional<ResolvedExport> resolveAtCursor(
    const QString& editorSource, int caretPosition,
    const std::function<QString(const QString&)>& moduleSourceResolver);
// Shared HTML for completion tooltip and the full F1 help window.
QString helpHtml(const ResolvedExport& reference);

}
