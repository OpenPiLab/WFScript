#pragma once
// RC1.5.14: declarative, deterministic conditions and run-scoped variable declarations.
// No JavaScript evaluation, implicit I/O, or dynamic jumps.
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariantMap>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace TestSequenceConditions {
inline bool reject(QString* error,const QString& message) {if(error)*error=message;return false;}
inline bool identifier(const QString& key) {
    return QRegularExpression(QStringLiteral("^[A-Za-z][A-Za-z0-9_]{0,39}$")).match(key).hasMatch();
}
inline bool scalar(const QJsonValue& value) {
    return value.isString() || value.isBool() || (value.isDouble() && std::isfinite(value.toDouble()));
}
inline bool validateVariables(const QJsonArray& list,QString* error=nullptr) {
    if(list.size()>64)return reject(error,QStringLiteral("At most 64 declared test variables"));
    QSet<QString> names;
    for(const auto& v:list) {
        if(!v.isObject())return reject(error,QStringLiteral("Variable declaration must be an object"));
        const QJsonObject d=v.toObject();
        for(auto it=d.begin();it!=d.end();++it)
            if(it.key()!=QLatin1String("name") && it.key()!=QLatin1String("type") &&
               it.key()!=QLatin1String("initial") && it.key()!=QLatin1String("description"))
                return reject(error,QStringLiteral("Unknown variable field '%1'").arg(it.key()));
        const QString name=d.value(QStringLiteral("name")).toString();
        const QString type=d.value(QStringLiteral("type")).toString();
        if(!identifier(name)||names.contains(name))return reject(error,QStringLiteral("Invalid or duplicate variable '%1'").arg(name));
        names.insert(name);
        if(type!=QLatin1String("number") && type!=QLatin1String("string") && type!=QLatin1String("boolean"))
            return reject(error,QStringLiteral("Variable %1 needs type number, string, or boolean").arg(name));
        if(!d.value(QStringLiteral("description")).isUndefined() &&
           (!d.value(QStringLiteral("description")).isString()||d.value(QStringLiteral("description")).toString().size()>160))
            return reject(error,QStringLiteral("Variable description must be at most 160 characters"));
        if(d.contains(QStringLiteral("initial"))) {
            const auto initial=d.value(QStringLiteral("initial"));
            if(!scalar(initial) || (type==QLatin1String("number") && !initial.isDouble()) ||
                (type==QLatin1String("boolean") && !initial.isBool()) ||
                (type==QLatin1String("string") && (!initial.isString()||initial.toString().size()>500)))
                return reject(error,QStringLiteral("Variable %1 initial value does not match its type").arg(name));
        }
    }
    return true;
}
inline bool validate(const QJsonObject& condition,const QJsonArray& declared,QString* error=nullptr) {
    static const QSet<QString> keys={QStringLiteral("source"),QStringLiteral("key"),
        QStringLiteral("step"),QStringLiteral("path"),QStringLiteral("operator"),QStringLiteral("value")};
    for(auto it=condition.begin();it!=condition.end();++it)
        if(!keys.contains(it.key()))return reject(error,QStringLiteral("Unknown condition field '%1'").arg(it.key()));
    const QString source=condition.value(QStringLiteral("source")).toString();
    const QString op=condition.value(QStringLiteral("operator")).toString();
    if(!QSet<QString>{QStringLiteral("variable"),QStringLiteral("parameter"),QStringLiteral("runInfo"),
            QStringLiteral("measurement"),QStringLiteral("stepStatus"),QStringLiteral("counter")}.contains(source))
        return reject(error,QStringLiteral("Unknown condition data source '%1'").arg(source));
    if(!QSet<QString>{QStringLiteral("=="),QStringLiteral("!="),QStringLiteral("<"),
            QStringLiteral("<="),QStringLiteral(">"),QStringLiteral(">=")}.contains(op))
        return reject(error,QStringLiteral("Condition operator is invalid"));
    if(!scalar(condition.value(QStringLiteral("value"))))
        return reject(error,QStringLiteral("Condition comparison value must be a number, string or boolean"));
    const QString key=condition.value(QStringLiteral("key")).toString();
    const QString step=condition.value(QStringLiteral("step")).toString();
    if(source==QLatin1String("measurement") || source==QLatin1String("stepStatus")) {
        if(step.isEmpty()||step.size()>64)return reject(error,QStringLiteral("Select a preceding step ID"));
        if(source==QLatin1String("measurement") && condition.value(QStringLiteral("path")).toString().size()>100)
            return reject(error,QStringLiteral("Measurement path is too long"));
    } else if(!identifier(key))return reject(error,QStringLiteral("Condition needs a named source field"));
    if(source==QLatin1String("variable") && !declared.isEmpty()) {
        bool found=false;for(const auto& v:declared)if(v.toObject().value(QStringLiteral("name")).toString()==key)found=true;
        if(!found)return reject(error,QStringLiteral("Variable '%1' is not declared; add it to Test Variables").arg(key));
    }
    if(source==QLatin1String("counter") && !QSet<QString>{QStringLiteral("passedMeasurements"),
        QStringLiteral("failedMeasurements"),QStringLiteral("passedSteps"),QStringLiteral("failedSteps")}.contains(key))
        return reject(error,QStringLiteral("Unsupported test counter '%1'").arg(key));
    if(op!=QLatin1String("==") && op!=QLatin1String("!=") && !condition.value(QStringLiteral("value")).isDouble())
        return reject(error,QStringLiteral("Ordering comparisons require a numeric value"));
    return true;
}
inline bool compare(const QVariant& lhs,const QJsonObject& condition,bool* decision,QString* error=nullptr) {
    if(!decision)return reject(error,QStringLiteral("Missing comparison output"));
    if(!lhs.isValid()||lhs.isNull())return reject(error,QStringLiteral("Condition data is unavailable (not false)"));
    const QJsonValue rhs=condition.value(QStringLiteral("value"));
    const QString op=condition.value(QStringLiteral("operator")).toString();
    const int ty=lhs.metaType().id();
    bool match=false;
    if(rhs.isDouble()) {
        if(ty!=QMetaType::Double && ty!=QMetaType::Int && ty!=QMetaType::LongLong &&
           ty!=QMetaType::UInt && ty!=QMetaType::ULongLong)
            return reject(error,QStringLiteral("Condition expected numeric data"));
        const double a=lhs.toDouble(),b=rhs.toDouble();
        if(!std::isfinite(a))return reject(error,QStringLiteral("Condition value is not finite"));
        if(op==QLatin1String("=="))match=a==b;
        else if(op==QLatin1String("!="))match=a!=b;
        else if(op==QLatin1String("<"))match=a<b;
        else if(op==QLatin1String("<="))match=a<=b;
        else if(op==QLatin1String(">"))match=a>b;
        else match=a>=b;
    } else if(rhs.isBool()) {
        if(ty!=QMetaType::Bool)return reject(error,QStringLiteral("Condition expected boolean data"));
        if(op!=QLatin1String("==") && op!=QLatin1String("!="))return reject(error,QStringLiteral("Cannot order boolean data"));
        match=(lhs.toBool()==rhs.toBool());if(op==QLatin1String("!="))match=!match;
    } else {
        if(ty!=QMetaType::QString)return reject(error,QStringLiteral("Condition expected string data"));
        if(op!=QLatin1String("==") && op!=QLatin1String("!="))return reject(error,QStringLiteral("Cannot order string data"));
        match=(lhs.toString()==rhs.toString());if(op==QLatin1String("!="))match=!match;
    }
    *decision=match;return true;
}
} // namespace TestSequenceConditions
