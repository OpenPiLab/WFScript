#pragma once
#include "PanelPlotFactory.h"
// Separate Toolbox widget type with a dedicated lane renderer. It accepts the
// same plots.setSeries/appendPoint/configure messages as ordinary plots.
class DigitalTimingPlotFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("digital_plot");}
    QString displayName() const override {return QStringLiteral("Digital Timing / Protocol");}
    PanelControl defaultDefinition(const QString& id) const override;
    QVector<WidgetProperty> properties() const override;
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget* w,const PanelControl& d) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget*,const PanelControl&,const QVariant&) const override {}
    void deliverPlotCommand(QWidget* w,const PlotCommand& c) const override;
};
