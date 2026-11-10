#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

// Editor-only JavaScript lexer. Tracks lexical context across QTextBlocks,
// including nested ${...} template expressions. Does not parse/execute code.
class JavaScriptHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit JavaScriptHighlighter(QTextDocument* document);
    void setDarkTheme(bool dark);

protected:
    void highlightBlock(const QString& text) override;

private:
    bool m_dark = true;
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_globalFormat;
    QTextCharFormat m_objectFormat;
    QTextCharFormat m_propertyFormat;
    QTextCharFormat m_constantFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_templateDelimiterFormat;
    QTextCharFormat m_regexFormat;
    QTextCharFormat m_commentFormat;
    void initializeFormats();
};
