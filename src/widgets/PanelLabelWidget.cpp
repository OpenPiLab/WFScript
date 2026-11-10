#include "PanelLabelWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QColor>
#include <QtGlobal>

PanelLabelWidget::PanelLabelWidget(const QString& text, QWidget* parent)
    : QLabel(text, parent) {
    setAlignment(Qt::AlignCenter);
    // QFrame::StyledPanel uses platform/style-dependent bevels, which can
    // produce inconsistent edge rendering. The outline is painted below.
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
}

void PanelLabelWidget::setTextPresentation(const QString& format, bool wrap) {
    // Automatic is the legacy QLabel default, preserving old .wfpanel results.
    // Explicit Plain Text is important for displaying literal angle brackets.
    const Qt::TextFormat qtFormat = format == QStringLiteral("Plain Text") ? Qt::PlainText
        : format == QStringLiteral("Rich Text") ? Qt::RichText : Qt::AutoText;
    if(textFormat()!=qtFormat)setTextFormat(qtFormat);
    if(wordWrap()!=wrap)setWordWrap(wrap);
    // QLabel should render formatted text, never open arbitrary links from a Panel.
    setOpenExternalLinks(false);
}

void PanelLabelWidget::setBorderAppearance(bool visible, int cornerRadius) {
    cornerRadius = qBound(0, cornerRadius, 48);
    if(m_borderVisible == visible && m_cornerRadius == cornerRadius)
        return;
    m_borderVisible = visible;
    m_cornerRadius = cornerRadius;
    update();
}

void PanelLabelWidget::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);
    if(!m_borderVisible || width() < 2 || height() < 2)
        return;

    const bool light = parentWidget() && parentWidget()->property("wfPanelLight").toBool();
    const QColor border = light ? QColor("#a4b9cb") : QColor("#56718a");
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, m_cornerRadius > 0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(border, 1.0));
    // A half-pixel inset centers the stroke entirely inside the widget.
    const QRectF outline = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = qMin(qreal(m_cornerRadius), qMin(outline.width(), outline.height()) / 2.0);
    painter.drawRoundedRect(outline, radius, radius);
}
