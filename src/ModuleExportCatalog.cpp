#include "ModuleExportCatalog.h"
#include <QRegularExpression>
#include <QSet>

namespace ModuleExportCatalog {
namespace {
// Lightweight lexical guard: never index declarations inside quoted text or
// comments. Full JavaScript parsing (templates with ${...}, destructuring,
// computed properties) is intentionally left to the runtime/diagnostics.
bool isCodeAt(const QString& source,int position) {
    enum class State { Code, LineComment, BlockComment, Single, Double, Template };
    State state=State::Code;
    bool escape=false;
    for(int i=0;i<position && i<source.size();++i) {
        const QChar c=source.at(i);
        const QChar next=(i+1<source.size()?source.at(i+1):QChar());
        switch(state) {
        case State::Code:
            if(c==QLatin1Char('/') && next==QLatin1Char('/')) {state=State::LineComment;++i;}
            else if(c==QLatin1Char('/') && next==QLatin1Char('*')) {state=State::BlockComment;++i;}
            else if(c==QLatin1Char('\'')) {state=State::Single;escape=false;}
            else if(c==QLatin1Char('"')) {state=State::Double;escape=false;}
            else if(c==QLatin1Char('`')) {state=State::Template;escape=false;}
            break;
        case State::LineComment:
            if(c==QLatin1Char('\n'))state=State::Code;
            break;
        case State::BlockComment:
            if(c==QLatin1Char('*') && next==QLatin1Char('/')) {state=State::Code;++i;}
            break;
        case State::Single:case State::Double:case State::Template:
            if(escape){escape=false;break;}
            if(c==QLatin1Char('\\')){escape=true;break;}
            if((state==State::Single && c==QLatin1Char('\'')) ||
               (state==State::Double && c==QLatin1Char('"')) ||
               (state==State::Template && c==QLatin1Char('`')))state=State::Code;
            break;
        }
    }
    return state==State::Code;
}
QString descriptionBefore(const QString& source, int exportAt) {
    // Only attach an immediately preceding JSDoc comment, not an unrelated
    // comment elsewhere in the module. Never execute or interpret annotations.
    const QString preceding=source.left(exportAt);
    const int end=preceding.lastIndexOf(QStringLiteral("*/"));
    if(end<0 || !preceding.mid(end+2).trimmed().isEmpty())return {};
    const int begin=preceding.lastIndexOf(QStringLiteral("/**"),end);
    if(begin<0)return {};
    QString comment=preceding.mid(begin+3,end-begin-3);
    QStringList lines;
    for(QString line:comment.split(QLatin1Char('\n'))) {
        line=line.trimmed();
        if(line.startsWith(QLatin1Char('*')))line=line.mid(1).trimmed();
        if(!line.isEmpty())lines<<line;
    }
    return lines.join(QLatin1Char('\n')).left(900);
}
void append(QVector<Export>& out,QSet<QString>& seen,const QString& name,
            CompletionCatalog::Kind kind,const QString& detail,const QString& sig={}) {
    if(name.isEmpty() || seen.contains(name))return;
    seen.insert(name);
    out.append({{name,kind,detail.isEmpty()?QStringLiteral("Exported module member"):detail},sig});
}
}
QString moduleForAlias(const QString& text,const QString& alias) {
    static const QRegularExpression assignment(QStringLiteral(
        R"(\b(?:var|let|const)\s+([A-Za-z_$][\w$]*)\s*=\s*modules\s*\.\s*load\s*\(\s*(['"])([^'"\r\n]+)\2\s*\))"));
    QString module;
    int after=-1;
    auto it=assignment.globalMatch(text);
    while(it.hasNext()) {
        const auto m=it.next();
        if(m.captured(1)!=alias || !isCodeAt(text,m.capturedStart(1)))continue;
        module=m.captured(3);after=m.capturedEnd();
    }
    if(after<0)return {};
    // A later assignment invalidates the previous import inference. This is
    // intentionally conservative; dynamic aliases are not promised.
    const QRegularExpression re(QStringLiteral(R"(\b%1\s*=(?!=))")
       .arg(QRegularExpression::escape(alias)));
    if(re.match(text,after).hasMatch())return {};
    return module.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive)?module:QString();
}
QVector<Export> namedExports(const QString& source) {
    QVector<Export> result;
    QSet<QString> seen;
    // Anchoring to start-of-line keeps expressions, ordinary calls and most
    // strings from being mistaken for declarations. Multiline export syntax
    // and re-exports are intentionally outside this lightweight subset.
    static const QRegularExpression declarations(QStringLiteral(
        R"((?:^|\n)\s*export\s+(?:(?:async\s+)?function\s*\*?\s*([A-Za-z_$][\w$]*)\s*\(([^\r\n)]*)\)|(?:const|let|var)\s+([A-Za-z_$][\w$]*)\b|class\s+([A-Za-z_$][\w$]*)\b))"));
    auto it=declarations.globalMatch(source);
    while(it.hasNext()) {
        const auto m=it.next();
        // Compute comment location from the actual `export` token, including
        // indentation, rather than from a potentially multiline declaration.
        const int exportAt=source.indexOf(QStringLiteral("export"),m.capturedStart());
        if(exportAt<0 || !isCodeAt(source,exportAt))continue;
        const QString description=descriptionBefore(source,exportAt);
        if(!m.captured(1).isEmpty()) {
            const QString sig=m.captured(1)+QStringLiteral("(")+m.captured(2).trimmed()+QLatin1Char(')');
            append(result,seen,m.captured(1),CompletionCatalog::Kind::Function,
                   description.isEmpty()?QStringLiteral("Exported function %1").arg(sig):description,sig);
        } else if(!m.captured(3).isEmpty()) {
            append(result,seen,m.captured(3),CompletionCatalog::Kind::Object,
                   description.isEmpty()?QStringLiteral("Exported constant or variable"):description);
        } else if(!m.captured(4).isEmpty()) {
            append(result,seen,m.captured(4),CompletionCatalog::Kind::Object,
                   description.isEmpty()?QStringLiteral("Exported JavaScript class"):description);
        }
    }
    // Names exposed by `export { localName as publicName }`.
    static const QRegularExpression namedList(QStringLiteral(R"((?:^|\n)\s*export\s*\{([^}\r\n]+)\}\s*;?)"));
    auto lists=namedList.globalMatch(source);
    while(lists.hasNext()) {
        const auto m=lists.next();
        const int exportAt=source.indexOf(QStringLiteral("export"),m.capturedStart());
        if(exportAt<0 || !isCodeAt(source,exportAt))continue;
        for(const QString& member:m.captured(1).split(QLatin1Char(','))) {
            const QString trimmed=member.trimmed();
            static const QRegularExpression item(QStringLiteral(
                R"(^([A-Za-z_$][\w$]*)(?:\s+as\s+([A-Za-z_$][\w$]*))?$)"));
            const auto n=item.match(trimmed);
            if(n.hasMatch())append(result,seen,n.captured(2).isEmpty()?n.captured(1):n.captured(2),
                                   CompletionCatalog::Kind::Object,
                                   QStringLiteral("Named export (re-exported local symbol)"));
        }
    }
    return result;
}
}


namespace ModuleExportCatalog {
std::optional<ResolvedExport> resolveAtCursor(
    const QString& editorSource, int caretPosition,
    const std::function<QString(const QString&)>& moduleSourceResolver) {
    if(!moduleSourceResolver || caretPosition<0 || caretPosition>editorSource.size())return std::nullopt;
    auto memberCharacter=[](QChar c){return c.isLetterOrNumber() || c==QLatin1Char('_') || c==QLatin1Char('$') || c==QLatin1Char('.');};
    int start=caretPosition;
    int end=caretPosition;
    while(start>0 && memberCharacter(editorSource.at(start-1))) --start;
    while(end<editorSource.size() && memberCharacter(editorSource.at(end))) ++end;
    if(start==end || !isCodeAt(editorSource,start))return std::nullopt;
    const QString expression=editorSource.mid(start,end-start);
    static const QRegularExpression twoMembers(QStringLiteral(
        R"(^([A-Za-z_$][A-Za-z0-9_$]*)\.([A-Za-z_$][A-Za-z0-9_$]*)$)"));
    const auto match=twoMembers.match(expression);
    if(!match.hasMatch())return std::nullopt;
    // Caret must be over the exported member, not the import alias.
    const int memberBegin=start+match.capturedStart(2);
    if(caretPosition<memberBegin)return std::nullopt;
    const QString alias=match.captured(1);
    const QString module=moduleForAlias(editorSource.left(start),alias);
    if(module.isEmpty())return std::nullopt;
    const QString moduleSource=moduleSourceResolver(module);
    for(const auto& exported:namedExports(moduleSource)) {
        if(exported.item.name==match.captured(2))return ResolvedExport{alias,module,exported};
    }
    return std::nullopt;
}

QString helpHtml(const ResolvedExport& reference) {
    const auto& exported=reference.member;
    const QString signature=exported.signature.isEmpty()
        ? exported.item.name : exported.signature;
    QString kind=QStringLiteral("Exported value");
    if(exported.item.kind==CompletionCatalog::Kind::Function)kind=QStringLiteral("Exported function");
    else if(exported.item.description==QStringLiteral("Exported JavaScript class"))kind=QStringLiteral("Exported class");
    // HTML-escape all module/user-source text: JSDoc is data, never markup.
    const QString docs=exported.item.description.toHtmlEscaped()
        .replace(QLatin1Char('\n'),QStringLiteral("<br>"));
    return QStringLiteral("<h2>%1.%2</h2><p><b>%3</b> &middot; <code>%4</code></p>"
                          "<h3>Signature</h3><pre>%5</pre>"
                          "<h3>Documentation</h3><p>%6</p>"
                          "<small>Extracted from the module's named exports and JSDoc. "
                          "Source is not executed for help.</small>")
        .arg(reference.alias.toHtmlEscaped(),exported.item.name.toHtmlEscaped(),
             kind.toHtmlEscaped(),reference.module.toHtmlEscaped(),
             signature.toHtmlEscaped(),docs);
}
}
