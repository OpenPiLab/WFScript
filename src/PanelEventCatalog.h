#pragma once
// One authoritative catalog for the events emitted by PanelCanvas's Qt widgets.
// The inspector, document validator, and runtime consult these same definitions.
#include <QJsonObject>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

struct PanelEventDefinition {
    QString name;
    QString signature;
    QString description;
    QString argument; // JS parameter name; empty for a button
};
namespace PanelEventCatalog {
// Optional new types register their events at factory-registration time.
// This catalog is shared between the GUI and JS runtime threads; registration
// must finish before the runtime starts. Existing built-in defaults remain.
inline QMutex& catalogMutex() { static QMutex mutex; return mutex; }
inline QHash<QString,QVector<PanelEventDefinition>>& catalog() {
    static QHash<QString,QVector<PanelEventDefinition>> events;
    return events;
}
inline void registerType(const QString& type,const QVector<PanelEventDefinition>& definitions) {
    QMutexLocker lock(&catalogMutex());
    catalog().insert(type,definitions);
}
inline QVector<PanelEventDefinition> forType(const QString& type) {
    {
        QMutexLocker lock(&catalogMutex());
        const auto it=catalog().constFind(type);
        if(it!=catalog().cend())return it.value();
    }
    if(type == QStringLiteral("button")) return {{QStringLiteral("clicked"), QStringLiteral("clicked"),
        QStringLiteral("Activated when the user clicks the button."), QString()}};
    if(type == QStringLiteral("slider")) return {{QStringLiteral("valueChanged"),QStringLiteral("valueChanged(value: number)"),
        QStringLiteral("Fires when the user changes the slider. Value is the actual configured numeric range."),QStringLiteral("value")}};
    if(type == QStringLiteral("toggle")) return {{QStringLiteral("toggled"),QStringLiteral("toggled(checked: boolean)"),
        QStringLiteral("Fires when the checkbox is toggled."),QStringLiteral("checked")}};
    return {}; // label, numeric, gauge and plot are display-only at present
}
inline bool supports(const QString& type,const QString& event) {
    for (const auto& entry : forType(type)) if(entry.name==event) return true;
    return false;
}
// Event arguments are not necessarily persistent widget values. A Button's
// clicked=true must never become its caption when switching Pages.
inline bool carriesPersistentState(const QString& event) {
    return event==QStringLiteral("valueChanged") || event==QStringLiteral("toggled");
}
inline bool validHandler(const QString& name) {
    static const QRegularExpression ident(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
    if(!ident.match(name).hasMatch())return false;
    // These cannot be ordinary JavaScript function declaration identifiers.
    static const QStringList reserved={QStringLiteral("for"),QStringLiteral("if"),
        QStringLiteral("while"),QStringLiteral("switch"),QStringLiteral("class"),
        QStringLiteral("return"),QStringLiteral("new"),QStringLiteral("var"),
        QStringLiteral("let"),QStringLiteral("const"),QStringLiteral("function"),
        QStringLiteral("default"),QStringLiteral("export"),QStringLiteral("import"),
        QStringLiteral("try"),QStringLiteral("catch"),QStringLiteral("throw"),
        QStringLiteral("break"),QStringLiteral("continue"),QStringLiteral("do"),
        QStringLiteral("delete"),QStringLiteral("this"),QStringLiteral("super"),
        QStringLiteral("true"),QStringLiteral("false"),QStringLiteral("null")};
    return !reserved.contains(name);
}
inline bool validateBindings(const QString& type,const QJsonObject& bindings,QString* error=nullptr) {
    for(auto it=bindings.begin();it!=bindings.end();++it) {
        if(!supports(type,it.key()) || !it.value().isObject()) {
            if(error)*error=QStringLiteral("Unsupported widget event: %1.%2").arg(type,it.key());
            return false;
        }
        const QJsonObject b=it.value().toObject();
        const QString handler=b.value(QStringLiteral("handler")).toString();
        if(!validHandler(handler) || !b.value(QStringLiteral("enabled")).isBool()) {
            if(error)*error=QStringLiteral("Invalid handler configuration for event %1").arg(it.key());
            return false;
        }
    }
    return true;
}
}
