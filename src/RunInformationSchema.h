#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QVariantMap>
#include <QStringList>
#include <cmath>

// Project-owned UUT field definitions. Actual operator values are deliberately
// NOT stored in the .wfpanel: they are captured only by individual test runs.
namespace RunInformationSchema {
inline bool validate(const QJsonObject& schema,QString* error=nullptr) {
    auto fail=[&](const QString& why) {if(error)*error=why;return false;};
    if(schema.isEmpty())return true;
    if(!schema.value("fields").isArray())return fail("Run Information requires a fields array");
    const QJsonArray fields=schema.value("fields").toArray();
    if(fields.size()>16)return fail("Run Information supports at most 16 fields");
    static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z][A-Za-z0-9_]{0,39}$"));
    QSet<QString> seen;
    for(const auto& item:fields) {
        if(!item.isObject())return fail("Run Information field must be an object");
        const QJsonObject f=item.toObject();
        const QString id=f.value("id").toString();
        const QString label=f.value("label").toString().trimmed();
        const QString type=f.value("type").toString();
        if(!idPattern.match(id).hasMatch() || seen.contains(id))return fail("Invalid or duplicate Run Information field ID: "+id);
        if(label.isEmpty() || label.size()>80)return fail("Invalid Run Information label for "+id);
        if(!QStringList{"text","number","choice","boolean"}.contains(type))return fail("Unsupported Run Information type for "+id);
        if(f.contains("required") && !f.value("required").isBool())return fail("Invalid required flag for "+id);
        if(type=="choice") {
            if(!f.value("options").isArray())return fail("Choice field "+id+" needs options");
            const auto options=f.value("options").toArray();
            if(options.isEmpty() || options.size()>30)return fail("Choice field "+id+" needs 1..30 choices");
            QSet<QString> choices;
            for(const auto& o:options){
                if(!o.isString() || o.toString().trimmed().isEmpty() || o.toString().size()>80 || choices.contains(o.toString()))
                    return fail("Invalid or duplicate choice for "+id);
                choices.insert(o.toString());
            }
        }
        if(f.contains("default")) {
            const QJsonValue d=f.value("default");
            if(type=="boolean" ? !d.isBool() : type=="number" ? !d.isDouble() : !d.isString())
                return fail("Default value has wrong type for "+id);
            if(type=="choice" && !d.toString().isEmpty()) {
                bool found=false;
                for(const auto& option:f.value("options").toArray())if(option.toString()==d.toString())found=true;
                if(!found)return fail("Default is not a valid choice for "+id);
            }
            if(d.isString() && d.toString().size()>200)return fail("Default too long for "+id);
            if(d.isDouble() && !std::isfinite(d.toDouble()))return fail("Non-finite default for "+id);
        }
        seen.insert(id);
    }
    return true;
}
inline QVariantMap defaults(const QJsonObject& schema) {
    QVariantMap out;
    for(const auto& v:schema.value("fields").toArray()) {
        const auto f=v.toObject();
        const QString id=f.value("id").toString();
        const QString type=f.value("type").toString();
        if(f.contains("default"))out.insert(id,f.value("default").toVariant());
        else if(type=="boolean")out.insert(id,false);
        else if(type=="number")out.insert(id,QString()); // no implicit 0 for an untouched numeric input
        else out.insert(id,QString());
    }
    return out;
}
inline bool validateValues(const QJsonObject& schema,const QVariantMap& values,QVariantMap* normalized,QString* error=nullptr) {
    auto fail=[&](const QString& why) {if(error)*error=why;return false;};
    QVariantMap output;
    for(const auto& v:schema.value("fields").toArray()) {
        const auto f=v.toObject();
        const QString id=f.value("id").toString();
        const QString label=f.value("label").toString();
        const QString type=f.value("type").toString();
        const bool required=f.value("required").toBool(false);
        const QVariant raw=values.value(id);
        if(type=="boolean") {
            const bool checked=raw.toBool();
            if(required && !checked)return fail(label+" must be checked");
            output.insert(id,checked);continue;
        }
        const QString str=raw.toString().trimmed();
        if(str.isEmpty()) {
            if(required)return fail(label+" is required");
            output.insert(id,QString());continue;
        }
        if(type=="number") {
            bool good=false;const double num=str.toDouble(&good);
            if(!good || !std::isfinite(num))return fail(label+" must be a finite number");
            output.insert(id,num);continue;
        }
        if(str.size()>200)return fail(label+" must be 200 characters or fewer");
        if(type=="choice") {
            bool found=false;
            for(const auto& option:f.value("options").toArray())if(option.toString()==str)found=true;
            if(!found)return fail(label+" must match a configured choice");
        }
        output.insert(id,str);
    }
    if(normalized)*normalized=output;
    return true;
}
}
