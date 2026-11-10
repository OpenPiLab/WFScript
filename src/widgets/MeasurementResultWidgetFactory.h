#pragma once
#include "WidgetFactory.h"
class MeasurementResultWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("measurement_result");}
    QString displayName() const override {return QStringLiteral("Measurement Result");}
    QVector<WidgetProperty> properties() const override;
    PanelControl defaultDefinition(const QString& id) const override;
    QVariant initialRuntimeValue(const PanelControl&) const override {return QVariant();}
    QWidget* create(const PanelControl&,QWidget*,EventSink) const override;
    void applyDefinition(QWidget*,const PanelControl&) const override;
    void readDefinition(QWidget*,PanelControl&) const override;
    void applyRuntimeValue(QWidget*,const PanelControl&,const QVariant&) const override;
};
