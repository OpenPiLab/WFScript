#pragma once
#include <QJSValue>
#include <QString>
#include <QStringList>

// A diagnostic is about a source document, not the QJSEngine instance or tab.
// Source identity survives editor tab closure and makes errors navigable.
struct ScriptErrorDetails {
    QString source;
    int line = 0;
    int column = 0;
    QString message;
    QString stack;
};
namespace ScriptErrorFormatting {
    QString sourceFromQt(const QString& raw);
    ScriptErrorDetails fromException(const QJSValue& result,
                                    const QString& defaultSource,
                                    const QStringList& frames = {});
}
