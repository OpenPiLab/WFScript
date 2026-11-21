#pragma once

// Responsive typography for the six original WFScript Qt-control widgets.
// Instrument widgets retain InstrumentVisuals' custom, logical-coordinate
// painting; charts/tables/terminals deliberately retain density-oriented fonts.
#include <QFont>
#include <QFontMetricsF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtGlobal>

namespace BasicWidgetVisuals {
inline QFont fit(const QFont& base, const QString& text, const QRectF& content,
                 qreal heightFraction, int minimumPixels = 9,
                 int maximumPixels = 72, bool wrap = false) {
    QFont f(base);
    const int maxPixels = qMax(minimumPixels, maximumPixels);
    const int ideal = qBound(minimumPixels, qRound(content.height() * heightFraction), maxPixels);
    const qreal usableWidth = qMax(1.0, content.width());
    const qreal usableHeight = qMax(1.0, content.height());
    for (int px = ideal; px >= minimumPixels; --px) {
        f.setPixelSize(px);
        const QFontMetricsF metrics(f);
        if (wrap) {
            const QRectF measured = metrics.boundingRect(QRectF(0, 0, usableWidth, 10000),
                                                          Qt::TextWordWrap, text);
            if (measured.height() <= usableHeight && measured.width() <= usableWidth + 1)
                return f;
        } else {
            const QStringList lines = text.split(QLatin1Char('\n'));
            bool fits = true;
            for (const QString& line : lines) {
                if (metrics.horizontalAdvance(line) > usableWidth) {fits = false; break;}
            }
            if (fits && metrics.height() * qMax(1, lines.size()) <= usableHeight)
                return f;
        }
    }
    f.setPixelSize(minimumPixels);
    return f;
}
} // namespace BasicWidgetVisuals
