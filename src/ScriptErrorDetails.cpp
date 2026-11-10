#include "ScriptErrorDetails.h"
#include <QRegularExpression>
#include <QUrl>

QString ScriptErrorFormatting::sourceFromQt(const QString& raw) {
    QString text=QUrl::fromPercentEncoding(raw.toUtf8()).trimmed();
    if(text.startsWith(QStringLiteral("file:"),Qt::CaseInsensitive)) {
        const QUrl url(raw);
        if(url.isLocalFile() && !url.toLocalFile().isEmpty())
            return url.toLocalFile();
        text=text.mid(5);
        if(text.startsWith(QStringLiteral("//"))) text=text.mid(2);
    }
    return text;
}

ScriptErrorDetails ScriptErrorFormatting::fromException(const QJSValue& result,
                                                         const QString& defaultSource,
                                                         const QStringList& frames) {
    ScriptErrorDetails detail;
    const QJSValue fileProperty=result.property(QStringLiteral("fileName"));
    if(fileProperty.isString())detail.source=sourceFromQt(fileProperty.toString());
    const bool explicitSource=!detail.source.isEmpty();
    if(detail.source.isEmpty()) detail.source=defaultSource;
    detail.line=result.property(QStringLiteral("lineNumber")).toInt();
    detail.column=result.property(QStringLiteral("columnNumber")).toInt();
    detail.message=result.toString();
    const QJSValue stackProperty=result.property(QStringLiteral("stack"));
    if(stackProperty.isString())detail.stack=stackProperty.toString();
    if(detail.stack.isEmpty() && !frames.isEmpty()) detail.stack=frames.join(QLatin1Char('\n'));
    // Qt's evaluate exception frames are: function:line:column:filename.
    // Parse from the final numeric fields, since filenames may contain ':' (Windows drives).
    static const QRegularExpression framePattern(QStringLiteral(R"(:(\d+):(\d+):(.+)$)"));
    const QStringList lines=!frames.isEmpty() ? frames : detail.stack.split(QLatin1Char('\n'));
    for(const QString& frame:lines) {
        const auto match=framePattern.match(frame.trimmed());
        if(!match.hasMatch()) continue;
        const QString frameSource=sourceFromQt(match.captured(3));
        if(!explicitSource && !frameSource.isEmpty())detail.source=frameSource;
        if(frameSource!=detail.source) continue;
        if(detail.line<=0)detail.line=match.captured(1).toInt();
        if(detail.column<=0)detail.column=match.captured(2).toInt();
        break;
    }
    if(detail.source.isEmpty())detail.source=defaultSource;
    return detail;
}
