#pragma once
#include <QString>
#include <QVector>

// Conservative, hardware-free editor diagnostics. No JavaScript evaluation.
// Only validates *literal* dotted paths rooted at an instrument API global.
// It deliberately skips dynamic expressions and local aliases rather than
// guessing their types or generating false positive error messages.
namespace ApiDiagnostics {
struct Issue {
    int line=0;          // one based
    int column=0;        // one based
    int length=0;
    QString path;
    QString message;
    QString suggestion;
    int start=0;         // absolute UTF-16 position in QTextDocument
};
QVector<Issue> analyze(const QString& source);
}
