#include "ScriptEditor.h"
#include "JavaScriptHighlighter.h"
#include "CompletionCatalog.h"
#include "CompletionTypeResolver.h"
#include "ModuleExportCatalog.h"
#include "EditorAssist.h"
#include "ApiKnowledge.h"
#include "ApiDiagnostics.h"
#include <QTimer>

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QLabel>
#include <QCompleter>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QPixmap>
#include <QIcon>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QToolTip>
#include <QTextCursor>
#include <algorithm>
#include <utility>

namespace {
// Small vector-drawn category glyphs; no external icon asset or font dependency.
QIcon completionIcon(CompletionCatalog::Kind kind) {
    using K = CompletionCatalog::Kind;
    QColor color; QString symbol;
    switch (kind) {
    case K::Object:   color=QColor("#7c63d5"); symbol="{}"; break;
    case K::Function: color=QColor("#397ac9"); symbol="f"; break;
    case K::Number:   color=QColor("#238e82"); symbol="#"; break;
    case K::String:   color=QColor("#ba7b24"); symbol="T"; break;
    case K::Boolean:  color=QColor("#c16c6c"); symbol="?"; break;
    case K::Array:    color=QColor("#4d9d9b"); symbol="[]"; break;
    case K::Keyword:  color=QColor("#66798b"); symbol="K"; break;
    case K::Snippet:  color=QColor("#8c66ae"); symbol="<>"; break;
    }
    QPixmap px(40,40); px.fill(Qt::transparent);
    QPainter pt(&px); pt.setRenderHint(QPainter::Antialiasing);
    pt.setPen(Qt::NoPen); pt.setBrush(color); pt.drawRoundedRect(QRectF(3,3,34,34),8,8);
    QFont f=QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setBold(true); f.setPixelSize(symbol.size()>1 ? 16 : 22);
    pt.setFont(f);pt.setPen(Qt::white);pt.drawText(px.rect(),Qt::AlignCenter,symbol);
    return QIcon(px);
}
}

class LineNumberArea final : public QWidget {
public:
    explicit LineNumberArea(ScriptEditor* editor) : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }
protected:
    void paintEvent(QPaintEvent* event) override { m_editor->paintLineNumberArea(event); }
private:
    ScriptEditor* m_editor;
};

ScriptEditor::ScriptEditor(QWidget* parent) : QPlainTextEdit(parent) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(11);
    setFont(font);
    setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setCenterOnScroll(true);

    m_gutter = new LineNumberArea(this);
    m_diagnosticTimer = new QTimer(this);
    m_diagnosticTimer->setSingleShot(true);
    m_diagnosticTimer->setInterval(550);
    connect(document(), &QTextDocument::contentsChanged, this, [this] {
        m_diagnosticTimer->start();
    });
    connect(m_diagnosticTimer, &QTimer::timeout, this, [this] {
        m_issues=ApiDiagnostics::analyze(toPlainText());
        highlightCurrentLine();
        m_gutter->update();
        emit diagnosticsChanged();
    });
    m_highlighter = new JavaScriptHighlighter(document());
    m_completer = new QCompleter(this);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchStartsWith);

    m_completionModel = new QStandardItemModel(m_completer);
    m_completer->setModel(m_completionModel);
    m_completer->setCompletionRole(Qt::EditRole);
    m_completer->setMaxVisibleItems(12);
    m_completer->popup()->setIconSize(QSize(20,20));
    m_completer->popup()->setMinimumWidth(300);
    connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated),
            this, &ScriptEditor::insertCompletion);
    // Always give the selected keyboard completion contextual documentation.
    connect(m_completer->popup()->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex&,const QModelIndex&) { showCompletionDocumentation(); });

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &ScriptEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &ScriptEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &ScriptEditor::highlightCurrentLine);
    updateLineNumberAreaWidth(0);
    setDarkTheme(true);
}

void ScriptEditor::setDarkTheme(bool dark) {
    m_dark = dark;
    QPalette p = palette();
    p.setColor(QPalette::Base, QColor(dark ? "#17212e" : "#ffffff"));
    p.setColor(QPalette::Text, QColor(dark ? "#e3ecf6" : "#19212b"));
    p.setColor(QPalette::Highlight, QColor(dark ? "#294f70" : "#b6d8f8"));
    p.setColor(QPalette::HighlightedText, QColor(dark ? "#ffffff" : "#111111"));
    setPalette(p);
    m_highlighter->setDarkTheme(dark);
    highlightCurrentLine();
    m_gutter->update();
}

void ScriptEditor::setModuleSourceResolver(std::function<QString(const QString&)> resolver) {
    m_moduleSourceResolver=std::move(resolver);
}

void ScriptEditor::setHintsEnabled(bool enabled) {
    m_hintsEnabled = enabled;
    if (!enabled) m_completer->popup()->hide();
}

int ScriptEditor::lineNumberAreaWidth() const {
    int digits = 1;
    for (int n = qMax(1, blockCount()); n >= 10; n /= 10) ++digits;
    return 16 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void ScriptEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void ScriptEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) m_gutter->scroll(0, dy);
    else m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void ScriptEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_gutter->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void ScriptEditor::paintLineNumberArea(QPaintEvent* event) {
    QPainter painter(m_gutter);
    painter.fillRect(event->rect(), QColor(m_dark ? "#111b27" : "#f0f2f6"));

    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(QColor(m_dark ? "#7b91aa" : "#7a8796"));
            painter.drawText(0, top, m_gutter->width()-7, fontMetrics().height(),
                             Qt::AlignRight, QString::number(number+1));
            for(const auto& issue:m_issues) {
                if(issue.line==number+1) {
                    painter.setBrush(QColor("#d5a248"));
                    painter.setPen(Qt::NoPen);
                    painter.drawEllipse(QRect(3,top+qMax(2,(fontMetrics().height()-8)/2),8,8));
                    break;
                }
            }
        }
        block = block.next();
        ++number;
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}

void ScriptEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection current;
        current.format.setBackground(QColor(m_dark ? "#213247" : "#eef5ff"));
        current.format.setProperty(QTextFormat::FullWidthSelection, true);
        current.cursor = textCursor();
        current.cursor.clearSelection();
        selections.append(current);
    }
    if (m_errorLine > 0) {
        const QTextBlock block = document()->findBlockByNumber(m_errorLine-1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection error;
            error.format.setBackground(QColor(m_dark ? "#582b37" : "#ffe4e4"));
            error.format.setProperty(QTextFormat::FullWidthSelection, true);
            error.cursor = QTextCursor(block);
            error.cursor.clearSelection();
            selections.append(error);
        }
    }
    for(const auto& issue:m_issues) {
        if(issue.start<0 || issue.start+issue.length>document()->characterCount()) continue;
        QTextEdit::ExtraSelection warning;
        warning.cursor=QTextCursor(document());
        warning.cursor.setPosition(issue.start);
        warning.cursor.setPosition(issue.start+issue.length,QTextCursor::KeepAnchor);
        warning.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        warning.format.setUnderlineColor(QColor(m_dark ? "#f0b75a" : "#b57500"));
        warning.format.setToolTip(issue.message);
        selections.append(warning);
    }
    // Highlight a matched pair only when the cursor is adjacent to a bracket.
    // Never include brackets embedded in strings/comments.
    const QString source = document()->toPlainText();
    const int caret = textCursor().position();
    const QString brackets = QStringLiteral("()[]{}");
    int bracket = -1;
    if (caret>0 && brackets.contains(source.at(caret-1))) bracket=caret-1;
    else if (caret<source.size() && brackets.contains(source.at(caret))) bracket=caret;
    if (bracket>=0) {
        const int other = EditorAssist::matchingBracket(source,bracket);
        if (other>=0) {
            for (int at : {bracket,other}) {
                QTextEdit::ExtraSelection match;
                match.format.setBackground(QColor(m_dark ? "#52756e" : "#b2e4d6"));
                match.format.setFontWeight(QFont::Bold);
                match.cursor=QTextCursor(document());
                match.cursor.setPosition(at);
                match.cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
                selections.append(match);
            }
        }
    }
    setExtraSelections(selections);
}

void ScriptEditor::highlightErrorLine(int oneBasedLine) {
    m_errorLine = oneBasedLine;
    if (oneBasedLine > 0) {
        const QTextBlock block = document()->findBlockByNumber(oneBasedLine-1);
        if (block.isValid()) {
            setTextCursor(QTextCursor(block));
            centerCursor();
        }
    }
    highlightCurrentLine();
}

void ScriptEditor::goToLine(int oneBasedLine) {
    const QTextBlock block=document()->findBlockByNumber(oneBasedLine-1);
    if(!block.isValid()) return;
    QTextCursor cursor(block);
    setTextCursor(cursor);
    centerCursor();
    setFocus();
}

void ScriptEditor::insertApiPath(const QString& path) {
    if(path.isEmpty() || isReadOnly()) return;
    // Inserting text is explicit user action: never invoke the JavaScript engine.
    insertPlainText(path);
    setFocus();
}

void ScriptEditor::insertCompletion(const QString& completion) {
    if (m_completer->widget() != this) return;
    QTextCursor cursor = textCursor();
    const auto snippet = ApiKnowledge::snippet(completion);
    if (!snippet.trigger.isEmpty() && !m_completionIsModule && !m_valueCompletion && m_completionPath.isEmpty()) {
        insertSnippet(snippet.body);
        QToolTip::hideText();
        return;
    }
    m_snippetStops.clear();m_snippetIndex=-1;
    if (m_valueCompletion) {
        // In a quoted argument or *.text assignment replace only the typed
        // prefix. Never consume the closing quote inserted by smart pairing.
        const int length = qMin(m_valuePrefix.size(), cursor.positionInBlock());
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, length);
        cursor.insertText(completion);
        m_valueCompletion=false;
        m_valuePrefix.clear();
    } else {
        cursor.select(QTextCursor::WordUnderCursor);
        cursor.insertText(completion);
    }
    setTextCursor(cursor);
}

void ScriptEditor::showCompletions(bool explicitRequest) {
    if (!m_hintsEnabled) return;
    const QString beforeFull = document()->toPlainText().left(textCursor().position());
    const QString before = textCursor().block().text().left(textCursor().positionInBlock());
    QVector<CompletionCatalog::Item> members;
    QString prefix;
    m_valueCompletion=false;
    m_valuePrefix.clear();
    m_completionPath.clear();
    m_completionIsModule=false;
    m_moduleCompletionDocs.clear();

    // Value completion is a distinct context from member completion; this
    // accepts one-character and empty prefixes inside quoted enums/measurements.
    const auto value=EditorAssist::valueContext(before);
    if (value.valid) {
        const int lastDot=value.apiPath.lastIndexOf(QLatin1Char('.'));
        const QString base=value.apiPath.left(lastDot);
        const QString parent=CompletionTypeResolver::resolvePath(beforeFull,base);
        const QString path=parent.isEmpty() ? value.apiPath : parent+value.apiPath.mid(lastDot);
        const QStringList values=EditorAssist::enumValues(path);
        if (!values.isEmpty()) {
            m_valueCompletion=true;
            m_completionPath=path;
            m_valuePrefix=value.typed;
            prefix=value.typed;
            for (const QString& v:values)
                members.append({v,CompletionCatalog::Kind::String,
                                QStringLiteral("Supported value for %1").arg(path)});
        }
    }
    if (!m_valueCompletion) {
        // Ordinary member/keyword completion belongs to JavaScript code,
        // not inside comments or arbitrary string literals.
        if (!EditorAssist::inCode(document()->toPlainText(),textCursor().position())) {
            m_completer->popup()->hide();return;
        }
        static const QRegularExpression chain(QStringLiteral(
            R"((([A-Za-z_$][A-Za-z0-9_$]*\.)*)([A-Za-z_$][A-Za-z0-9_$]*)?$)"));
        const auto match=chain.match(before);
        if (!match.hasMatch()) {m_completer->popup()->hide();return;}
        const QString dotted=match.captured(1);
        const QString path=dotted.endsWith(QLatin1Char('.')) ? dotted.left(dotted.size()-1) : dotted;
        prefix=match.captured(3);
        // A numeric literal or punctuation can match the empty regex suffix.
        // Do not offer every global symbol while typing ordinary numbers.
        if(path.isEmpty() && prefix.isEmpty() && !explicitRequest) {
            m_completer->popup()->hide();return;
        }
        const QString resolved=path.isEmpty() ? path :
            CompletionTypeResolver::resolvePath(beforeFull,path);
        members=resolved.isEmpty() && !path.isEmpty()
            ? QVector<CompletionCatalog::Item>{} : CompletionCatalog::entries(resolved);
        m_completionPath=resolved;
        // An imported ES module is an explicit namespace, not a Wavegen/Scope
        // API type. Read named exports from its source without importing it.
        // Only a direct identifier such as `math.` is supported here.
        if (members.isEmpty() && !path.contains(QLatin1Char('.')) && m_moduleSourceResolver) {
            const QString module=ModuleExportCatalog::moduleForAlias(beforeFull,path);
            if (!module.isEmpty()) {
                m_completionIsModule=true;
                const QString source=m_moduleSourceResolver(module);
                for (const auto& exp:ModuleExportCatalog::namedExports(source)) {
                    members.append(exp.item);
                    m_moduleCompletionDocs.insert(exp.item.name,
                        ModuleExportCatalog::helpHtml({path,module,exp}));
                }
            }
        }
        if(path.isEmpty()) {
            for(const auto& snip:ApiKnowledge::snippets())
                members.append({snip.trigger,CompletionCatalog::Kind::Snippet,snip.description});
        }
    }
    if (members.isEmpty()) {m_completer->popup()->hide();return;}
    std::sort(members.begin(),members.end(),[](const auto& a,const auto& b){
        return a.name.compare(b.name,Qt::CaseInsensitive)<0;
    });
    m_completionModel->clear();
    for(const auto& entry:members) {
        auto* item=new QStandardItem(completionIcon(entry.kind),entry.name);
        item->setEditable(false);
        item->setData(entry.name,Qt::EditRole);
        const QString full=m_completionPath.isEmpty() ? entry.name : m_completionPath+QLatin1Char('.')+entry.name;
        const auto doc=ApiKnowledge::lookup(full);
        item->setToolTip(doc.known ? ApiKnowledge::html(doc,m_dark) :
            m_moduleCompletionDocs.value(entry.name,entry.description));
        m_completionModel->appendRow(item);
    }
    m_completer->setCompletionPrefix(prefix);
    if (m_completer->completionCount()==0) {m_completer->popup()->hide();return;}
    QRect r=cursorRect();
    r.setWidth(qMax(320,m_completer->popup()->sizeHintForColumn(0)+
        m_completer->popup()->verticalScrollBar()->sizeHint().width()+24));
    m_completer->complete(r);
    // Explicit first filtered selection for keyboard-only completion.
    if (m_completer->setCurrentRow(0)) {
        const QModelIndex first=m_completer->completionModel()->index(0,0);
        if(first.isValid()) {
            m_completer->popup()->setCurrentIndex(first);
            if(auto* selection=m_completer->popup()->selectionModel())
                selection->setCurrentIndex(first,QItemSelectionModel::ClearAndSelect);
        }
    }
    showCompletionDocumentation();
}

void ScriptEditor::showCompletionDocumentation() {
    if(!m_completer->popup()->isVisible() || !m_hintsEnabled) return;
    const QModelIndex selected=m_completer->popup()->currentIndex();
    if(!selected.isValid()) return;
    const QString name=selected.data(Qt::EditRole).toString();
    const auto snip=ApiKnowledge::snippet(name);
    QString content;
    if(!snip.trigger.isEmpty() && !m_completionIsModule && m_completionPath.isEmpty()) {
        content=QStringLiteral("<b>Snippet: %1</b><p>%2</p><pre>%3</pre><small>Enter/Tab inserts this template. Tab advances editable fields.</small>")
            .arg(snip.trigger.toHtmlEscaped(),snip.description.toHtmlEscaped(),snip.body.toHtmlEscaped());
    } else {
        const QString path=m_valueCompletion ? m_completionPath :
            (m_completionPath.isEmpty()?name:m_completionPath+QLatin1Char('.')+name);
        const auto doc=ApiKnowledge::lookup(path);
        if (!doc.known) {
            content=m_moduleCompletionDocs.value(name);
            if(content.isEmpty()) { QToolTip::hideText();return; }
        } else content=ApiKnowledge::html(doc,false); // QToolTip uses a light native palette
    }
    // Non-modal, keyboard-friendly documentation. The selection remains in the
    // QCompleter popup so Up/Down and Return/Tab keep working.
    const QPoint where=m_completer->popup()->mapToGlobal(
        QPoint(m_completer->popup()->width()+8,4));
    QToolTip::showText(where,content,m_completer->popup());
}

QString ScriptEditor::pathAtCursor() const {
    const QString source=document()->toPlainText();
    const int pos=textCursor().position();
    auto identifier=[](QChar c){return c.isLetterOrNumber() || c=='_' || c=='$' || c=='.';};
    int a=pos,b=pos;
    while(a>0 && identifier(source.at(a-1))) --a;
    while(b<source.size() && identifier(source.at(b))) ++b;
    QString path=source.mid(a,b-a);
    while(path.startsWith('.')) path.remove(0,1);
    while(path.endsWith('.')) path.chop(1);
    if(path.isEmpty()) return {};
    const QString resolved=CompletionTypeResolver::resolvePath(source.left(pos),path);
    if(!resolved.isEmpty() && ApiKnowledge::isDocumentedSymbol(resolved)) return resolved;
    return ApiKnowledge::isDocumentedSymbol(path) ? path : QString();
}

void ScriptEditor::showApiHelpAtCursor() { showApiHelp(); }

void ScriptEditor::showApiHelp() {
    QString path;
    if(m_completer->popup()->isVisible()) {
        const QModelIndex selected=m_completer->popup()->currentIndex();
        const QString selectedName=selected.data(Qt::EditRole).toString();
        const auto snip=ApiKnowledge::snippet(selectedName);
        if(!snip.trigger.isEmpty() && !m_completionIsModule) {
            // Snippet help is available in the completion popup; F1 can also
            // show its full, editable source without inserting it.
            QDialog dialog(this);
            dialog.setWindowTitle(QStringLiteral("WFScript snippet: %1").arg(snip.trigger));
            dialog.resize(580,370);
            auto* layout=new QVBoxLayout(&dialog);
            auto* browser=new QTextBrowser(&dialog);
            browser->setStyleSheet(m_dark ? QStringLiteral("background:#17212e;color:#e3ecf6;") : QStringLiteral("background:white;color:#19212b;"));
            browser->setHtml(QStringLiteral("<h2>%1</h2><p>%2</p><pre>%3</pre>")
                .arg(snip.trigger.toHtmlEscaped(),snip.description.toHtmlEscaped(),snip.body.toHtmlEscaped()));
            layout->addWidget(browser);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
            connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::accept);
            layout->addWidget(buttons);
            m_completer->popup()->hide();QToolTip::hideText();dialog.exec();return;
        }
        path=m_valueCompletion ? m_completionPath :
            (m_completionPath.isEmpty()?selectedName:m_completionPath+QLatin1Char('.')+selectedName);
    }
    if(path.isEmpty()) path=pathAtCursor();
    const auto doc=ApiKnowledge::lookup(path);
    // Resolve the imported module member at the caret even when the popup is
    // closed. Autocomplete and F1 both consume ModuleExportCatalog::namedExports.
    const auto resolvedModule=ModuleExportCatalog::resolveAtCursor(
        toPlainText(),textCursor().position(),m_moduleSourceResolver);
    const QModelIndex current=m_completer->popup()->currentIndex();
    const QString completionName=current.data(Qt::EditRole).toString();
    const QString popupModuleDoc=m_completer->popup()->isVisible() && m_completionIsModule
        ? m_moduleCompletionDocs.value(completionName) : QString();
    const QString moduleDoc=!popupModuleDoc.isEmpty() ? popupModuleDoc :
        (resolvedModule ? ModuleExportCatalog::helpHtml(*resolvedModule) : QString());
    QDialog dialog(this);
    dialog.setWindowTitle(doc.known
        ? QStringLiteral("WFScript API Help — ")+doc.path
        : !moduleDoc.isEmpty() ? QStringLiteral("WFScript Module Help")
                               : QStringLiteral("WFScript API Help"));
    dialog.resize(650,440);
    auto* layout=new QVBoxLayout(&dialog);
    auto* browser=new QTextBrowser(&dialog);
    browser->setStyleSheet(m_dark ? QStringLiteral("background:#17212e;color:#e3ecf6;") : QStringLiteral("background:white;color:#19212b;"));
    browser->setHtml(doc.known ? ApiKnowledge::html(doc,m_dark) :
        !moduleDoc.isEmpty() ? moduleDoc :
        QStringLiteral("<h2>WFScript API Help</h2><p>Place the cursor on a supported Scope, Wavegen, Device, or ui member and press F1.</p>"
                       "<p>Use Ctrl+Space to browse members, type a snippet name such as <b>scopecapture</b>, then press Tab or Enter to insert it.</p>"
                       "<p>Signature hints: Ctrl+Shift+Space inside a function call.</p>"));
    layout->addWidget(browser);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::accept);
    layout->addWidget(buttons);
    m_completer->popup()->hide();QToolTip::hideText();dialog.exec();
}

void ScriptEditor::insertSnippet(const QString& body) {
    // Expand ${1:value}, ${2:value}, etc. into text while tracking editable
    // ranges via QTextCursor anchors that follow subsequent user edits.
    static const QRegularExpression field(QStringLiteral(R"(\$\{([1-9][0-9]*):([^}]*)\})"));
    struct Offset { int n;int begin;int length; };
    QVector<Offset> offsets;
    QString plain;
    int read=0;
    auto it=field.globalMatch(body);
    while(it.hasNext()) {
        auto m=it.next();
        plain+=body.mid(read,m.capturedStart()-read);
        offsets.append({m.captured(1).toInt(),static_cast<int>(plain.size()),
                        static_cast<int>(m.captured(2).size())});
        plain+=m.captured(2);
        read=m.capturedEnd();
    }
    plain+=body.mid(read);
    QTextCursor cursor=textCursor();
    const QString line=cursor.block().text();
    QString baseIndent;
    for(const QChar c:line) {if(c==' ' || c=='\t') baseIndent+=c;else break;}
    if(!baseIndent.isEmpty()) {
        for(auto& o:offsets) o.begin+=plain.left(o.begin).count(QLatin1Char('\n'))*baseIndent.size();
        plain.replace(QStringLiteral("\n"),QStringLiteral("\n")+baseIndent);
        if(plain.endsWith(baseIndent)) plain.chop(baseIndent.size());
    }
    cursor.select(QTextCursor::WordUnderCursor);
    const int start=cursor.selectionStart();
    cursor.insertText(plain);
    setTextCursor(cursor);
    m_snippetStops.clear();m_snippetIndex=-1;
    for(const auto& o:offsets) {
        QTextCursor begin(document()),end(document());
        begin.setPosition(start+o.begin);
        begin.setKeepPositionOnInsert(true);
        end.setPosition(start+o.begin+o.length);
        m_snippetStops.append({o.n,begin,end});
    }
    std::sort(m_snippetStops.begin(),m_snippetStops.end(),[](const auto& a,const auto& b){return a.number<b.number;});
    if(!m_snippetStops.isEmpty()) {
        m_snippetIndex=0;
        QTextCursor selection(document());
        selection.setPosition(m_snippetStops[0].start.position());
        selection.setPosition(m_snippetStops[0].end.position(),QTextCursor::KeepAnchor);
        setTextCursor(selection);
    }
}

bool ScriptEditor::nextSnippetStop() {
    if(m_snippetIndex<0 || m_snippetIndex>=m_snippetStops.size()) return false;
    const auto& old=m_snippetStops.at(m_snippetIndex);
    const int caret=textCursor().position();
    if(caret<old.start.position() || caret>old.end.position()) {
        m_snippetIndex=-1;m_snippetStops.clear();return false;
    }
    ++m_snippetIndex;
    if(m_snippetIndex>=m_snippetStops.size()) {
        m_snippetIndex=-1;m_snippetStops.clear();return true;
    }
    const auto& next=m_snippetStops.at(m_snippetIndex);
    QTextCursor target(document());
    target.setPosition(next.start.position());
    target.setPosition(next.end.position(),QTextCursor::KeepAnchor);
    setTextCursor(target);
    return true;
}

void ScriptEditor::updateSignatureHelp() {
    if (!m_hintsEnabled) { QToolTip::hideText();return; }
    const QString before = document()->toPlainText().left(textCursor().position());
    auto active=EditorAssist::callContext(before);
    if (!active.valid) {QToolTip::hideText();return;}
    const int lastDot=active.apiPath.lastIndexOf(QLatin1Char('.'));
    const QString base=active.apiPath.left(lastDot);
    const QString parent=CompletionTypeResolver::resolvePath(before,base);
    const QString path=parent.isEmpty() ? active.apiPath : parent+active.apiPath.mid(lastDot);
    const QString text=EditorAssist::signature(path,active.argument);
    if(text.isEmpty()) {QToolTip::hideText();return;}
    QToolTip::showText(mapToGlobal(cursorRect().bottomRight()),text,this);
}

// Ctrl+/ comments or uncomments every selected line together.
void ScriptEditor::toggleLineComments() {
    QTextCursor cursor=textCursor();
    const int start=cursor.selectionStart(),end=cursor.selectionEnd();
    QTextBlock first=document()->findBlock(start);
    QTextBlock last=document()->findBlock(qMax(start,end-1));
    if(!first.isValid() || !last.isValid()) return;
    bool allCommented=true;
    for(QTextBlock b=first;b.isValid();b=b.next()) {
        if(!b.text().trimmed().isEmpty() && !b.text().trimmed().startsWith(QStringLiteral("//")))
            allCommented=false;
        if(b==last) break;
    }
    cursor.beginEditBlock();
    for(QTextBlock b=first;b.isValid();) {
        const QString line=b.text();
        int lead=0;while(lead<line.size() && (line.at(lead)==' ' || line.at(lead)=='\t')) ++lead;
        QTextCursor edit(b);edit.movePosition(QTextCursor::Right,QTextCursor::MoveAnchor,lead);
        if(allCommented) {
            if(line.mid(lead).startsWith(QStringLiteral("//"))) {
                edit.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,2);
                edit.removeSelectedText();
            }
        } else if(!line.trimmed().isEmpty()) edit.insertText(QStringLiteral("//"));
        if(b==last) break;
        b=b.next();
    }
    cursor.endEditBlock();
    m_completer->popup()->hide();
}

// Tab/Shift+Tab work on selections or a single line. No injected tab character.
void ScriptEditor::shiftIndent(bool outdent) {
    QTextCursor cursor=textCursor();
    const int start=cursor.selectionStart(),end=cursor.selectionEnd();
    QTextBlock first=document()->findBlock(start);
    QTextBlock last=document()->findBlock(qMax(start,end-1));
    if(!first.isValid() || !last.isValid()) return;
    cursor.beginEditBlock();
    for(QTextBlock b=first;b.isValid();) {
        QTextCursor edit(b);
        if(outdent) {
            const QString line=b.text();
            int n=0;
            if(line.startsWith(QLatin1Char('\t'))) n=1;
            else while(n<4 && n<line.size() && line.at(n)==QLatin1Char(' ')) ++n;
            if(n) {edit.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,n);edit.removeSelectedText();}
        } else edit.insertText(QStringLiteral("    "));
        if(b==last) break;
        b=b.next();
    }
    cursor.endEditBlock();
}

bool ScriptEditor::handleSmartPair(QKeyEvent* event) {
    if (event->modifiers().testFlag(Qt::ControlModifier) ||
        event->modifiers().testFlag(Qt::AltModifier) ||
        event->modifiers().testFlag(Qt::MetaModifier) || event->text().size()!=1) return false;
    const QChar typed=event->text().at(0);
    const QString opening=QStringLiteral("([{\"'`"), closing=QStringLiteral(")] }\"'`").remove(QLatin1Char(' '));
    QTextCursor cursor=textCursor();
    const QString source=document()->toPlainText();
    const int pos=cursor.position();
    const int openAt=opening.indexOf(typed);
    const int closeAt=closing.indexOf(typed);
    const bool code=EditorAssist::inCode(source,pos);

    // Replace selection by a balanced pair and keep the selection inside.
    if(cursor.hasSelection() && openAt>=0 && code) {
        const QString selection=cursor.selectedText();
        const int start=cursor.selectionStart();
        cursor.insertText(QString(typed)+selection+QString(closing.at(openAt)));
        cursor.setPosition(start+1);
        cursor.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,selection.size());
        setTextCursor(cursor);
        return true;
    }
    // A typed close character passes over an existing mate, including quotes.
    if(closeAt>=0 && pos<source.size() && source.at(pos)==typed) {
        cursor.movePosition(QTextCursor::Right);setTextCursor(cursor);return true;
    }
    if(openAt>=0 && code) {
        // Only pair when not immediately preceding an identifier/number.
        if(pos<source.size() && (source.at(pos).isLetterOrNumber() || source.at(pos)=='_') &&
           (typed=='\"' || typed=='\'' || typed=='`')) return false;
        cursor.insertText(QString(typed)+QString(closing.at(openAt)));
        cursor.movePosition(QTextCursor::Left);
        setTextCursor(cursor);
        return true;
    }
    return false;
}

void ScriptEditor::keyPressEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_F1) {showApiHelp();event->accept();return;}
    if(event->key()==Qt::Key_Tab && !m_completer->popup()->isVisible() &&
       !event->modifiers().testFlag(Qt::ShiftModifier) && nextSnippetStop()) {
        event->accept();return;
    }
    if (m_completer->popup()->isVisible()) {
        // QCompleter normally consumes Enter/Tab through its popup event filter.
        // It does not always receive an event that was delivered directly to
        // the editor (for example by GUI automation or certain focus routes).
        // If the key reaches our editor, accept the *already selected* row
        // rather than silently ignoring it. The QCompleter::activated signal
        // remains the normal route when Qt intercepts the key first.
        switch(event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab: {
            const QModelIndex current=m_completer->popup()->currentIndex();
            const QString completion=current.data(Qt::EditRole).toString();
            if(current.isValid() && !completion.isEmpty()) {
                m_completer->popup()->hide();
                QToolTip::hideText();
                insertCompletion(completion);
                event->accept();
                return;
            }
            // No selection: keep Qt's existing popup event-filter behavior.
            event->ignore();
            return;
        }
        case Qt::Key_Escape:
            m_completer->popup()->hide();
            QToolTip::hideText();
            event->accept();
            return;
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:break;
        }
    }
    if(event->key()==Qt::Key_Space && event->modifiers()==Qt::ControlModifier) {
        showCompletions(true);return;
    }
    if(event->key()==Qt::Key_Space && event->modifiers()==(Qt::ControlModifier|Qt::ShiftModifier)) {
        updateSignatureHelp();return;
    }
    if(event->key()==Qt::Key_Slash && event->modifiers()==Qt::ControlModifier) {
        toggleLineComments();return;
    }
    if(event->modifiers()==Qt::ControlModifier &&
       (event->key()==Qt::Key_BracketLeft || event->key()==Qt::Key_BracketRight)) {
        shiftIndent(event->key()==Qt::Key_BracketLeft);return;
    }
    if(event->key()==Qt::Key_Backtab ||
       (event->key()==Qt::Key_Tab && event->modifiers().testFlag(Qt::ShiftModifier))) {
        shiftIndent(true);return;
    }
    if(event->key()==Qt::Key_Tab && !m_completer->popup()->isVisible()) {
        if(textCursor().hasSelection()) shiftIndent(false);
        else insertPlainText(QStringLiteral("    "));
        return;
    }
    if(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter) {
        const QTextCursor cursor=textCursor();
        const QString line=cursor.block().text();
        QString indent;
        for(QChar c:line) {if(c==' ' || c=='\t') indent+=c;else break;}
        const QString left=line.left(cursor.positionInBlock()).trimmed();
        const QString right=line.mid(cursor.positionInBlock()).trimmed();
        const bool opens=left.endsWith(QLatin1Char('{'));
        const bool between=opens && right.startsWith(QLatin1Char('}'));
        QPlainTextEdit::keyPressEvent(event);
        if(between) {
            insertPlainText(indent+QStringLiteral("    ")+QStringLiteral("\n")+indent);
            QTextCursor c=textCursor();
            c.movePosition(QTextCursor::Up);
            c.movePosition(QTextCursor::EndOfLine);
            setTextCursor(c);
        } else insertPlainText(indent+(opens ? QStringLiteral("    ") : QString()));
        QToolTip::hideText();return;
    }
    if(event->key()==Qt::Key_Backspace && !textCursor().hasSelection()) {
        const QString s=document()->toPlainText();
        const int pos=textCursor().position();
        const QString pairs=QStringLiteral("()[]{}\"\"''``");
        if(pos>0 && pos<s.size()) {
            bool paired=false;
            for(int i=0;i+1<pairs.size();i+=2)
                if(s.at(pos-1)==pairs.at(i) && s.at(pos)==pairs.at(i+1)) {paired=true;break;}
            if(paired) {
                QTextCursor c=textCursor();c.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor);
                c.removeSelectedText();setTextCursor(c);
            }
        }
    }
    const bool wasVisible=m_completer->popup()->isVisible();
    bool paired=false;
    if (!isReadOnly()) paired=handleSmartPair(event);
    if(!paired) QPlainTextEdit::keyPressEvent(event);
    if(!m_hintsEnabled) {m_completer->popup()->hide();return;}
    const QString input=event->text();
    const bool letter=input.size()==1 &&
        (input.at(0).isLetterOrNumber() || input==QStringLiteral("_") || input==QStringLiteral("$"));
    const bool dot=input==QStringLiteral(".");
    const bool quoted=input==QStringLiteral("\"") || input==QStringLiteral("'");
    const bool backspace=event->key()==Qt::Key_Backspace;
    const bool normal=!event->modifiers().testFlag(Qt::ControlModifier) &&
        !event->modifiers().testFlag(Qt::AltModifier) &&
        !event->modifiers().testFlag(Qt::MetaModifier);
    if ((normal && (letter || dot || quoted || (m_valueCompletion && input==QStringLiteral(" ")))) ||
        (backspace && (wasVisible ||
                        textCursor().block().text().left(textCursor().positionInBlock()).contains('.')))) {
        showCompletions();
    } else m_completer->popup()->hide();
    if(input==QStringLiteral("(") || input==QStringLiteral(",")) updateSignatureHelp();
    else if(input==QStringLiteral(")") || input==QStringLiteral(";")) QToolTip::hideText();
}
