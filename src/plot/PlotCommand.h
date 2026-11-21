#pragma once

#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QVector>

// Value-only message from the JavaScript worker to the GUI thread.
// Never contains QWidget/QJSEngine/DWF pointers; never saved in .wfpanel.
struct PlotCommand {
    enum class Action { ReplaceSeries, AppendSeries, RemoveSeries, Clear, Configure, StyleSeries, SetView };
    Action action = Action::ReplaceSeries;
    QString plotId;
    QString seriesName;
    QVector<double> x;
    QVector<double> y;
    bool nativeSamples=false; // implicit shared vector, no JS per-sample conversion
    double nativeStart=0.0;   // same coordinate unit as the plot X axis
    double nativeStep=1.0;    // positive uniform X increment
    QVariantMap options;
};
Q_DECLARE_METATYPE(PlotCommand)
