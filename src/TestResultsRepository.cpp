#include "TestResultsRepository.h"
#include "TestReportStorage.h"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

namespace {
QString slug(QString value,int limit=48) {
    value=value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),QStringLiteral("_"));
    return value.left(limit);
}
bool fail(QString* error,const QString& message) {
    if(error)*error=message;
    return false;
}
}
bool TestResultsRepository::archive(const QVariantMap& execution,
                                    const QJsonObject& settings,
                                    QString* savedPath,QString* error) {
    if(savedPath)savedPath->clear();
    if(!TestReportStorage::validate(settings,error))return false;
    const QString id=execution.value(QStringLiteral("executionId")).toString();
    const QString state=execution.value(QStringLiteral("state")).toString();
    static const QRegularExpression validId(QStringLiteral("^[A-Fa-f0-9-]{32,40}$"));
    if(!validId.match(id).hasMatch() ||
       !QStringList{QStringLiteral("passed"),QStringLiteral("failed"),
                    QStringLiteral("error"),QStringLiteral("aborted")}.contains(state))
        return fail(error,QStringLiteral("Only terminal runs with a valid execution ID can be archived"));
    const QString destination=TestReportStorage::directory(settings);
    if(destination.isEmpty())return fail(error,QStringLiteral("Documents folder is unavailable; select an explicit report folder"));
    if(!QDir().mkpath(destination))
        return fail(error,QStringLiteral("Cannot create report folder: %1").arg(destination));
    const QString uut=slug(execution.value(QStringLiteral("runInfo")).toMap()
                            .value(QStringLiteral("uutId")).toString());
    const QString timestamp=QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'"));
    const QString name=QStringLiteral("%1_%2_%3_%4.json")
        .arg(timestamp,uut.isEmpty()?QStringLiteral("UUT"):uut,state,id);
    const QString target=QDir(destination).filePath(name);
    if(QFileInfo::exists(target))return fail(error,QStringLiteral("Archive already exists; refusing to replace: %1").arg(target));

    const QByteArray json=QJsonDocument::fromVariant(execution).toJson(QJsonDocument::Indented);
    if(json.isEmpty())return fail(error,QStringLiteral("Unable to serialize execution record"));
    QSaveFile file(target);
    if(!file.open(QIODevice::WriteOnly))
        return fail(error,QStringLiteral("Cannot open archive: %1").arg(file.errorString()));
    if(file.write(json)!=json.size()) {
        const QString detail=file.errorString();
        file.cancelWriting();
        return fail(error,QStringLiteral("Cannot write archive: %1").arg(detail));
    }
    if(!file.commit())return fail(error,QStringLiteral("Cannot commit archive: %1").arg(file.errorString()));
    if(savedPath)*savedPath=QFileInfo(target).absoluteFilePath();
    return true;
}
