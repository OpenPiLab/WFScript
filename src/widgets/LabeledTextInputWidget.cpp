#include "LabeledTextInputWidget.h"
#include "InstrumentVisuals.h"
#include "TextInputValidation.h"
#include <QEvent>
#include <QFontMetrics>
#include <QLineEdit>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStringList>
#include <QtGlobal>

LabeledTextInputWidget::LabeledTextInputWidget(bool ipv4, QWidget* parent)
    : QWidget(parent), m_ipv4(ipv4) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    m_editor = new QLineEdit(this);
    m_editor->setObjectName(QStringLiteral("labeledTextEditor"));
    m_editor->setClearButtonEnabled(!ipv4);
    m_editor->setAccessibleDescription(ipv4
        ? QStringLiteral("IPv4 address, four decimal octets from 0 to 255")
        : QStringLiteral("Single-line text input"));

    // textEdited is only fired by the human operator; setText / ui.set does
    // not echo into JavaScript or Panel Data.  Never publish partial IPv4 text.
    connect(m_editor, &QLineEdit::textEdited, this,
            &LabeledTextInputWidget::onEdited);
    connect(m_editor, &QLineEdit::editingFinished, this,
            &LabeledTextInputWidget::onFinished);
    updateLayout();
}

bool LabeledTextInputWidget::isValidValue(const QString& text) const {
    if (text.size() > m_editor->maxLength()) return false;
    if (text.isEmpty()) return !m_required;
    return !m_ipv4 || TextInputValidation::ipv4(text);
}

QString LabeledTextInputWidget::text() const { return m_editor->text(); }

void LabeledTextInputWidget::configure(const QString& caption,
                                       const QString& initialText,
                                       const QString& placeholder,
                                       const QString& labelPosition,
                                       const QString& accent,
                                       int maxLength, bool required, bool readOnly) {
    m_caption = caption;
    m_labelPosition = QStringList{QStringLiteral("Top"), QStringLiteral("Bottom"),
                                  QStringLiteral("Left"), QStringLiteral("Right")}
                        .contains(labelPosition) ? labelPosition : QStringLiteral("Top");
    m_accent = accent;
    m_required = required;
    const QSignalBlocker blocker(m_editor);
    m_editor->setMaxLength(qBound(1, maxLength, 4096));
    m_editor->setPlaceholderText(placeholder);
    m_editor->setReadOnly(readOnly);
    m_editor->setAccessibleName(caption);
    // Saved defaults are validated by the codec. If an externally constructed
    // definition is invalid, never leak its malformed IP into a running script.
    const QString safe = isValidValue(initialText) ? initialText : QString();
    m_editor->setText(safe);
    m_lastAccepted = safe;
    m_invalid = false;
    updateLayout();
}

bool LabeledTextInputWidget::setRuntimeText(const QString& text) {
    if (!isValidValue(text)) {
        // Reject invalid programmatic IP updates without replacing the last
        // valid value; scripts can still inspect their original argument.
        return false;
    }
    const QSignalBlocker blocker(m_editor);
    m_editor->setText(text);
    m_lastAccepted = text;
    m_invalid = false;
    updateStyle();
    return true;
}

void LabeledTextInputWidget::onEdited(const QString& text) {
    // Do not paint a partially entered address red on every keystroke. Red
    // means the operator actually tried to commit an invalid value.
    if(m_invalid) { m_invalid = false; updateStyle(); }
    if (isValidValue(text) && text != m_lastAccepted) {
        m_lastAccepted = text;
        emit userValueChanged(text);
    }
}

void LabeledTextInputWidget::onFinished() {
    const QString current = m_editor->text();
    m_invalid = !isValidValue(current);
    updateStyle();
    if (m_invalid) {
        emit userValidationFailed(current);
        return;
    }
    if (current != m_lastAccepted) {
        m_lastAccepted = current;
        emit userValueChanged(current);
    }
    emit userEditingFinished(current);
}

void LabeledTextInputWidget::updateLayout() {
    const int w = width(), h = height();
    const int pad = qBound(4, qRound(qMin(w / 320., h / 88.) * 11), 15);
    const int gap = qBound(3, qRound(h * 0.06), 9);
    const bool side = m_labelPosition == QLatin1String("Left") ||
                      m_labelPosition == QLatin1String("Right");
    const int labelSize = side ? qBound(64, qRound(w * 0.33), qMax(64, w - 82)) :
                                  qBound(15, qRound(h * 0.30), 31);
    QRect edit;
    if (side) {
        const int inputWidth = qMax(20, w - pad * 2 - gap - labelSize);
        edit = m_labelPosition == QLatin1String("Left")
            ? QRect(pad + labelSize + gap, pad, inputWidth, qMax(20, h - 2 * pad))
            : QRect(pad, pad, inputWidth, qMax(20, h - 2 * pad));
    } else {
        const int fieldHeight = qMax(20, h - pad * 2 - gap - labelSize);
        edit = m_labelPosition == QLatin1String("Top")
            ? QRect(pad, pad + labelSize + gap, qMax(20, w - 2 * pad), fieldHeight)
            : QRect(pad, pad, qMax(20, w - 2 * pad), fieldHeight);
    }
    m_editor->setGeometry(edit);
    QFont inputFont = font();
    inputFont.setPixelSize(qBound(11, qRound(edit.height() * 0.38), 25));
    m_editor->setFont(inputFont);
    updateStyle();
    update();
}

void LabeledTextInputWidget::updateStyle() {
    const bool light = InstrumentVisuals::light(this);
    const QColor border = m_invalid ? QColor(QStringLiteral("#de6067"))
                                     : InstrumentVisuals::accent(m_accent);
    const QString base = light ? QStringLiteral("#ffffff") : QStringLiteral("#132a3f");
    const QString ink = light ? QStringLiteral("#18354a") : QStringLiteral("#eef7ff");
    const QString disabled = light ? QStringLiteral("#eaf0f5") : QStringLiteral("#172434");
    m_editor->setStyleSheet(QStringLiteral(
        "QLineEdit {background:%1;color:%2;border:1px solid %3;"
        "border-radius:7px;padding:3px 9px;selection-background-color:%3;}"
        "QLineEdit:read-only {background:%4;}"
    ).arg(base, ink, border.name(), disabled));
}

void LabeledTextInputWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    InstrumentVisuals::background(painter, QRectF(rect()), 11, this);
    const QRect edit = m_editor->geometry();
    const int gap = qBound(3, qRound(height() * 0.06), 9);
    const int pad = qBound(4, qRound(qMin(width() / 320., height() / 88.) * 11), 15);
    QRect textRect;
    int alignment = Qt::AlignVCenter | Qt::AlignLeft;
    if (m_labelPosition == QLatin1String("Top")) {
        textRect = QRect(pad + 2, pad, qMax(10, width() - 2 * pad - 4),
                         qMax(12, edit.top() - pad - gap));
    } else if (m_labelPosition == QLatin1String("Bottom")) {
        textRect = QRect(pad + 2, edit.bottom() + gap, qMax(10, width() - 2 * pad - 4),
                         qMax(12, height() - pad - edit.bottom() - gap));
    } else if (m_labelPosition == QLatin1String("Left")) {
        textRect = QRect(pad + 2, pad, qMax(15, edit.left() - pad - gap - 4),
                         qMax(14, height() - 2 * pad));
    } else {
        textRect = QRect(edit.right() + gap + 3, pad,
                         qMax(15, width() - edit.right() - gap - pad - 3),
                         qMax(14, height() - 2 * pad));
    }
    QFont labelFont = font();
    labelFont.setPixelSize(qBound(10, qRound(qMin(height() * 0.19, width() * 0.046)), 16));
    labelFont.setWeight(QFont::DemiBold);
    painter.setFont(labelFont);
    painter.setPen(InstrumentVisuals::tone(this, QColor(QStringLiteral("#c8dbe8"))));
    const QString caption = m_caption + (m_required ? QStringLiteral(" *") : QString());
    painter.drawText(textRect, alignment,
                     InstrumentVisuals::elide(labelFont, caption, textRect.width()));
}

void LabeledTextInputWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    updateLayout();
}

void LabeledTextInputWidget::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (e->type() == QEvent::ThemeChange || e->type() == QEvent::PaletteChange)
        updateStyle();
}
