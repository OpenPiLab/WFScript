#pragma once
#include "WidgetFactory.h"
class EventLogWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("event_log");}
    QString displayName() const override {return QStringLiteral("Event Log");}
    QVector<WidgetProperty> properties() const override;
    PanelControl defaultDefinition(const QString& id) const override;
    QVariant initialRuntimeValue(const PanelControl&) const override {return QVariantList{};}
    QWidget* create(const PanelControl&,QWidget*,EventSink) const override;
    void applyDefinition(QWidget*,const PanelControl&) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget*,const PanelControl&,const QVariant&) const override;
};
