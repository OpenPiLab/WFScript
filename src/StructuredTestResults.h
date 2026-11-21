#pragma once

// RC1.5.10: data-only named measurement rules. This code is shared by the
// native Test Executive and the visual definition editor, so authoring and
// execution reject the same malformed acceptance rules.
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QSet>
#include <QMetaType>
#include <cmath>

namespace StructuredTestResults {
struct Rule {
    QString path,name,unit,resultWidget;
    double low=0,high=0;
};
struct Reading {
    Rule rule;
    QString resolvedPath;
    double value=0;
};

inline bool fail(QString* error,const QString& why) {
    if(error)*error=why;
    return false;
}
inline bool finiteNumber(const QVariant& v,double* out=nullptr) {
    if(v.isNull() || !v.isValid())return false;
    const auto id=v.metaType().id();
    if(id!=QMetaType::Double && id!=QMetaType::Float && id!=QMetaType::Int &&
       id!=QMetaType::UInt && id!=QMetaType::LongLong && id!=QMetaType::ULongLong &&
       id!=QMetaType::Short && id!=QMetaType::UShort)return false;
    bool ok=false;
    const double n=v.toDouble(&ok);
    if(!ok || !std::isfinite(n))return false;
    if(out)*out=n;
    return true;
}
inline bool validPath(const QString& path) {
    // Simple properties and array indices, never expressions or function calls.
    // A wildcard is permitted only at the very end, e.g. channels[*].
    static const QRegularExpression re(QStringLiteral(
        R"(^(?:[A-Za-z_$][A-Za-z0-9_$]*|\[(?:0|[1-9][0-9]{0,2})\])(?:\.[A-Za-z_$][A-Za-z0-9_$]*|\[(?:0|[1-9][0-9]{0,2})\])*(?:\[\*\])?$)"));
    return path==QStringLiteral("[*]") ||
           (!path.isEmpty() && path.size()<=128 && re.match(path).hasMatch());
}
inline bool parse(const QVariantMap& entries,QVector<Rule>* out,QString* error=nullptr) {
    if(!out)return fail(error,QStringLiteral("Missing result rule output"));
    out->clear();
    if(entries.isEmpty() || entries.size()>64)
        return fail(error,QStringLiteral("Named measurements require 1..64 result paths"));
    QSet<QString> labels;
    for(auto it=entries.cbegin();it!=entries.cend();++it) {
        if(!validPath(it.key()))
            return fail(error,QStringLiteral("Invalid measurement path: %1").arg(it.key()));
        if(it.value().metaType().id()!=QMetaType::QVariantMap)
            return fail(error,QStringLiteral("Measurement %1 needs {low,high,name,unit} rules").arg(it.key()));
        const auto spec=it.value().toMap();
        for(auto property=spec.cbegin();property!=spec.cend();++property)
            if(!QStringList{QStringLiteral("low"),QStringLiteral("high"),QStringLiteral("name"),
                            QStringLiteral("unit"),QStringLiteral("resultWidget")}.contains(property.key()))
                return fail(error,QStringLiteral("Measurement %1 has unknown property %2")
                    .arg(it.key(),property.key()));
        Rule rule;
        rule.path=it.key();
        if(!spec.contains(QStringLiteral("low")) || !spec.contains(QStringLiteral("high")) ||
           !finiteNumber(spec.value(QStringLiteral("low")),&rule.low) ||
           !finiteNumber(spec.value(QStringLiteral("high")),&rule.high) || rule.low>rule.high)
            return fail(error,QStringLiteral("Measurement %1 requires finite low <= high").arg(it.key()));
        for(const auto& textKey:{QStringLiteral("name"),QStringLiteral("unit"),QStringLiteral("resultWidget")})
            if(spec.contains(textKey) && spec.value(textKey).metaType().id()!=QMetaType::QString)
                return fail(error,QStringLiteral("Measurement %1.%2 must be text").arg(it.key(),textKey));
        rule.name=spec.value(QStringLiteral("name"),it.key()).toString().trimmed();
        rule.unit=spec.value(QStringLiteral("unit")).toString();
        rule.resultWidget=spec.value(QStringLiteral("resultWidget")).toString();
        if(rule.name.isEmpty() || rule.name.size()>120 || rule.unit.size()>32 || rule.resultWidget.size()>100)
            return fail(error,QStringLiteral("Measurement %1 name/unit/widget is too long or blank").arg(it.key()));
        if(rule.path.endsWith(QStringLiteral("[*]")) && !rule.resultWidget.isEmpty())
            return fail(error,QStringLiteral("Array-wide rule %1 cannot target one result widget").arg(it.key()));
        if(labels.contains(rule.name))
            return fail(error,QStringLiteral("Duplicate measurement label: %1").arg(rule.name));
        labels.insert(rule.name);
        out->append(rule);
    }
    return true;
}
inline bool checkJson(const QVariant& v,int depth,int* count,QString* error) {
    if(++(*count)>512 || depth>10)return fail(error,QStringLiteral("Result nesting/element limit exceeded"));
    if(!v.isValid())return fail(error,QStringLiteral("Result contains undefined or unsupported values"));
    if(v.isNull())return true;
    const int id=v.metaType().id();
    if(id==QMetaType::Bool)return true;
    if(id==QMetaType::QString)return v.toString().size()<=8192 ||
        fail(error,QStringLiteral("Result contains an oversized text field"));
    if(finiteNumber(v))return true;
    if(id==QMetaType::QVariantList){
        for(const QVariant& entry:v.toList())if(!checkJson(entry,depth+1,count,error))return false;
        return true;
    }
    if(id==QMetaType::QVariantMap){
        const auto map=v.toMap();
        for(auto it=map.cbegin();it!=map.cend();++it) {
            if(it.key().size()>128)return fail(error,QStringLiteral("Result key too long"));
            if(!checkJson(it.value(),depth+1,count,error))return false;
        }
        return true;
    }
    return fail(error,QStringLiteral("Result contains non-JSON or non-finite values"));
}
inline bool validateResult(const QVariant& v,QString* error=nullptr) {
    if(v.metaType().id()!=QMetaType::QVariantMap && v.metaType().id()!=QMetaType::QVariantList)
        return fail(error,QStringLiteral("Structured result must be a JSON object or array"));
    int count=0;
    if(!checkJson(v,0,&count,error))return false;
    QJsonDocument doc=QJsonDocument::fromVariant(v);
    if(doc.isNull() || doc.toJson(QJsonDocument::Compact).size()>32768)
        return fail(error,QStringLiteral("Structured result exceeds 32 KiB or is invalid JSON"));
    return true;
}
inline bool property(const QVariant& value,const QString& name,QVariant* out) {
    if(value.metaType().id()!=QMetaType::QVariantMap)return false;
    const auto map=value.toMap();
    if(!map.contains(name))return false;
    *out=map.value(name);
    return true;
}
inline bool lookup(const QVariant& root,const QString& path,QVariant* out,QString* error=nullptr) {
    QVariant at=root;
    int index=0;
    while(index<path.size()) {
        if(path.at(index)==QLatin1Char('.')){++index;continue;}
        if(path.at(index)==QLatin1Char('[')){
            const int end=path.indexOf(QLatin1Char(']'),index);
            if(end<0)return fail(error,QStringLiteral("Malformed index in %1").arg(path));
            if(at.metaType().id()!=QMetaType::QVariantList)
                return fail(error,QStringLiteral("%1 requires an array").arg(path));
            const auto list=at.toList();
            bool ok=false;
            const int offset=path.mid(index+1,end-index-1).toInt(&ok);
            if(!ok || offset<0 || offset>=list.size())
                return fail(error,QStringLiteral("Missing array element %1").arg(path));
            at=list.at(offset);index=end+1;
        } else {
            int end=index;
            while(end<path.size() && path.at(end)!=QLatin1Char('[') && path.at(end)!=QLatin1Char('.'))++end;
            if(!property(at,path.mid(index,end-index),&at))
                return fail(error,QStringLiteral("Missing result property %1").arg(path));
            index=end;
        }
    }
    *out=at;return true;
}
inline bool evaluate(const QVariant& root,const QVector<Rule>& rules,QVector<Reading>* out,QString* error=nullptr) {
    if(!out)return fail(error,QStringLiteral("Missing measurement output"));
    out->clear();
    for(const Rule& rule:rules) {
        if(rule.path.endsWith(QStringLiteral("[*]"))) {
            const QString base=rule.path.left(rule.path.size()-3);
            QVariant array;
            if(base.isEmpty())array=root;
            else if(!lookup(root,base,&array,error))return false;
            if(array.metaType().id()!=QMetaType::QVariantList)
                return fail(error,QStringLiteral("%1 must be an array").arg(rule.path));
            const auto list=array.toList();
            if(list.isEmpty() || list.size()>32)
                return fail(error,QStringLiteral("%1 requires 1..32 array elements").arg(rule.path));
            for(int i=0;i<list.size();++i) {
                double n=0;
                if(!finiteNumber(list.at(i),&n))
                    return fail(error,QStringLiteral("%1[%2] is not a finite number").arg(base).arg(i));
                Rule expanded=rule;
                expanded.name+=QStringLiteral(" [%1]").arg(i);
                Reading reading{expanded,base+QStringLiteral("[%1]").arg(i),n};
                out->append(reading);
            }
        } else {
            QVariant value;
            if(!lookup(root,rule.path,&value,error))return false;
            double n=0;
            if(!finiteNumber(value,&n))
                return fail(error,QStringLiteral("%1 is not a finite number").arg(rule.path));
            out->append(Reading{rule,rule.path,n});
        }
        if(out->size()>100)return fail(error,QStringLiteral("One step exceeds 100 measurement results"));
    }
    return true;
}
}
