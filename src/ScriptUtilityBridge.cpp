#include "ScriptUtilityBridge.h"
#include "ScriptValueConversion.h"
#include "BinaryBridge.h"
#include <QJSEngine>
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "PanelDataStorage.h"
#include <cmath>

ScriptUtilityBridge::ScriptUtilityBridge(QJSEngine* engine,const QString& projectPath,QObject* parent)
    :QObject(parent),m_engine(engine) {
    Q_UNUSED(projectPath); // Retained constructor signature for existing runtime callers.
    m_root=PanelDataStorage::directory();
    if (!m_root.isEmpty()) QDir().mkpath(m_root);
}
QString ScriptUtilityBridge::dataPath(const QString& name) const {
    if(m_root.isEmpty() || name.isEmpty() || name==QStringLiteral(".") || name==QStringLiteral("..") ||
       QFileInfo(name).isAbsolute())return {};
    const QString path=QDir::cleanPath(m_root+QLatin1Char('/')+name);
    if(!path.startsWith(m_root+QLatin1Char('/')))return {};
    return path;
}
bool ScriptUtilityBridge::permitted(const QString& path,bool write) {
    m_error.clear();
    if(m_stopped) {m_error=QStringLiteral("Panel utilities stopped");return false;}
    const QString normalized=QFileInfo(path).absoluteFilePath();
    const QString rootPrefix=m_root+QLatin1Char('/');
    const bool own=normalized.startsWith(rootPrefix);
    const bool grant=write?m_grantedWrite.contains(normalized):
                           (m_grantedRead.contains(normalized)||m_grantedWrite.contains(normalized));
    if(!own && !grant){m_error=QStringLiteral("File access denied. Use FileIO.dataPath() or a path selected in a Tool file dialog.");return false;}
    QFileInfo fi(normalized);
    if(fi.isSymLink()){m_error=QStringLiteral("Symbolic-link files are not permitted");return false;}
    if(own) {
        const QString parent=fi.absoluteDir().canonicalPath();
        const QString root=QDir(m_root).canonicalPath();
        if(parent.isEmpty() || root.isEmpty() || (parent!=root && !parent.startsWith(root+QLatin1Char('/')))) {
            m_error=QStringLiteral("File parent directory is outside Panel data storage or does not exist");return false;
        }
    }
    if(write && (!fi.absoluteDir().exists() || fi.isDir())) {
        m_error=QStringLiteral("Destination parent directory must exist");return false;
    }
    return true;
}
bool ScriptUtilityBridge::exists(const QString& path){return permitted(path,false) && QFileInfo::exists(path);}
QString ScriptUtilityBridge::readString(const QString& path){
    if(!permitted(path,false))return {};
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)){m_error=f.errorString();return {};}
    if(f.size()>kMaxFileBytes){m_error=QStringLiteral("File exceeds 2 MiB limit");return {};}
    const QByteArray bytes=f.read(kMaxFileBytes+1);
    if(bytes.size()>kMaxFileBytes){m_error=QStringLiteral("File exceeds 2 MiB limit");return {};}
    return QString::fromUtf8(bytes);
}
QString ScriptUtilityBridge::readText(const QString& path){return readString(path);}
bool ScriptUtilityBridge::writeString(const QString& path,const QByteArray& bytes,bool append){
    if(!permitted(path,true))return false;
    if(bytes.size()>kMaxFileBytes){m_error=QStringLiteral("Write exceeds 2 MiB limit");return false;}
    if(append) {
        QFile f(path);
        if(f.exists() && f.size()+bytes.size()>kMaxFileBytes){m_error=QStringLiteral("File would exceed 2 MiB limit");return false;}
        if(!f.open(QIODevice::WriteOnly|QIODevice::Append)){m_error=f.errorString();return false;}
        if(f.write(bytes)!=bytes.size()){m_error=f.errorString();return false;}
        return true;
    }
    QSaveFile f(path);
    if(!f.open(QIODevice::WriteOnly)){m_error=f.errorString();return false;}
    if(f.write(bytes)!=bytes.size()){m_error=f.errorString();f.cancelWriting();return false;}
    if(!f.commit()){m_error=f.errorString();return false;}
    return true;
}
bool ScriptUtilityBridge::writeText(const QString& path,const QString& value){return writeString(path,value.toUtf8(),false);}
bool ScriptUtilityBridge::appendText(const QString& path,const QString& value){return writeString(path,value.toUtf8(),true);}
QVariant ScriptUtilityBridge::readJson(const QString& path){
    const QString text=readString(path);
    if(!m_error.isEmpty())return {};
    QJsonParseError pe;
    const QJsonDocument doc=QJsonDocument::fromJson(text.toUtf8(),&pe);
    if(pe.error!=QJsonParseError::NoError){m_error=pe.errorString();return {};}
    return doc.toVariant();
}
bool ScriptUtilityBridge::writeJson(const QString& path,const QJSValue& value){
    // QJSEngine does not reliably marshal a JS object to a QVariantMap when
    // Q_INVOKABLE takes QVariant: the value may remain a wrapped QJSValue.
    // Accept the original JS value, then explicitly convert nested objects.
    m_error.clear();
    if(!value.isObject() || value.isCallable()){
        m_error=QStringLiteral("Expected a JSON object or array");
        return false;
    }
    const QVariant converted=value.toVariant(QJSValue::ConvertJSObjects);
    const QJsonDocument doc=QJsonDocument::fromVariant(converted);
    if(doc.isNull()){
        m_error=QStringLiteral("Expected a JSON object or array");
        return false;
    }
    return writeString(path,doc.toJson(QJsonDocument::Indented),false);
}
QObject* ScriptUtilityBridge::readBuffer(const QString& path){
    if(!permitted(path,false))return nullptr;
    if(!m_binary){m_error=QStringLiteral("Binary subsystem unavailable");return nullptr;}
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)){m_error=f.errorString();return nullptr;}
    if(f.size()>kMaxFileBytes){m_error=QStringLiteral("File exceeds 2 MiB limit");return nullptr;}
    const QByteArray bytes=f.read(kMaxFileBytes+1);
    if(bytes.size()>kMaxFileBytes){m_error=QStringLiteral("File exceeds 2 MiB limit");return nullptr;}
    auto* b=m_binary->fromNativeBytes(bytes);
    if(!b)m_error=m_binary->lastError();
    return b;
}
bool ScriptUtilityBridge::writeBuffer(const QString& path,QObject* value){
    auto* b=qobject_cast<BinaryBuffer*>(value);
    if(!b || b->isReleased()){m_error=QStringLiteral("Expected live BinaryBuffer");return false;}
    if(b->length()>kMaxFileBytes){m_error=QStringLiteral("Write exceeds 2 MiB limit");return false;}
    return writeString(path,b->bytesCopy(),false);
}
bool ScriptUtilityBridge::appendBuffer(const QString& path,QObject* value){
    auto* b=qobject_cast<BinaryBuffer*>(value);
    if(!b || b->isReleased()){m_error=QStringLiteral("Expected live BinaryBuffer");return false;}
    if(b->length()>kMaxFileBytes){m_error=QStringLiteral("Append exceeds 2 MiB limit");return false;}
    return writeString(path,b->bytesCopy(),true);
}
QVariantList ScriptUtilityBridge::readBytes(const QString& path){
    if(!permitted(path,false))return {};
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)){m_error=f.errorString();return {};}
    if(f.size()>kMaxFileBytes){m_error=QStringLiteral("File exceeds 2 MiB limit");return {};}
    const QByteArray bytes=f.read(kMaxFileBytes+1);
    if(bytes.size()>kMaxFileBytes){m_error=QStringLiteral("File exceeds 2 MiB limit");return {};}
    QVariantList list;list.reserve(bytes.size());
    for(unsigned char c:bytes)list.append(int(c));
    return list;
}
bool ScriptUtilityBridge::writeBytes(const QString& path,const QVariantList& values){
    if(values.size()>kMaxFileBytes){m_error=QStringLiteral("Write exceeds 2 MiB limit");return false;}
    QByteArray data;data.reserve(values.size());
    for(const QVariant& value:values){
        bool ok=false;const double number=value.toDouble(&ok);
        if(!ok || !std::isfinite(number) || number<0 || number>255 || std::floor(number)!=number){
            m_error=QStringLiteral("Byte values must be integers 0..255");return false;
        }
        data.append(char(static_cast<unsigned char>(number)));
    }
    return writeString(path,data,false);
}
// CSV is RFC4180-style quoted fields. All fields are returned as strings.
static QString csvEscape(const QString& s){
    if(!s.contains(QLatin1Char(',')) && !s.contains(QLatin1Char('"')) &&
       !s.contains(QLatin1Char('\n')) && !s.contains(QLatin1Char('\r')))return s;
    QString t=s;t.replace(QLatin1Char('"'),QStringLiteral("\"\""));return QLatin1Char('"')+t+QLatin1Char('"');
}
QVariantList ScriptUtilityBridge::readCsv(const QString& path){
    const QString s=readString(path);if(!m_error.isEmpty())return {};
    QVariantList rows,row;QString field;bool quoted=false;
    for(int i=0;i<s.size();++i){const QChar ch=s[i];
        if(quoted){if(ch==QLatin1Char('"')){if(i+1<s.size() && s[i+1]==QLatin1Char('"')){field+=ch;++i;}else quoted=false;}else field+=ch;}
        else if(ch==QLatin1Char('"'))quoted=true;
        else if(ch==QLatin1Char(',')){row.append(field);field.clear();}
        else if(ch==QLatin1Char('\n')){row.append(field);field.clear();rows.append(QVariant(row));row.clear();}
        else if(ch==QLatin1Char('\r')){if(i+1<s.size() && s[i+1]==QLatin1Char('\n'))++i;row.append(field);field.clear();rows.append(QVariant(row));row.clear();}
        else field+=ch;
    }
    if(quoted){m_error=QStringLiteral("Unterminated quoted CSV field");return {};}
    if(!row.isEmpty()||!field.isEmpty()){row.append(field);rows.append(QVariant(row));}
    return rows;
}
QJSValue ScriptUtilityBridge::readCsvArray(const QString& path){
    return ScriptValueConversion::toJavaScript(m_engine,QVariant(readCsv(path)));
}
bool ScriptUtilityBridge::writeCsv(const QString& path,const QVariantList& rows){
    QString output;
    for(const QVariant& line:rows){
        const QVariantList row=line.toList();
        if(!line.canConvert<QVariantList>()){m_error=QStringLiteral("CSV input must be an array of arrays");return false;}
        QStringList fields;
        for(const QVariant& v:row)fields.append(csvEscape(v.toString()));
        output+=fields.join(QLatin1Char(','))+QLatin1Char('\n');
        if(output.size()>kMaxFileBytes){m_error=QStringLiteral("CSV exceeds 2 MiB limit");return false;}
    }
    return writeString(path,output.toUtf8(),false);
}
int ScriptUtilityBridge::request(const QString& kind,const QVariantMap& options,const QJSValue& callback){
    m_error.clear();
    static const QSet<QString> allowed={QStringLiteral("question"),QStringLiteral("getText"),QStringLiteral("getNumber"),
                                         QStringLiteral("getOpenFile"),QStringLiteral("getSaveFile"),QStringLiteral("getItem")};
    if(m_stopped || !allowed.contains(kind) || !callback.isCallable() || m_pending.size()>=32){
        m_error=QStringLiteral("Invalid dialog request, callback, or too many outstanding dialogs");return 0;
    }
    const int id=m_nextId++;
    m_pending.insert(id,callback);
    emit dialogRequested(id,kind,options);
    return id;
}
void ScriptUtilityBridge::complete(int id,const QVariantMap& result){
    auto it=m_pending.find(id);
    if(m_stopped || it==m_pending.end() || !m_engine)return;
    QJSValue cb=it.value();m_pending.erase(it);
    if(result.value(QStringLiteral("accepted")).toBool() &&
       (result.value(QStringLiteral("kind"))==QStringLiteral("getOpenFile") ||
        result.value(QStringLiteral("kind"))==QStringLiteral("getSaveFile"))){
        const QString normalized=QFileInfo(result.value(QStringLiteral("value")).toString()).absoluteFilePath();
        m_grantedRead.insert(normalized);
        if(result.value(QStringLiteral("kind"))==QStringLiteral("getSaveFile"))m_grantedWrite.insert(normalized);
    }
    emit callbackStarted();
    QJSValue outcome=cb.call(QJSValueList{m_engine->toScriptValue(result)});
    emit callbackFinished();
    if(outcome.isError())emit callbackFailed(QStringLiteral("Tool dialog callback"),outcome);
}
void ScriptUtilityBridge::shutdown(){m_stopped=true;m_pending.clear();m_grantedRead.clear();m_grantedWrite.clear();}
