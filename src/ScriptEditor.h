#pragma once

#include <QPlainTextEdit>
#include <QPointer>
#include <QTextCursor>
#include <QVector>
#include "ApiDiagnostics.h"
#include <functional>
#include <QHash>

class QWidget;
class QCompleter;
class QStandardItemModel;
class QDialog;
class JavaScriptHighlighter;
class LineNumberArea;
class QTimer;

class ScriptEditor final : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit ScriptEditor(QWidget* parent = nullptr);

    int lineNumberAreaWidth() const;
    void paintLineNumberArea(QPaintEvent* event);
    void setDarkTheme(bool dark);
    void setHintsEnabled(bool enabled);
    // Editor-only source resolver for user .mjs exports; never executes code.
    void setModuleSourceResolver(std::function<QString(const QString&)> resolver);
    void highlightErrorLine(int oneBasedLine);
    void showApiHelpAtCursor();
    void insertApiPath(const QString& path);
    void goToLine(int oneBasedLine);
    const QVector<ApiDiagnostics::Issue>& diagnostics() const { return m_issues; }
signals:
    void diagnosticsChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();
    void insertCompletion(const QString& completion);

private:
    friend class LineNumberArea;
    QWidget* m_gutter = nullptr;
    QCompleter* m_completer = nullptr;
    QStandardItemModel* m_completionModel = nullptr;
    JavaScriptHighlighter* m_highlighter = nullptr;
    bool m_dark = true;
    bool m_hintsEnabled = true;
    bool m_valueCompletion = false;
    QString m_valuePrefix;
    QString m_completionPath;
    bool m_completionIsModule=false;
    std::function<QString(const QString&)> m_moduleSourceResolver;
    QHash<QString,QString> m_moduleCompletionDocs;
    struct SnippetStop { int number=0; QTextCursor start; QTextCursor end; };
    QVector<SnippetStop> m_snippetStops;
    int m_snippetIndex=-1;
    int m_errorLine = -1;
    QVector<ApiDiagnostics::Issue> m_issues;
    QTimer* m_diagnosticTimer = nullptr;
    void showCompletions(bool explicitRequest=false);
    void showCompletionDocumentation();
    void showApiHelp();
    QString pathAtCursor() const;
    void insertSnippet(const QString& text);
    bool nextSnippetStop();
    void updateSignatureHelp();
    void toggleLineComments();
    void shiftIndent(bool outdent);
    bool handleSmartPair(QKeyEvent* event);
};
