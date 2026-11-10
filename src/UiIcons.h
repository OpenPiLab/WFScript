#pragma once
// Built-in, DPI-friendly transparent toolbar icons. All files are compiled
// into Qt resources; there are no external icon packs or runtime paths.
#include <QIcon>
#include <QPushButton>
#include <QSize>
#include <QString>

namespace UiIcons {
inline QIcon icon(const char* name) {
    return QIcon(QStringLiteral(":/wfscript/icons/") + QString::fromLatin1(name) + QStringLiteral(".png"));
}
inline void decorate(QPushButton* button, const char* iconName,
                     const QString& tip = QString()) {
    if (!button) return;
    button->setIcon(icon(iconName));
    button->setIconSize(QSize(16, 16));
    if (!tip.isEmpty()) button->setToolTip(tip);
}
}
