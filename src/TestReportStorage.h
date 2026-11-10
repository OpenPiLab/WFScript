#pragma once

#include <QDir>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QSet>

// One project-owned policy. The repository owns file I/O; the TestRunner
// owns execution records. Never put per-run values or archive state here.
namespace TestReportStorage {
inline bool validate(const QJsonObject& config,QString* error=nullptr) {
    const QSet<QString> keys={QStringLiteral("autoSave"),QStringLiteral("directory")};
    for(auto it=config.constBegin();it!=config.constEnd();++it) {
        if(!keys.contains(it.key())) {
            if(error)*error=QStringLiteral("Unsupported reportStorage field: %1").arg(it.key());
            return false;
        }
    }
    if(config.contains(QStringLiteral("autoSave")) && !config.value(QStringLiteral("autoSave")).isBool()) {
        if(error)*error=QStringLiteral("reportStorage.autoSave must be true or false");
        return false;
    }
    if(config.contains(QStringLiteral("directory"))) {
        const auto value=config.value(QStringLiteral("directory"));
        if(!value.isString() || value.toString().size()>1000 ||
           value.toString().contains(QChar::Null) ||
           (!value.toString().trimmed().isEmpty() && !QDir::isAbsolutePath(value.toString()))) {
            if(error)*error=QStringLiteral("reportStorage.directory must be empty or an absolute folder path (max 1000 characters)");
            return false;
        }
    }
    return true;
}
inline QString directory(const QJsonObject& config) {
    const QString configured=config.value(QStringLiteral("directory")).toString().trimmed();
    if(!configured.isEmpty())return QDir::cleanPath(configured);
    const QString documents=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if(documents.isEmpty())return {};
    return QDir(documents).filePath(QStringLiteral("WFScript Reports"));
}
inline bool enabled(const QJsonObject& config) {
    return config.value(QStringLiteral("autoSave")).toBool(false);
}
}
