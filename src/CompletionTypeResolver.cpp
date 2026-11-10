#include "CompletionTypeResolver.h"
#include "CompletionCatalog.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace {
struct Token {
    QString text;
    bool identifier = false;
};

bool idStart(QChar c) { return c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$'); }
bool idPart(QChar c) { return idStart(c) || c.isDigit(); }

// Ignore JS text which could falsely look like declarations: comments, strings,
// regular expressions and template strings. This is intentionally a tokenizer
// of the supported completion subset, not a full ECMAScript grammar.
QVector<Token> lex(const QString& src) {
    QVector<Token> tokens;
    const int n = src.size();
    bool mayStartRegex = true;
    for (int i = 0; i < n;) {
        const QChar c = src.at(i);
        if (c.isSpace()) { ++i; continue; }
        if (c == QLatin1Char('/') && i+1 < n && src.at(i+1) == QLatin1Char('/')) {
            i += 2;
            while (i < n && src.at(i) != QLatin1Char('\n')) ++i;
            continue;
        }
        if (c == QLatin1Char('/') && i+1 < n && src.at(i+1) == QLatin1Char('*')) {
            i += 2;
            while (i+1 < n && !(src.at(i)==QLatin1Char('*') && src.at(i+1)==QLatin1Char('/'))) ++i;
            i = (i+1 < n) ? i+2 : n;
            continue;
        }
        if (c == QLatin1Char('\'') || c == QLatin1Char('"') || c == QLatin1Char('`')) {
            const QChar delimiter = c;
            ++i;
            while (i < n) {
                if (src.at(i) == QLatin1Char('\\')) { i += (i+1<n ? 2 : 1); continue; }
                if (src.at(i++) == delimiter) break;
            }
            tokens.append(Token{QStringLiteral("@literal"),false});
            mayStartRegex = false;
            continue;
        }
        if (c == QLatin1Char('/') && mayStartRegex) {
            // Skip a regexp literal and its flags. Character classes may contain '/'.
            int j = i+1; bool inClass=false, closed=false;
            for (; j<n && src.at(j)!=QLatin1Char('\n'); ++j) {
                if (src.at(j)==QLatin1Char('\\')) { if (j+1<n) ++j; continue; }
                if (src.at(j)==QLatin1Char('[')) inClass=true;
                else if (src.at(j)==QLatin1Char(']')) inClass=false;
                else if (src.at(j)==QLatin1Char('/') && !inClass) { closed=true; ++j; break; }
            }
            if (closed) {
                while (j<n && idPart(src.at(j))) ++j;
                i=j; tokens.append(Token{QStringLiteral("@literal"),false}); mayStartRegex=false; continue;
            }
        }
        if (idStart(c)) {
            int j=i+1;
            while (j<n && idPart(src.at(j))) ++j;
            const QString word=src.mid(i,j-i);
            tokens.append(Token{word,true});
            mayStartRegex = word==QStringLiteral("return") || word==QStringLiteral("throw") ||
                            word==QStringLiteral("case") || word==QStringLiteral("delete") ||
                            word==QStringLiteral("typeof") || word==QStringLiteral("void") ||
                            word==QStringLiteral("yield") || word==QStringLiteral("await");
            i=j; continue;
        }
        if (c.isDigit()) {
            int j=i+1;
            while (j<n && (idPart(src.at(j)) || src.at(j)==QLatin1Char('.'))) ++j;
            tokens.append(Token{QStringLiteral("@number"),false});
            i=j; mayStartRegex=false; continue;
        }
        // Keep the distinction between plain '=' and equality/operators.
        if (i+2<n && (src.mid(i,3)==QStringLiteral("===") || src.mid(i,3)==QStringLiteral("!=="))) {
            tokens.append(Token{src.mid(i,3),false}); i+=3; mayStartRegex=true; continue;
        }
        if (i+1<n) {
            const QString two=src.mid(i,2);
            if (two==QStringLiteral("==") || two==QStringLiteral("!=") ||
                two==QStringLiteral("=>") || two==QStringLiteral("<=") ||
                two==QStringLiteral(">=") || two==QStringLiteral("&&") ||
                two==QStringLiteral("||") || two==QStringLiteral("??") ||
                two==QStringLiteral("+=") || two==QStringLiteral("-=") ||
                two==QStringLiteral("++") || two==QStringLiteral("--") ||
                two==QStringLiteral("?.")) {
                tokens.append(Token{two,false}); i+=2; mayStartRegex=true; continue;
            }
        }
        tokens.append(Token{QString(c),false});
        mayStartRegex = c != QLatin1Char(')') && c != QLatin1Char(']') &&
                        c != QLatin1Char('}');
        ++i;
    }
    return tokens;
}

// Channel1 and Channel2 share a completion contract, not the same device state.
QString contractPath(QString p) {
    if (p == QStringLiteral("Scope") || p.startsWith(QStringLiteral("Scope.")))
        p.replace(0,5,QStringLiteral("Scope1"));
    else if (p == QStringLiteral("Wavegen") || p.startsWith(QStringLiteral("Wavegen.")))
        p.replace(0,7,QStringLiteral("Wavegen1"));
    else if (p == QStringLiteral("Logic") || p.startsWith(QStringLiteral("Logic.")))
        p.replace(0,5,QStringLiteral("Logic1"));
    else if (p == QStringLiteral("Patterns") || p.startsWith(QStringLiteral("Patterns.")))
        p.replace(0,8,QStringLiteral("Patterns1"));
    // Use representative channel path for completion only.
    p.replace(QStringLiteral(".Channel2"),QStringLiteral(".Channel1"));
    return p;
}

bool isCatalogPath(const QString& p) {
    return !CompletionCatalog::entries(contractPath(p)).isEmpty();
}

struct Scope {
    bool isFunction = false;
    QHash<QString,QString> bindings; // Empty string = shadowed/unknown type.
};

class Analyzer {
public:
    explicit Analyzer(const QString& source): t(lex(source)) { frames.append(Scope{true,{}}); }

    QString resolve(const QString& typedPath) {
        analyze();
        if (typedPath.isEmpty()) return typedPath;
        const int dot=typedPath.indexOf(QLatin1Char('.'));
        const QString root=dot<0 ? typedPath : typedPath.left(dot);
        QString known;
        if (lookup(root,known)) {
            if (known.isEmpty()) return {};
            const QString result=contractPath(known+(dot<0?QString():typedPath.mid(dot)));
            return isCatalogPath(result) ? result : QString();
        }
        return typedPath; // Existing explicit catalog handles built-in paths.
    }

private:
    const QVector<Token> t;
    QVector<Scope> frames;
    QHash<int,QStringList> functionParamsAtBrace;

    bool lookup(const QString& name, QString& value) const {
        for (int s=frames.size()-1;s>=0;--s) {
            const auto it=frames.at(s).bindings.constFind(name);
            if (it!=frames.at(s).bindings.cend()) { value=it.value(); return true; }
        }
        return false;
    }
    void declare(const QString& name, const QString& type, bool functionScoped) {
        int index=frames.size()-1;
        if (functionScoped) while (index>0 && !frames.at(index).isFunction) --index;
        frames[index].bindings.insert(name,type);
    }
    void assign(const QString& name,const QString& type) {
        for (int s=frames.size()-1;s>=0;--s) {
            if (frames[s].bindings.contains(name)) { frames[s].bindings.insert(name,type);return; }
        }
        // Don't create aliases for undeclared globals or arbitrary properties.
    }
    bool outerParens(int begin,int end) const {
        if (end-begin<2 || t.at(begin).text!=QStringLiteral("(") ||
            t.at(end-1).text!=QStringLiteral(")")) return false;
        int depth=0;
        for(int k=begin;k<end;++k) {
            if(t.at(k).text==QStringLiteral("(")) ++depth;
            else if(t.at(k).text==QStringLiteral(")")) {
                --depth;
                if (depth==0 && k!=end-1) return false;
            }
        }
        return depth==0;
    }
    QString infer(int begin,int end,int recursion=0) const {
        if (recursion>12) return {};
        while (outerParens(begin,end)) { ++begin;--end; }
        if(begin>=end) return {};
        int round=0,square=0,curly=0,question=-1,colon=-1,nested=0;
        for(int k=begin;k<end;++k) {
            const QString& v=t.at(k).text;
            if(v==QStringLiteral("(")) ++round;
            else if(v==QStringLiteral(")")) --round;
            else if(v==QStringLiteral("[")) ++square;
            else if(v==QStringLiteral("]")) --square;
            else if(v==QStringLiteral("{")) ++curly;
            else if(v==QStringLiteral("}")) --curly;
            if(round==0 && square==0 && curly==0) {
                if(v==QStringLiteral("?")) { if(question<0) question=k; else ++nested; }
                else if(v==QStringLiteral(":") && question>=0) {
                    if(nested==0) {colon=k;break;} else --nested;
                }
            }
        }
        if(question>=0) {
            if(colon<0) return {};
            const QString yes=infer(question+1,colon,recursion+1);
            const QString no=infer(colon+1,end,recursion+1);
            return !yes.isEmpty() && yes==no ? yes : QString();
        }
        // A member expression only: Root(.field)*. No calls or computed access.
        if (!t.at(begin).identifier) return {};
        QStringList parts={t.at(begin).text};
        for(int k=begin+1;k<end;k+=2) {
            if(k+1>=end || t.at(k).text!=QStringLiteral(".") || !t.at(k+1).identifier)
                return {};
            parts.append(t.at(k+1).text);
        }
        QString prefix;
        if(lookup(parts.first(),prefix)) {
            if(prefix.isEmpty()) return {};
            parts.removeFirst();
            const QString full=contractPath(prefix+(parts.isEmpty()?QString():QStringLiteral(".")+parts.join('.')));
            return isCatalogPath(full) ? full : QString();
        }
        const QString full=contractPath(parts.join('.'));
        return isCatalogPath(full) ? full : QString();
    }
    // Find the end of an assignment expression without being fooled by
    // parentheses, calls, nested ternaries, or commas in function arguments.
    int endExpression(int start) const {
        int round=0,square=0,curly=0;
        for (int k=start;k<t.size();++k) {
            const QString& v=t.at(k).text;
            if(round==0 && square==0 && curly==0 &&
               (v==QStringLiteral(";") || v==QStringLiteral(",") ||
                v==QStringLiteral(")") || v==QStringLiteral("}"))) return k;
            if(v==QStringLiteral("(")) ++round;
            else if(v==QStringLiteral(")")) --round;
            else if(v==QStringLiteral("[")) ++square;
            else if(v==QStringLiteral("]")) --square;
            else if(v==QStringLiteral("{")) ++curly;
            else if(v==QStringLiteral("}")) --curly;
        }
        return t.size();
    }
    void captureFunctionParameters(int fn) {
        int open=-1;
        for(int k=fn+1;k<t.size() && k<fn+12;++k) {
            if(t.at(k).text==QStringLiteral("(")) {open=k;break;}
            if(t.at(k).text==QStringLiteral("{")) return;
        }
        if(open<0) return;
        int depth=1,close=open+1;
        for(;close<t.size() && depth;++close) {
            if(t.at(close).text==QStringLiteral("(")) ++depth;
            else if(t.at(close).text==QStringLiteral(")")) --depth;
        }
        if(depth) return;
        QStringList params;
        for(int k=open+1;k<close-1;++k) {
            if(t.at(k).identifier && (k==open+1 || t.at(k-1).text==QStringLiteral(",")))
                params.append(t.at(k).text);
        }
        if(close<t.size() && t.at(close).text==QStringLiteral("{"))
            functionParamsAtBrace.insert(close,params);
    }
    void analyze() {
        for(int k=0;k<t.size();++k) {
            const QString v=t.at(k).text;
            if(v==QStringLiteral("function")) captureFunctionParameters(k);
            if(v==QStringLiteral("{")) {
                const bool fn=functionParamsAtBrace.contains(k);
                frames.append(Scope{fn,{}});
                if(fn) for(const QString& param:functionParamsAtBrace.value(k)) declare(param,{},false);
                continue;
            }
            if(v==QStringLiteral("}")) { if(frames.size()>1) frames.removeLast();continue; }
            if ((v==QStringLiteral("var") || v==QStringLiteral("let") || v==QStringLiteral("const")) &&
                k+1<t.size() && t.at(k+1).identifier) {
                const bool funcScoped=(v==QStringLiteral("var"));
                int next=k+1;
                while(next<t.size() && t.at(next).identifier) {
                    const QString name=t.at(next).text;
                    int end=next+1;
                    QString type;
                    if(end<t.size() && t.at(end).text==QStringLiteral("=")) {
                        const int stop=endExpression(end+1);
                        type=infer(end+1,stop);
                        end=stop;
                    }
                    declare(name,type,funcScoped);
                    if(end<t.size() && t.at(end).text==QStringLiteral(",")) {next=end+1;continue;}
                    k=end>0?end-1:k;break;
                }
                continue;
            }
            if(t.at(k).identifier && k+1<t.size() && t.at(k+1).text==QStringLiteral("=") &&
               (k==0 || (t.at(k-1).text!=QStringLiteral(".") && t.at(k-1).text!=QStringLiteral("?.")))) {
                const int stop=endExpression(k+2);
                assign(v,infer(k+2,stop));
                if(stop>k+1) k=stop-1;
            }
        }
    }
};
} // namespace

QString CompletionTypeResolver::resolvePath(const QString& sourceBeforeCaret,const QString& typedPath) {
    return Analyzer(sourceBeforeCaret).resolve(typedPath);
}
