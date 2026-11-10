#include "WidgetDocumentCodec.h"
#include "WidgetRegistry.h"
#include "../PanelEventCatalog.h"
#include <QJsonObject>
#include <QJsonDocument>
#include "EducationWidgets.h"
#include "ExperimentWidgets.h"
#include <QSet>
#include <cmath>
#include <utility>

namespace WidgetDocumentCodec {
QJsonObject encode(const PanelControl& c) {
    QJsonObject result{{"id",c.id},{"type",c.type},{"text",c.text},
        {"x",c.geometry.x()},{"y",c.geometry.y()},
        {"width",c.geometry.width()},{"height",c.geometry.height()},
        {"value",c.value},{"min",c.minimum},{"max",c.maximum},
        {"events",c.events}};
    if(!c.properties.isEmpty()) result.insert("properties",c.properties);
    return result;
}
bool decode(const QJsonObject& o,PanelControl* output,QString* error) {
    if(!output)return false;
    PanelControl d;
    d.id=o.value("id").toString();d.type=o.value("type").toString();
    d.text=o.value("text").toString();
    d.geometry=QRect(o.value("x").toInt(),o.value("y").toInt(),
        o.value("width").toInt(),o.value("height").toInt());
    d.value=o.value("value").toDouble();
    d.minimum=o.value("min").toDouble();d.maximum=o.value("max").toDouble(100.0);
    if(o.contains("events") && !o.value("events").isObject()){
        if(error)*error=QStringLiteral("Invalid events on %1").arg(d.id);
        return false;
    }
    d.events=o.value("events").toObject();
    if(o.contains("properties") && !o.value("properties").isObject()) {
        if(error)*error=QStringLiteral("Invalid widget properties on %1").arg(d.id);
        return false;
    }
    d.properties=o.value("properties").toObject();
    if(d.type==QStringLiteral("image")) {
        const QJsonValue raw=d.properties.value(QStringLiteral("imageData"));
        const QString data=raw.toString();
        if((!raw.isUndefined() && !raw.isString()) ||
           data.size()>EmbeddedImage::MaxPropertyBytes ||
           (!data.isEmpty() && EmbeddedImage::decode(data).isNull())) {
            if(error)*error=QStringLiteral("Invalid or oversized embedded image on %1").arg(d.id);
            return false;
        }
        // Metadata excluding the Base64 image is subject to the normal limit.
        QJsonObject meta=d.properties;meta.remove(QStringLiteral("imageData"));
        if(QJsonDocument(meta).toJson(QJsonDocument::Compact).size()>16384) {
            if(error)*error=QStringLiteral("Image metadata too large on %1").arg(d.id);
            return false;
        }
    } else if(QJsonDocument(d.properties).toJson(QJsonDocument::Compact).size()>16384) {
        if(error)*error=QStringLiteral("Widget properties too large on %1").arg(d.id);
        return false;
    }
    if(d.type==QStringLiteral("equation")) {
        QString mathError;
        const QJsonValue source=d.properties.value(QStringLiteral("source"));
        if((!source.isUndefined() && !source.isString()) ||
           !EquationMath::validSource(source.toString(),&mathError)) {
            if(error)*error=QStringLiteral("Invalid equation on %1: %2").arg(d.id,mathError);
            return false;
        }
    }
    if(d.type==QStringLiteral("experiment_table")) {
        const QStringList headers=d.properties.value(QStringLiteral("columns")).toString().split('|',Qt::KeepEmptyParts);
        const QString rows=d.properties.value(QStringLiteral("initialRows")).toString();
        const int cap=d.properties.value(QStringLiteral("maxRows")).toInt(250);
        bool validHeaders=!headers.isEmpty() && headers.size()<=8;
        for(const auto& heading:headers)if(heading.trimmed().isEmpty() || heading.size()>100)validHeaders=false;
        const QStringList data=rows.split('\n',Qt::SkipEmptyParts);
        if(!validHeaders || cap<1 || cap>250 || data.size()>cap || rows.size()>12000) {
            if(error)*error=QStringLiteral("Invalid experiment table dimensions or headers on %1").arg(d.id);
            return false;
        }
        for(const auto& row:data) {
            const QStringList fields=row.split('|',Qt::KeepEmptyParts);
            if(fields.size()!=headers.size()) {
                if(error)*error=QStringLiteral("Experiment table row column count on %1 does not match headers").arg(d.id);
                return false;
            }
            for(const QString& field:fields)if(field.size()>256){
                if(error)*error=QStringLiteral("Experiment table cell too long on %1").arg(d.id);
                return false;
            }
        }
    }
    // Register all linked factories before validating factory-declared events.
    const auto* registeredFactory=WidgetRegistry::instance().factory(d.type);
    if(registeredFactory) for(const WidgetProperty& field:registeredFactory->properties()) {
        if(field.id=="text" || field.id=="geometry" || field.id=="value" ||
           field.id=="min" || field.id=="max" || !d.properties.contains(field.id)) continue;
        const QJsonValue v=d.properties.value(field.id);
        const bool valid = (field.kind=="string" || field.kind=="multiline") ? v.isString()
            : field.kind=="boolean" ? v.isBool()
            : field.kind=="integer" ? (v.isDouble() && std::isfinite(v.toDouble()) &&
                 v.toDouble()==std::trunc(v.toDouble()) && v.toDouble()>=field.minimum && v.toDouble()<=field.maximum)
            : field.kind=="double" ? (v.isDouble() && std::isfinite(v.toDouble()) &&
                 v.toDouble()>=field.minimum && v.toDouble()<=field.maximum)
            : field.kind=="choice" ? (v.isString() && field.choices.contains(v.toString()))
            : false;
        if(!valid) {
            if(error)*error=QStringLiteral("Invalid %1 property on %2").arg(field.id,d.id);
            return false;
        }
    }
    if(!PanelEventCatalog::validateBindings(d.type,d.events,error))return false;
    if(d.id.isEmpty() || !registeredFactory ||
       d.geometry.width()<25 || d.geometry.height()<22 ||
       d.geometry.width()>3000 || d.geometry.height()>3000 ||
       d.geometry.x()+d.geometry.width()>8192 ||
       d.geometry.y()+d.geometry.height()>8192 ||
       !std::isfinite(d.minimum) || !std::isfinite(d.maximum) ||
       !std::isfinite(d.value) || d.minimum>=d.maximum) {
        if(error)*error=QStringLiteral("Invalid control: %1").arg(d.id);
        return false;
    }
    *output=std::move(d);
    return true;
}
bool decodeList(const QJsonArray& json,QVector<PanelControl>* definitions,QString* error) {
    if(!definitions)return false;
    if(json.size()>200) {if(error)*error="Too many panel controls";return false;}
    QVector<PanelControl> parsed;
    QSet<QString> names;
    for(const auto& value:json) {
        if(!value.isObject()){if(error)*error="Invalid control entry";return false;}
        PanelControl d;
        if(!decode(value.toObject(),&d,error))return false;
        if(names.contains(d.id)) {
            if(error)*error=QStringLiteral("Invalid control: %1").arg(d.id);
            return false;
        }
        names.insert(d.id);
        parsed.append(std::move(d));
    }
    *definitions=std::move(parsed);
    return true;
}
}
