#pragma once
#include "WidgetDefinition.h"
#include <QJsonArray>
#include <QVector>

// v1/v2 .wfpanel control schema remains unchanged: id, type, text,
// x/y/width/height, value, min, max, events. No widget runtime state is saved.
namespace WidgetDocumentCodec {
QJsonObject encode(const PanelControl& definition);
bool decode(const QJsonObject& json, PanelControl* definition, QString* error=nullptr);
bool decodeList(const QJsonArray& json, QVector<PanelControl>* definitions, QString* error=nullptr);
}
