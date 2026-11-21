#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <cmath>

// Project-owned definitions, not the separate disk storage folder named PanelData.
// Live values are never written back into the project by simply running it.
namespace PanelDataSchema {
inline bool validName(const QString& name) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*(\\.[A-Za-z_][A-Za-z0-9_]*)*$"));
    return name.size()<=100 && pattern.match(name).hasMatch();
}
inline QString bindingType(const QString& widgetType) {
    if(QStringList{QStringLiteral("numeric"),QStringLiteral("digital_display"),
                   QStringLiteral("gauge"),QStringLiteral("precision_dial"),
                   QStringLiteral("segmented_bar"),QStringLiteral("slider"),
                   QStringLiteral("rotary_input"),QStringLiteral("numeric_stepper"),
                   QStringLiteral("status_banner")}.contains(widgetType)) return QStringLiteral("number");
    if(QStringList{QStringLiteral("toggle"),QStringLiteral("switch_button"),
                   QStringLiteral("status_beacon")}.contains(widgetType))return QStringLiteral("boolean");
    if(QStringList{QStringLiteral("label"),QStringLiteral("option_selector"),
                   QStringLiteral("text_input"),QStringLiteral("ip_address_input")}.contains(widgetType))
        return QStringLiteral("text");
    // Action buttons, charts, large tables, terminals, equations and Test Executive
    // widgets require structured inputs or side effects; do not pretend that a
    // scalar Value binding can update them correctly.
    return {};
}
inline QString inputEvent(const QString& widgetType) {
    if(QStringList{QStringLiteral("slider"),QStringLiteral("rotary_input"),
                   QStringLiteral("numeric_stepper")}.contains(widgetType))return QStringLiteral("valueChanged");
    if(widgetType==QStringLiteral("toggle") || widgetType==QStringLiteral("switch_button"))
        return QStringLiteral("toggled");
    if(widgetType==QStringLiteral("option_selector"))return QStringLiteral("selectionChanged");
    if(widgetType==QStringLiteral("text_input") || widgetType==QStringLiteral("ip_address_input"))
        return QStringLiteral("valueChanged");
    return {}; // read-only visual bindings
}
inline QVariant initialValue(const QJsonObject& variable) {
    const QString type=variable.value(QStringLiteral("type")).toString();
    const QJsonValue value=variable.value(QStringLiteral("initial"));
    if(type==QStringLiteral("number"))return value.toDouble();
    if(type==QStringLiteral("boolean"))return value.toBool();
    return value.toString();
}
inline bool validValue(const QString& type,const QJsonValue& v) {
    if(type==QStringLiteral("number"))return v.isDouble() && std::isfinite(v.toDouble());
    if(type==QStringLiteral("boolean"))return v.isBool();
    if(type==QStringLiteral("text"))return v.isString() && v.toString().size()<=4096;
    return false;
}
inline bool validate(const QJsonArray& defs, QString* error=nullptr) {
    const auto fail=[error](const QString& message){if(error)*error=message;return false;};
    if(defs.size()>128)return fail(QStringLiteral("Panel Data has more than 128 variables"));
    QSet<QString> names;
    for(const QJsonValue& v:defs) {
        if(!v.isObject())return fail(QStringLiteral("Panel Data entry must be an object"));
        const auto d=v.toObject();
        const QString name=d.value(QStringLiteral("name")).toString();
        const QString type=d.value(QStringLiteral("type")).toString();
        if(!validName(name) || names.contains(name))return fail(QStringLiteral("Invalid or duplicate Panel Data name: %1").arg(name));
        if(!validValue(type,d.value(QStringLiteral("initial"))))return fail(QStringLiteral("Invalid initial value or type for %1").arg(name));
        names.insert(name);
    }
    return true;
}
inline QHash<QString,QString> types(const QJsonArray& defs) {
    QHash<QString,QString> result;
    for(const auto& v:defs) {
        const auto obj=v.toObject();
        result.insert(obj.value(QStringLiteral("name")).toString(),obj.value(QStringLiteral("type")).toString());
    }
    return result;
}
inline bool validateBinding(const QString& widgetType,const QJsonObject& bindings,
                            const QHash<QString,QString>& defined,QString* error=nullptr) {
    if(bindings.isEmpty())return true;
    const auto fail=[error](const QString& message){if(error)*error=message;return false;};
    if(bindings.size()!=1 || !bindings.value(QStringLiteral("value")).isString())
        return fail(QStringLiteral("Only a single Value binding is supported"));
    const QString name=bindings.value(QStringLiteral("value")).toString();
    if(!defined.contains(name))return fail(QStringLiteral("Binding refers to missing Panel Data variable %1").arg(name));
    if(bindingType(widgetType)!=defined.value(name))
        return fail(QStringLiteral("Binding type mismatch: %1 requires %2").arg(widgetType,bindingType(widgetType)));
    return true;
}
}
