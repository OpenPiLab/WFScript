#pragma once
// WFScript v5.6.34 - Presentation only. Never stores live widget data.
#include <QRegularExpression>
#include <QString>

struct PanelAppearance {
    QString themeMode = QStringLiteral("follow"); // follow | dark | light
    QString background; // empty: theme canvas; otherwise #RRGGBB
    bool showDesignGrid = true;

    static bool validColor(const QString& s) {
        static const QRegularExpression hex(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
        return s.isEmpty() || hex.match(s).hasMatch();
    }
    bool valid() const {
        return (themeMode==QLatin1String("follow") ||
                themeMode==QLatin1String("dark") ||
                themeMode==QLatin1String("light")) && validColor(background);
    }
};
