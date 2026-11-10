#pragma once
// v5.6.70.2: One predictable conversion at the native/JavaScript boundary.
// Q_INVOKABLE QVariant results can otherwise reach QJSEngine as Qt's
// QVariant wrapper, which has numeric indexing but fails Array.isArray().
// Convert JSON-like native values into actual engine-owned JS arrays/objects,
// recursively; never move QJSValue instances across the GUI/worker thread.
#include <QJSEngine>
#include <QJSValue>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace ScriptValueConversion {
inline QJSValue toJavaScript(QJSEngine* engine,const QVariant& value,int depth=0) {
    if (!engine || depth>32 || !value.isValid())return QJSValue(QJSValue::UndefinedValue);
    const int type=value.metaType().id();
    // A default-constructed QString is a valid empty widget value, not JS null.
    if(type==QMetaType::QString)return QJSValue(value.toString());
    if(value.isNull() && type!=QMetaType::QVariantList && type!=QMetaType::QStringList &&
       type!=QMetaType::QVariantMap && type!=QMetaType::QVariantHash &&
       type!=QMetaType::QJsonArray && type!=QMetaType::QJsonObject)
        return QJSValue(QJSValue::NullValue);
    if(type==QMetaType::QVariantList) {
        const QVariantList list=value.toList();
        QJSValue result=engine->newArray(quint32(list.size()));
        for(int i=0;i<list.size();++i)
            result.setProperty(quint32(i),toJavaScript(engine,list.at(i),depth+1));
        return result;
    }
    if(type==QMetaType::QStringList) {
        const QStringList list=value.toStringList();
        QJSValue result=engine->newArray(quint32(list.size()));
        for(int i=0;i<list.size();++i)result.setProperty(quint32(i),QJSValue(list.at(i)));
        return result;
    }
    if(type==QMetaType::QVariantMap) {
        const QVariantMap map=value.toMap();
        QJSValue result=engine->newObject();
        for(auto it=map.cbegin();it!=map.cend();++it)
            result.setProperty(it.key(),toJavaScript(engine,it.value(),depth+1));
        return result;
    }
    if(type==QMetaType::QVariantHash) {
        const QVariantHash map=value.toHash();
        QJSValue result=engine->newObject();
        for(auto it=map.cbegin();it!=map.cend();++it)
            result.setProperty(it.key(),toJavaScript(engine,it.value(),depth+1));
        return result;
    }
    if(type==QMetaType::QJsonArray)
        return toJavaScript(engine,value.value<QJsonArray>().toVariantList(),depth+1);
    if(type==QMetaType::QJsonObject)
        return toJavaScript(engine,value.value<QJsonObject>().toVariantMap(),depth+1);
    if(type==QMetaType::QJsonValue)
        return toJavaScript(engine,value.value<QJsonValue>().toVariant(),depth+1);
    if(type==QMetaType::Bool)return QJSValue(value.toBool());
    switch(type) {
    case QMetaType::Char: case QMetaType::SChar: case QMetaType::UChar:
    case QMetaType::Short: case QMetaType::UShort:
    case QMetaType::Int: case QMetaType::UInt:
    case QMetaType::LongLong: case QMetaType::ULongLong:
    case QMetaType::Float: case QMetaType::Double:
        return QJSValue(value.toDouble());
    default:
        // Other types retain the existing Qt conversion behavior. All standard
        // Panel payloads are JSON-like types covered above.
        return engine->toScriptValue(value);
    }
}
} // namespace ScriptValueConversion
