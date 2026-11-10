#include "ApiDiagnostics.h"
#include "CompletionCatalog.h"

#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

namespace ApiDiagnostics {
namespace {
bool identifier(QChar c) {return c.isLetterOrNumber() || c=='_' || c=='$';}

// Preserve character offsets, replacing comments and quoted text with spaces.
// Template text is intentionally ignored, including interpolations: reporting
// fewer warnings is preferable to warning about code in an incomplete template.
QString codeOnly(const QString& source) {
    QString result=source;
    enum State { Code, SingleQuote, DoubleQuote, Template, LineComment, BlockComment };
    State state=Code;
    for(int i=0;i<source.size();++i) {
        const QChar c=source.at(i), n=i+1<source.size()?source.at(i+1):QChar();
        if(state==Code) {
            if(c=='/' && n=='/') {result[i]=' ';result[++i]=' ';state=LineComment;continue;}
            if(c=='/' && n=='*') {result[i]=' ';result[++i]=' ';state=BlockComment;continue;}
            if(c=='\'' || c=='"' || c=='`') {
                state = c=='\'' ? SingleQuote : c=='"' ? DoubleQuote : Template;
                // The opening delimiter starts a literal; it must not also
                // be considered its closing delimiter on this same iteration.
                result[i]=' ';
                continue;
            }
            continue;
        }
        if(c!='\n' && c!='\r') result[i]=' ';
        if(state==LineComment) {if(c=='\n') state=Code;}
        else if(state==BlockComment) {
            if(c=='*' && n=='/') {result[++i]=' ';state=Code;}
        } else if(state==SingleQuote || state==DoubleQuote || state==Template) {
            if(c=='\\' && i+1<source.size()) { if(source.at(++i)!='\n') result[i]=' '; }
            else if((state==SingleQuote && c=='\'') || (state==DoubleQuote && c=='"') || (state==Template && c=='`')) state=Code;
            else if(c=='\n' && state!=Template) state=Code;
        }
    }
    return result;
}
int distance(QString a, QString b) {
    a=a.toLower();b=b.toLower();
    QVector<int> previous(b.size()+1),current(b.size()+1);
    for(int j=0;j<=b.size();++j) previous[j]=j;
    for(int i=1;i<=a.size();++i) {
        current[0]=i;
        for(int j=1;j<=b.size();++j)
            current[j]=qMin(qMin(current[j-1]+1,previous[j]+1),
                            previous[j-1]+(a.at(i-1)==b.at(j-1)?0:1));
        previous.swap(current);
    }
    return previous.last();
}
QString closest(const QString& name,const QVector<CompletionCatalog::Item>& members) {
    int best=qMax(2,name.size()/3),score=best+1;
    QString suggestion;
    for(const auto& member:members) {
        const int d=distance(name,member.name);
        if(d<score && d<=best) {score=d;suggestion=member.name;}
    }
    return suggestion;
}
}
QVector<Issue> analyze(const QString& source) {
    QVector<Issue> issues;
    if(source.size()>300000) return issues; // bound interactive costs
    const QString code=codeOnly(source);
    static const QRegularExpression rx(QStringLiteral(
        R"(\b(Scope1|Scope|Wavegen1|Wavegen|Logic1|Logic|Supplies1|Supplies|StaticIO|Patterns1|Patterns|Device)(?:\s*\.\s*[A-Za-z_$][\w$]*)+)"));
    auto matches=rx.globalMatch(code);
    while(matches.hasNext()) {
        const auto m=matches.next();
        const int start=m.capturedStart();
        if(start>0 && (identifier(code.at(start-1)) || code.at(start-1)=='.')) continue;
        const QString candidate=m.captured();
        // Do not cross newlines to validate a partially edited expression.
        if(candidate.contains('\n')) continue;
        static const QRegularExpression memberRx(QStringLiteral(R"(\.\s*([A-Za-z_$][\w$]*))"));
        QString path=m.captured(1);
        auto it=memberRx.globalMatch(candidate);
        while(it.hasNext()) {
            const auto member=it.next();
            const QString leaf=member.captured(1);
            const auto entries=CompletionCatalog::entries(path);
            // The catalog only owns explicitly described API objects. A method
            // result, number, string or data array becomes ordinary JS here.
            if(entries.isEmpty()) break;
            auto found=entries.cend();
            for(auto e=entries.cbegin();e!=entries.cend();++e)
                if(e->name==leaf) {found=e;break;}
            if(found==entries.cend()) {
                Issue issue;
                issue.start=start+member.capturedStart(1);
                issue.length=leaf.size();
                issue.path=path+QLatin1Char('.')+leaf;
                issue.suggestion=closest(leaf,entries);
                issue.message=QStringLiteral("Unknown %1 member '%2'").arg(path,leaf);
                if(!issue.suggestion.isEmpty())
                    issue.message+=QStringLiteral(". Did you mean '%1'?").arg(issue.suggestion);
                const QString prior=source.left(issue.start);
                issue.line=prior.count('\n')+1;
                issue.column=issue.start-(prior.lastIndexOf('\n')+1)+1;
                issues.append(issue);
                break;
            }
            path+=QLatin1Char('.')+leaf;
            if(found->kind!=CompletionCatalog::Kind::Object) break;
        }
        if(issues.size()>=100) break;
    }
    return issues;
}
} // namespace ApiDiagnostics
