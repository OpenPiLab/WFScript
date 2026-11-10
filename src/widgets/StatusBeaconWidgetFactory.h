#pragma once
#include "WidgetFactory.h"

class StatusBeaconWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("status_beacon");}
    QString displayName() const override {return QStringLiteral("Status Beacon");}
    QVector<WidgetProperty> properties() const override;
    PanelControl defaultDefinition(const QString& id) const override;
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget* widget,const PanelControl& definition) const override;
    void readDefinition(QWidget* widget,PanelControl& definition) const override;
    void applyRuntimeValue(QWidget* widget,const PanelControl& definition,const QVariant& value) const override;
};
