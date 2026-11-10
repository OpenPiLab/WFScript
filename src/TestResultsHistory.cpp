#include "TestResultsRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace {
constexpr qint64 kMaxReportBytes=16LL*1024*1024;
bool fail(QString* error,const QString& why) {
    if(error)*error=why;
    return false;
}
QString str(const QJsonValue& value) {
    if(value.isNull() || value.isUndefined())return {};
    if(value.isString())return value.toString();
    if(value.isDouble())return QString::number(value.toDouble(),'g',15);
    if(value.isBool())return value.toBool()?QStringLiteral("true"):QStringLiteral("false");
    return QString::fromUtf8(QJsonDocument::fromVariant(value.toVariant()).toJson(QJsonDocument::Compact));
}
bool validRecord(const QJsonObject& obj) {
    static const QRegularExpression uuid(QStringLiteral("^[A-Fa-f0-9-]{32,40}$"));
    const QString state=obj.value(QStringLiteral("state")).toString();
    return obj.value(QStringLiteral("schemaVersion")).toInt()==1 &&
        uuid.match(obj.value(QStringLiteral("executionId")).toString()).hasMatch() &&
        QStringList{QStringLiteral("passed"),QStringLiteral("failed"),
                    QStringLiteral("error"),QStringLiteral("aborted")}.contains(state) &&
        obj.value(QStringLiteral("runInfo")).isObject() &&
        obj.value(QStringLiteral("steps")).isArray() &&
        obj.value(QStringLiteral("measurements")).isArray();
}
QString csvCell(QString value) {
    // Excel/LibreOffice formula protection for all untrusted labels and UUT fields.
    const QString trimmed=value.trimmed();
    // Keep genuine signed numeric measurements numeric in spreadsheets, but
    // neutralize all text starting with formula operators (also after whitespace).
    bool numeric=false;
    if(trimmed==value && !trimmed.startsWith(QLatin1Char('+')) && !trimmed.isEmpty()) {
        bool ok=false;
        trimmed.toDouble(&ok);
        numeric=ok && trimmed.at(0)!=QLatin1Char('=') && trimmed.at(0)!=QLatin1Char('@');
    }
    if(!numeric && !trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.at(0)))
        value.prepend(QLatin1Char('\''));
    value.replace(QLatin1Char('"'),QStringLiteral("\"\""));
    return QStringLiteral("\"")+value+QStringLiteral("\"");
}
QStringList csvLine(const QStringList& cells) {
    QStringList escaped;escaped.reserve(cells.size());
    for(const QString& cell:cells)escaped.push_back(csvCell(cell));
    return escaped;
}
}

bool TestResultsHistory::read(const QString& filePath,QJsonObject* record,QString* error) {
    if(record)*record={};
    if(!record)return fail(error,QStringLiteral("Missing report destination"));
    const QFileInfo info(filePath);
    if(!info.isFile() || !info.isReadable() || info.suffix().compare(QStringLiteral("json"),Qt::CaseInsensitive)!=0)
        return fail(error,QStringLiteral("Not a readable JSON report: %1").arg(filePath));
    if(info.size()<=0 || info.size()>kMaxReportBytes)
        return fail(error,QStringLiteral("Report size exceeds the 16 MB history viewer limit: %1").arg(filePath));
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))return fail(error,QStringLiteral("Cannot open report: %1").arg(file.errorString()));
    const QByteArray bytes=file.read(kMaxReportBytes+1);
    if(bytes.size()>kMaxReportBytes)return fail(error,QStringLiteral("Report exceeds 16 MB limit"));
    QJsonParseError parse;
    const QJsonDocument doc=QJsonDocument::fromJson(bytes,&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject())
        return fail(error,QStringLiteral("Invalid JSON report: %1").arg(parse.errorString()));
    const QJsonObject result=doc.object();
    if(!validRecord(result))return fail(error,QStringLiteral("Not a supported completed WFScript execution record"));
    *record=result;
    if(error)error->clear();
    return true;
}

QList<ArchivedTestSummary> TestResultsHistory::scan(const QString& directory,int maximum,QString* error) {
    QList<ArchivedTestSummary> entries;
    if(error)error->clear();
    if(directory.isEmpty()) {fail(error,QStringLiteral("Select a results directory"));return entries;}
    const QDir folder(directory);
    if(!folder.exists()) {
        // A new project may not have completed a test yet. Empty is not corruption.
        return entries;
    }
    if(!QFileInfo(directory).isDir() || !QFileInfo(directory).isReadable()) {
        fail(error,QStringLiteral("Cannot read results directory: %1").arg(directory));return entries;
    }
    if(maximum<=0)return entries;
    const QFileInfoList files=folder.entryInfoList({QStringLiteral("*.json")},QDir::Files|QDir::Readable,
                                                    QDir::Time|QDir::Reversed);
    // Do not trust filenames. Only valid, terminal WFScript records count.
    // Bound the number of filesystem candidates as well, so a folder full of
    // unrelated/corrupt JSON cannot make the Results Browser hang indefinitely.
    const int inspectionBudget=std::max(maximum,5000);
    int inspected=0;
    for(auto it=files.crbegin();it!=files.crend() && inspected<inspectionBudget;++it,++inspected) {
        QJsonObject record;
        if(!read(it->absoluteFilePath(),&record))continue;
        const QJsonObject info=record.value(QStringLiteral("runInfo")).toObject();
        QString uut=str(info.value(QStringLiteral("uutId")));
        for(const QString& fallback:{QStringLiteral("serialNumber"),QStringLiteral("serial"),
                                      QStringLiteral("uutSerial")}) {
            if(!uut.isEmpty())break;
            uut=str(info.value(fallback));
        }
        ArchivedTestSummary item;
        item.path=it->absoluteFilePath();
        item.executionId=record.value(QStringLiteral("executionId")).toString();
        item.finishedAtUtc=record.value(QStringLiteral("finishedAtUtc")).toString();
        item.startedAtUtc=record.value(QStringLiteral("startedAtUtc")).toString();
        item.uutId=uut;
        item.testName=record.value(QStringLiteral("title")).toString();
        item.projectName=record.value(QStringLiteral("projectName")).toString();
        item.state=record.value(QStringLiteral("state")).toString();
        item.elapsedMs=record.value(QStringLiteral("elapsedMs")).toVariant().toLongLong();
        item.instrumentMode=record.value(QStringLiteral("instrument")).toObject()
                            .value(QStringLiteral("mode")).toString();
        item.searchText=item.uutId+QLatin1Char(' ')+item.executionId+QLatin1Char(' ')+
            item.testName+QLatin1Char(' ')+item.projectName+QLatin1Char(' ')+
            item.state+QLatin1Char(' ')+item.instrumentMode+QLatin1Char(' ')+
            QString::fromUtf8(QJsonDocument(info).toJson(QJsonDocument::Compact));
        entries.push_back(item);
    }
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){
        if(a.finishedAtUtc==b.finishedAtUtc)return a.executionId>b.executionId;
        return a.finishedAtUtc>b.finishedAtUtc;
    });
    const bool truncated=entries.size()>maximum || inspected<files.size();
    if(entries.size()>maximum)entries=entries.mid(0,maximum);
    if(truncated && error)
        *error=QStringLiteral("Showing up to %1 most recent valid WFScript reports. "
                              "Additional JSON files may contain older records; narrow the folder to inspect them.").arg(maximum);
    return entries;
}

bool TestResultsHistory::exportRecord(const QJsonObject& record,const QString& destination,
                                      const QString& format,QString* error) {
    if(!validRecord(record))return fail(error,QStringLiteral("Invalid historical report"));
    const QString kind=format.trimmed().toLower();
    if(kind!=QStringLiteral("json") && kind!=QStringLiteral("csv"))
        return fail(error,QStringLiteral("Export format must be JSON or CSV"));
    if(destination.isEmpty() || destination.contains(QChar::Null) ||
       QFileInfo(destination).suffix().compare(kind,Qt::CaseInsensitive)!=0)
        return fail(error,QStringLiteral("Select a destination ending in .%1").arg(kind));
    QByteArray bytes;
    if(kind==QStringLiteral("json")) {
        bytes=QJsonDocument(record).toJson(QJsonDocument::Indented);
    } else {
        QStringList lines;
        lines<<QStringLiteral("recordType,executionId,uutId,stepId,group,type,name,status,elapsedMs,startedAtUtc,finishedAtUtc,value,low,high,unit");
        const QString id=record.value(QStringLiteral("executionId")).toString();
        const QString uut=str(record.value(QStringLiteral("runInfo")).toObject().value(QStringLiteral("uutId")));
        const auto append=[&](const QString& recordType,const QString& stepId,const QString& group,
                              const QString& type,const QString& name,const QString& status,
                              const QString& elapsed,const QString& started,const QString& finished,
                              const QString& value,const QString& low,const QString& high,const QString& unit) {
            lines<<csvLine({recordType,id,uut,stepId,group,type,name,status,elapsed,
                            started,finished,value,low,high,unit}).join(QLatin1Char(','));
        };
        append(QStringLiteral("RUN"),{},{},{},record.value(QStringLiteral("title")).toString(),
               record.value(QStringLiteral("state")).toString(),str(record.value(QStringLiteral("elapsedMs"))),
               record.value(QStringLiteral("startedAtUtc")).toString(),
               record.value(QStringLiteral("finishedAtUtc")).toString(),{},{},{},{});
        const QJsonObject runInfo=record.value(QStringLiteral("runInfo")).toObject();
        for(auto it=runInfo.constBegin();it!=runInfo.constEnd();++it)
            append(QStringLiteral("RUN_INFO"),{},{},{},it.key(),{},{},{},{},str(it.value()),{},{},{});
        for(const QJsonValue value:record.value(QStringLiteral("steps")).toArray()) {
            const QJsonObject m=value.toObject();
            append(QStringLiteral("STEP"),str(m.value(QStringLiteral("id"))),
                   str(m.value(QStringLiteral("group"))),str(m.value(QStringLiteral("type"))),
                   str(m.value(QStringLiteral("name"))),str(m.value(QStringLiteral("status"))),
                   str(m.value(QStringLiteral("elapsedMs"))),
                   str(m.value(QStringLiteral("startedAtUtc"))),
                   str(m.value(QStringLiteral("finishedAtUtc"))),{},{},{},{});
        }
        for(const QJsonValue value:record.value(QStringLiteral("measurements")).toArray()) {
            const QJsonObject m=value.toObject();
            append(QStringLiteral("MEASUREMENT"),str(m.value(QStringLiteral("stepId"))),{},
                   QStringLiteral("test"),str(m.value(QStringLiteral("name"))),
                   str(m.value(QStringLiteral("status"))),{},{},{},str(m.value(QStringLiteral("value"))),
                   str(m.value(QStringLiteral("low"))),str(m.value(QStringLiteral("high"))),
                   str(m.value(QStringLiteral("unit"))));
        }
        bytes=lines.join(QLatin1Char('\n')).toUtf8()+"\n";
    }
    const QFileInfo target(destination);
    if(!target.dir().exists())return fail(error,QStringLiteral("Export folder does not exist"));
    QSaveFile file(destination);
    if(!file.open(QIODevice::WriteOnly))return fail(error,QStringLiteral("Cannot export: %1").arg(file.errorString()));
    if(file.write(bytes)!=bytes.size()){
        const QString why=file.errorString();file.cancelWriting();
        return fail(error,QStringLiteral("Cannot write export: %1").arg(why));
    }
    if(!file.commit())return fail(error,QStringLiteral("Cannot finish export: %1").arg(file.errorString()));
    if(error)error->clear();
    return true;
}
