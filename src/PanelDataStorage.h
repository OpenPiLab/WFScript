#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

// Shared local storage for trusted WFScript Panel applications. Never derive
// storage identity from the .wfpanel filename, location, or runtime session.
// Use QStandardPaths rather than hard-coded Windows usernames/drive letters.
namespace PanelDataStorage {
inline QString directory() {
    const QString appData=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) return {};
    return QDir::cleanPath(QDir(appData).filePath(QStringLiteral("PanelData")));
}
inline bool ensureExists() {
    const QString path=directory();
    return !path.isEmpty() && QDir().mkpath(path);
}
} // namespace PanelDataStorage
