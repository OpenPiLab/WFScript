#pragma once

#include <QString>

// Conservative, editor-only JavaScript alias resolution. No QJSEngine execution,
// QObject introspection, device access, or arbitrary expression evaluation.
// The source must end at the *current caret*, so declarations in later lines
// cannot affect completion. Unknown and conflicting types stay unknown.
namespace CompletionTypeResolver {
// Returns the documented API path to query in CompletionCatalog::entries().
// An empty result means a shadowing local is known but has no inferable API type.
QString resolvePath(const QString& sourceBeforeCaret, const QString& typedPath);
}
