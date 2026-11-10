#pragma once

#include <QJsonObject>
#include <QRect>
#include <QString>

// Stable on-disk definition; do not confuse this with live QWidget state.
// A plot owns its captured samples, zoom and cursors independently.
struct PanelControl {
    QString id;
    QString type;
    QString text;
    QRect geometry;
    double value = 0;
    double minimum = 0;
    double maximum = 100;
    // eventName -> {handler,enabled}; saved in .wfpanel, not in the widget.
    QJsonObject events;
    // Optional, factory-owned settings; legacy document keys remain unchanged.
    QJsonObject properties;
};
