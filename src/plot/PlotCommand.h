#pragma once

#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QVector>

// Value-only message from the JavaScript worker to the GUI thread.
// Never contains QWidget/QJSEngine/DWF pointers; never saved in .wfpanel.
struct PlotCommand {
    enum class Action { ReplaceSeries, AppendSeries, RemoveSeries, Clear, Configure, StyleSeries };
    Action action = Action::ReplaceSeries;
    QString plotId;
    QString seriesName;
    QVector<double> x;
    QVector<double> y;
    QVariantMap options;
};
Q_DECLARE_METATYPE(PlotCommand)
