#pragma once
#include <QString>
#include <QStringList>

// Static, hardware-free editing helpers. Never evaluate JavaScript here.
namespace EditorAssist {
struct ValueContext {
    QString apiPath;      // Path for the string property or method
    QString typed;        // String content before the caret
    bool valid=false;
};
struct CallContext {
    QString apiPath;
    int argument=0;
    bool valid=false;
};

// Conservative lexical context. Code includes whitespace, but not comments,
// quoted strings or template raw text.
bool inCode(const QString& source, int position);
int matchingBracket(const QString& source, int bracketIndex);
ValueContext valueContext(const QString& beforeCaret);
CallContext callContext(const QString& beforeCaret);
QStringList enumValues(QString resolvedPath);
QString signature(QString resolvedPath, int argument);
}
