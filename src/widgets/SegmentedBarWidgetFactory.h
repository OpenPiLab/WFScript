#pragma once
#include "WidgetFactory.h"

class SegmentedBarWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("segmented_bar");}
    QString displayName() const override {return QStringLiteral("Segmented Bar Meter");}
    QVector<WidgetProperty> properties() const override;
    PanelControl defaultDefinition(const QString& id) const override;
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget* widget,const PanelControl& definition) const override;
    void readDefinition(QWidget* widget,PanelControl& definition) const override;
    void applyRuntimeValue(QWidget* widget,const PanelControl& definition,const QVariant& value) const override;
};
