#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>

// Filesystem-only storage. This class does not know about test steps,
// JavaScript engines, GUI widgets, devices or how a run is executed.
class TestResultsRepository final {
public:
    // Atomic UTF-8 JSON archive. Unique execution-ID filenames, no deliberate
    // replacement of earlier reports. Returns absolute path on success.
    static bool archive(const QVariantMap& execution,
                        const QJsonObject& settings,
                        QString* savedPath,QString* error);
};

// Lightweight archive index row. Reading history never executes a script,
// accesses an instrument, or changes a live TestRunner session.
struct ArchivedTestSummary {
    QString path;
    QString executionId;
    QString finishedAtUtc;
    QString startedAtUtc;
    QString uutId;
    QString testName;
    QString projectName;
    QString state;
    QString instrumentMode;
    QString searchText; // Includes user-defined Run Information fields.
    qint64 elapsedMs=0;
};

class TestResultsHistory final {
public:
    // Only records conforming to the v1 terminal TestRunner report schema are indexed.
    // Invalid JSON and unrelated .json files are skipped, never executed.
    static QList<ArchivedTestSummary> scan(const QString& directory,
                                            int maximum=5000,QString* error=nullptr);
    static bool read(const QString& filePath,QJsonObject* record,QString* error=nullptr);
    static bool exportRecord(const QJsonObject& record,const QString& destination,
                             const QString& format,QString* error=nullptr);
};
