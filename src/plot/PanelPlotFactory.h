#pragma once
#include "../widgets/WidgetFactory.h"

// Registry plugin for the independent Panel X/Y plotting widget.
class PanelPlotFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("plot");}
    QString displayName() const override {return QStringLiteral("Data Plot");}
    PanelControl defaultDefinition(const QString& id) const override;
    QVector<WidgetProperty> properties() const override;
    QWidget* create(const PanelControl& def,QWidget* parent,EventSink onUserEvent) const override;
    void applyDefinition(QWidget* widget,const PanelControl& def) const override;
    void readDefinition(QWidget* widget,PanelControl& def) const override;
    void applyRuntimeValue(QWidget*,const PanelControl&,const QVariant&) const override {}
    void deliverPlotCommand(QWidget* widget,const PlotCommand& command) const override;
};
