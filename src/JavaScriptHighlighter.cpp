#include "JavaScriptHighlighter.h"

#include <QColor>
#include <QFont>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextDocument>
#include <QVector>

namespace {
QTextCharFormat colored(const char* dark, const char* light, bool isDark, bool bold = false) {
    QTextCharFormat result;
    result.setForeground(QColor(isDark ? dark : light));
    if (bold) result.setFontWeight(QFont::DemiBold);
    return result;
}

enum class Mode : quint8 { Code, BlockComment, SingleQuote, DoubleQuote, Template, Interpolation };
struct Context {
    Mode mode = Mode::Code;
    int braceDepth = 0;  // Nested { } inside a ${...} expression.
    bool expressionExpected = true; // Distinguish /regexp/ from division.
};
class LexerData final : public QTextBlockUserData {
public:
    QVector<Context> stack;
};

// A stable state fingerprint tells QSyntaxHighlighter to re-highlight later
// blocks when the template/comment nesting of this block changes. The full
// stack is stored in QTextBlockUserData, not squeezed into a single int.
int stateFingerprint(const QVector<Context>& stack) {
    quint32 h = 2166136261u;
    for (const Context& c : stack) {
        h = (h ^ (quint32(c.mode) + 1u)) * 16777619u;
        h = (h ^ quint32(c.braceDepth)) * 16777619u;
        h = (h ^ quint32(c.expressionExpected ? 1 : 0)) * 16777619u;
    }
    return int(h & 0x7fffffffu);
}

bool identifierStart(QChar ch) {
    return ch == QLatin1Char('_') || ch == QLatin1Char('$') || ch.isLetter();
}
bool identifierPart(QChar ch) { return identifierStart(ch) || ch.isDigit(); }

const QSet<QString>& keywords() {
    static const QSet<QString> words = {
        QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("debugger"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("export"), QStringLiteral("extends"),
        QStringLiteral("finally"), QStringLiteral("for"), QStringLiteral("from"),
        QStringLiteral("function"), QStringLiteral("get"), QStringLiteral("if"),
        QStringLiteral("import"), QStringLiteral("in"), QStringLiteral("instanceof"),
        QStringLiteral("let"), QStringLiteral("new"), QStringLiteral("of"),
        QStringLiteral("return"), QStringLiteral("set"), QStringLiteral("static"),
        QStringLiteral("super"), QStringLiteral("switch"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("typeof"),
        QStringLiteral("var"), QStringLiteral("void"), QStringLiteral("while"),
        QStringLiteral("with"), QStringLiteral("yield")
    };
    return words;
}
const QSet<QString>& expressionKeywords() {
    static const QSet<QString> words = {
        QStringLiteral("return"), QStringLiteral("throw"), QStringLiteral("case"),
        QStringLiteral("delete"), QStringLiteral("typeof"), QStringLiteral("void"),
        QStringLiteral("new"), QStringLiteral("await"), QStringLiteral("yield"),
        QStringLiteral("in"), QStringLiteral("instanceof"), QStringLiteral("of")
    };
    return words;
}
const QSet<QString>& constants() {
    static const QSet<QString> words = {
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("undefined"), QStringLiteral("NaN"), QStringLiteral("Infinity")
    };
    return words;
}
const QSet<QString>& globals() {
    static const QSet<QString> words = {
        QStringLiteral("Device"), QStringLiteral("Scope"), QStringLiteral("Scope1"),
        QStringLiteral("Wavegen"), QStringLiteral("Wavegen1"),
        QStringLiteral("Supplies"), QStringLiteral("Supplies1"), QStringLiteral("StaticIO"), QStringLiteral("Patterns"), QStringLiteral("Patterns1"),
        QStringLiteral("Logic"), QStringLiteral("Logic1"), QStringLiteral("ui"),
        QStringLiteral("Math"), QStringLiteral("JSON"), QStringLiteral("console"),
        QStringLiteral("Array"), QStringLiteral("Object"), QStringLiteral("Date"),
        QStringLiteral("String"), QStringLiteral("Number"), QStringLiteral("Boolean"),
        QStringLiteral("RegExp"), QStringLiteral("Promise"), QStringLiteral("Error")
    };
    return words;
}
const QSet<QString>& objectMembers() {
    static const QSet<QString> words = {
        QStringLiteral("Time"), QStringLiteral("Trigger"),
        QStringLiteral("Channel1"), QStringLiteral("Channel2"),
        QStringLiteral("Mode"), QStringLiteral("Simple"), QStringLiteral("Custom"),
        QStringLiteral("Burst"), QStringLiteral("Modulation"),
        QStringLiteral("FM"), QStringLiteral("AM"), QStringLiteral("Sweep"),
        QStringLiteral("Frequency"), QStringLiteral("Amplitude"),
        QStringLiteral("Period"), QStringLiteral("Offset"), QStringLiteral("Range"),
        QStringLiteral("Attenuation"), QStringLiteral("Rate"),
        QStringLiteral("Samples"), QStringLiteral("SampleMode"),
        QStringLiteral("Source"), QStringLiteral("Condition"),
        QStringLiteral("Level"), QStringLiteral("Hysteresis"),
        QStringLiteral("HoldOff"), QStringLiteral("LCondition"),
        QStringLiteral("Length"), QStringLiteral("Filter"),
        QStringLiteral("Type"), QStringLiteral("Play"),
        QStringLiteral("Run"), QStringLiteral("Wait"), QStringLiteral("Repeat"),
        QStringLiteral("Master"), QStringLiteral("Symmetry"),
        QStringLiteral("Phase"), QStringLiteral("RecordLength"),
        QStringLiteral("TriggerPositionPercent"), QStringLiteral("Idle"),
        QStringLiteral("Start"), QStringLiteral("Stop"), QStringLiteral("Depth")
    };
    return words;
}
const QSet<QString>& propertyMembers() {
    static const QSet<QString> words = {
        QStringLiteral("value"), QStringLiteral("real"), QStringLiteral("text"),
        QStringLiteral("preset"), QStringLiteral("enable"),
        QStringLiteral("lastError"), QStringLiteral("length"),
        QStringLiteral("data"), QStringLiteral("name"), QStringLiteral("SN"),
        QStringLiteral("taken"), QStringLiteral("takenEpochMs"),
        QStringLiteral("timestampSource"), QStringLiteral("recordLostSamples"),
        QStringLiteral("recordCorruptSamples"), QStringLiteral("lastSettleMs"),
        QStringLiteral("configurationCount"),
        QStringLiteral("triggerConfigurationCount"),
        QStringLiteral("acquisitionCount"), QStringLiteral("RepeatTrigger")
    };
    return words;
}

// JS numeric literals (hex/binary/octal/decimal, separators, exponent, bigint).
// Anchored at the candidate character by the match call below.
const QRegularExpression& numberPattern() {
    static const QRegularExpression pattern(QStringLiteral(
        R"((?:0[xX][0-9a-fA-F](?:_?[0-9a-fA-F])*n?|0[bB][01](?:_?[01])*n?|0[oO][0-7](?:_?[0-7])*n?|(?:\d(?:_?\d)*(?:\.(?:\d(?:_?\d)*)?)?|\.\d(?:_?\d)*)(?:[eE][+-]?\d(?:_?\d)*)?n?))"));
    return pattern;
}

// Returns the index following a /regex/flags literal, or -1. Respects
// escapes and character classes, so // in /https?:\/\/host/ is not a comment.
int regexpEnd(const QString& text, int slash) {
    bool escaped = false, inClass = false;
    for (int i = slash + 1; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (escaped) { escaped = false; continue; }
        if (c == QLatin1Char('\\')) { escaped = true; continue; }
        if (c == QLatin1Char('[')) inClass = true;
        else if (c == QLatin1Char(']')) inClass = false;
        else if (c == QLatin1Char('/') && !inClass) {
            ++i;
            while (i < text.size() && identifierPart(text.at(i))) ++i;
            return i;
        }
    }
    return -1;
}
} // namespace

JavaScriptHighlighter::JavaScriptHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document) {
    initializeFormats();
}

void JavaScriptHighlighter::setDarkTheme(bool dark) {
    if (m_dark == dark) return;
    m_dark = dark;
    initializeFormats();
    rehighlight();
}

void JavaScriptHighlighter::initializeFormats() {
    m_keywordFormat = colored("#c792ea", "#803ab7", m_dark, true);
    m_globalFormat = colored("#82aaff", "#0758a7", m_dark);
    m_objectFormat = colored("#a1c4ff", "#24568e", m_dark);
    m_propertyFormat = colored("#89ddc7", "#087d69", m_dark);
    m_constantFormat = colored("#ffcb6b", "#a66505", m_dark);
    m_numberFormat = colored("#f7bd73", "#9b5b08", m_dark);
    m_functionFormat = colored("#82cfff", "#04688f", m_dark);
    m_stringFormat = colored("#c3e88d", "#35730e", m_dark);
    m_templateDelimiterFormat = colored("#d6b6ff", "#803ab7", m_dark);
    m_regexFormat = colored("#c3e88d", "#35730e", m_dark);
    m_commentFormat = colored("#7d8c98", "#74808b", m_dark);
}

void JavaScriptHighlighter::highlightBlock(const QString& text) {
    QVector<Context> stack;
    const QTextBlock preceding = currentBlock().previous();
    if (preceding.isValid()) {
        if (const auto* state = dynamic_cast<const LexerData*>(preceding.userData()))
            stack = state->stack;
    }
    if (stack.isEmpty()) stack.append(Context{});

    const int length = text.size();
    int i = 0;
    while (i < length) {
        const Mode mode = stack.last().mode;

        if (mode == Mode::BlockComment) {
            const int end = text.indexOf(QStringLiteral("*/"), i);
            const int to = end < 0 ? length : end + 2;
            setFormat(i, to-i, m_commentFormat);
            i = to;
            if (end >= 0) stack.removeLast();
            continue;
        }
        if (mode == Mode::SingleQuote || mode == Mode::DoubleQuote) {
            const QChar quote = mode == Mode::SingleQuote ? QLatin1Char('\'') : QLatin1Char('"');
            const int start = i;
            bool escaped = false, closed = false;
            for (; i < length; ++i) {
                const QChar c = text.at(i);
                if (escaped) { escaped = false; continue; }
                if (c == QLatin1Char('\\')) { escaped = true; continue; }
                if (c == quote) { ++i; closed = true; break; }
            }
            setFormat(start, i-start, m_stringFormat);
            // JavaScript supports quoted-string continuation only with a
            // trailing unescaped backslash; do not poison the next line when
            // the user is merely editing an unterminated string.
            if (closed || !escaped) stack.removeLast();
            continue;
        }
        if (mode == Mode::Template) {
            const int start = i;
            bool escaped = false;
            while (i < length) {
                const QChar c = text.at(i);
                if (escaped) { escaped = false; ++i; continue; }
                if (c == QLatin1Char('\\')) { escaped = true; ++i; continue; }
                if (c == QLatin1Char('`')) {
                    ++i;
                    setFormat(start, i-start, m_stringFormat);
                    stack.removeLast();
                    stack.last().expressionExpected = false;
                    break;
                }
                if (c == QLatin1Char('$') && i+1 < length &&
                    text.at(i+1) == QLatin1Char('{')) {
                    setFormat(start, i-start, m_stringFormat);
                    setFormat(i, 2, m_templateDelimiterFormat);
                    i += 2;
                    stack.append({Mode::Interpolation, 0, true});
                    break;
                }
                ++i;
            }
            if (i == length && stack.last().mode == Mode::Template)
                setFormat(start, i-start, m_stringFormat);
            continue;
        }

        // Mode::Code and Mode::Interpolation use the same JS tokenization.
        if (mode == Mode::Interpolation && text.at(i) == QLatin1Char('}')) {
            if (stack.last().braceDepth == 0) {
                setFormat(i, 1, m_templateDelimiterFormat);
                ++i;
                stack.removeLast();
                continue;
            }
            --stack.last().braceDepth;
            stack.last().expressionExpected = false;
            ++i;
            continue;
        }
        const QChar c = text.at(i);
        if (c.isSpace()) { ++i; continue; }
        if (c == QLatin1Char('/') && i+1 < length) {
            const QChar next = text.at(i+1);
            if (next == QLatin1Char('/')) {
                setFormat(i, length-i, m_commentFormat);
                i = length;
                break;
            }
            if (next == QLatin1Char('*')) {
                stack.append({Mode::BlockComment, 0, true});
                continue;
            }
            if (stack.last().expressionExpected) {
                const int end = regexpEnd(text, i);
                if (end > i) {
                    setFormat(i, end-i, m_regexFormat);
                    i = end;
                    stack.last().expressionExpected = false;
                    continue;
                }
            }
        }
        if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
            stack.last().expressionExpected = false;
            setFormat(i, 1, m_stringFormat);
            stack.append({c == QLatin1Char('\'') ? Mode::SingleQuote : Mode::DoubleQuote, 0, false});
            ++i;
            continue;
        }
        if (c == QLatin1Char('`')) {
            setFormat(i, 1, m_stringFormat);
            stack.append({Mode::Template, 0, false});
            ++i;
            continue;
        }
        if (identifierStart(c)) {
            const int start = i++;
            while (i < length && identifierPart(text.at(i))) ++i;
            const QString word = text.mid(start, i-start);
            int before = start-1, after = i;
            while (before >= 0 && text.at(before).isSpace()) --before;
            while (after < length && text.at(after).isSpace()) ++after;
            const bool member = before >= 0 && text.at(before) == QLatin1Char('.');
            const bool call = after < length && text.at(after) == QLatin1Char('(');

            // Keywords must win even when directly followed by '('.
            if (!member && keywords().contains(word)) {
                setFormat(start, i-start, m_keywordFormat);
                stack.last().expressionExpected = expressionKeywords().contains(word);
            } else if (!member && constants().contains(word)) {
                setFormat(start, i-start, m_constantFormat);
                stack.last().expressionExpected = false;
            } else if (!member && globals().contains(word)) {
                setFormat(start, i-start, m_globalFormat);
                stack.last().expressionExpected = false;
            } else if (call) {
                setFormat(start, i-start, m_functionFormat);
                stack.last().expressionExpected = false;
            } else if (member && propertyMembers().contains(word)) {
                setFormat(start, i-start, m_propertyFormat);
                stack.last().expressionExpected = false;
            } else if (member && objectMembers().contains(word)) {
                setFormat(start, i-start, m_objectFormat);
                stack.last().expressionExpected = false;
            } else {
                stack.last().expressionExpected = false;
            }
            continue;
        }
        if (c.isDigit() || (c == QLatin1Char('.') && i+1 < length && text.at(i+1).isDigit())) {
            const auto number = numberPattern().match(text, i, QRegularExpression::NormalMatch,
                                                       QRegularExpression::AnchorAtOffsetMatchOption);
            if (number.hasMatch() && number.capturedLength() > 0) {
                setFormat(i, number.capturedLength(), m_numberFormat);
                i += number.capturedLength();
                stack.last().expressionExpected = false;
                continue;
            }
        }
        if (mode == Mode::Interpolation && c == QLatin1Char('{'))
            ++stack.last().braceDepth;
        if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}'))
            stack.last().expressionExpected = false;
        else if (c == QLatin1Char('.') || c == QLatin1Char('`'))
            stack.last().expressionExpected = false;
        else
            stack.last().expressionExpected = true;
        ++i;
    }

    auto* data = new LexerData;
    data->stack = stack;
    setCurrentBlockUserData(data);
    setCurrentBlockState(stateFingerprint(stack));
}
