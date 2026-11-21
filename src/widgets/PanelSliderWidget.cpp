#include "PanelSliderWidget.h"
#include "InstrumentVisuals.h"
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QtGlobal>
#include <cmath>

PanelSliderWidget::PanelSliderWidget(QWidget* parent)
    : QWidget(parent), m_slider(new QSlider(Qt::Horizontal, this)) {
    // Keep the existing 1000-step mapping, so callbacks and saved values have
    // exactly the same meaning as in the original bare QSlider implementation.
    m_slider->setObjectName(QStringLiteral("wfSliderTrack"));
    m_slider->setRange(0, 1000);
    m_slider->setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    positionSlider();
}

QString PanelSliderWidget::formatNumber(double number) {
    // Avoid rounding small engineering ranges to 0.000. A fixed typography
    // policy is retained; resizing does not silently change the font size.
    return QString::number(number, 'g', 8);
}

void PanelSliderWidget::configure(const PanelControl& definition) {
    m_title = definition.text;
    m_minimum = definition.minimum;
    m_maximum = definition.maximum;
    m_showValue = definition.properties.value(QStringLiteral("showValue")).toBool(true);
    m_showRange = definition.properties.value(QStringLiteral("showRange")).toBool(true);
    setRuntimeValue(definition.value);
    positionSlider();
    update();
}

void PanelSliderWidget::setRuntimeValue(double number) {
    if (!std::isfinite(number)) return;
    const double span = m_maximum - m_minimum;
    const int raw = span > 0.0
        ? int(qBound(0.0, (number - m_minimum) / span, 1.0) * 1000.0)
        : 0;
    const QSignalBlocker block(m_slider);
    m_slider->setValue(raw);
    update();
}

double PanelSliderWidget::value() const {
    return m_minimum + (m_maximum - m_minimum) * m_slider->value() / 1000.0;
}

void PanelSliderWidget::positionSlider() {
    if (!m_slider) return;
    const int pad = qBound(5, width() / 28, 12);
    const int top = height() >= 43 ? 23 : height() >= 34 ? 20 : 3;
    const bool rangeFits = m_showRange && height() >= 55;
    const int usable = qMax(8, height() - top - (rangeFits ? 17 : 5));
    const int trackHeight = qMin(28, usable);
    m_slider->setGeometry(pad, top, qMax(12, width() - 2 * pad), trackHeight);
}

void PanelSliderWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    positionSlider();
}

void PanelSliderWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ThemeChange ||
        event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::StyleChange)
        update();
}

void PanelSliderWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    InstrumentVisuals::background(painter, QRectF(rect()), 9, this);

    const int padding = qBound(5, width() / 28, 12);
    const int labelWidth = qMax(1, width() - 2 * padding);
    const QString displayedValue = formatNumber(value());
    QFont titleFont(font());
    titleFont.setPixelSize(11);
    titleFont.setWeight(QFont::Medium);
    QFont valueFont(font());
    valueFont.setPixelSize(13);
    valueFont.setWeight(QFont::DemiBold);

    // At PanelCanvas's absolute minimum control height (22px), show the
    // draggable track rather than attempting two overlapping text rows.
    if (height() < 34) return;
    painter.setPen(InstrumentVisuals::tone(this, QColor(190, 218, 236)));
    int titleWidth = labelWidth;
    if (m_showValue) {
        const int valueWidth = qMin(labelWidth / 2,
            QFontMetrics(valueFont).horizontalAdvance(displayedValue) + 5);
        titleWidth = qMax(1, labelWidth - valueWidth - 6);
        painter.setPen(InstrumentVisuals::tone(this, QColor(235, 246, 255)));
        painter.setFont(valueFont);
        painter.drawText(QRect(padding + titleWidth + 6, 3, valueWidth, 19),
                         Qt::AlignRight | Qt::AlignVCenter, displayedValue);
    }
    painter.setPen(InstrumentVisuals::tone(this, QColor(190, 218, 236)));
    painter.setFont(titleFont);
    const QString caption = m_title.isEmpty() ? QStringLiteral("SLIDER") : m_title;
    painter.drawText(QRect(padding, 3, titleWidth, 18),
        Qt::AlignLeft | Qt::AlignVCenter,
        QFontMetrics(titleFont).elidedText(caption, Qt::ElideRight, titleWidth));

    if (m_showRange && height() >= 55) {
        QFont rangeFont(font());
        rangeFont.setPixelSize(9);
        painter.setFont(rangeFont);
        painter.setPen(InstrumentVisuals::tone(this, QColor(144, 166, 185)));
        const QString lo = formatNumber(m_minimum);
        const QString hi = formatNumber(m_maximum);
        const int y = height() - 16;
        const int half = labelWidth / 2;
        painter.drawText(QRect(padding, y, half, 12),
                         Qt::AlignLeft | Qt::AlignVCenter, lo);
        painter.drawText(QRect(padding + half, y, labelWidth - half, 12),
                         Qt::AlignRight | Qt::AlignVCenter, hi);
    }
}
