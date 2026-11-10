#pragma once
#include "WidgetFactory.h"

class DigitalDisplayWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override { return QStringLiteral("digital_display"); }
    QString displayName() const override { return QStringLiteral("Digital Display"); }
    QVector<WidgetProperty> properties() const override;
    PanelControl defaultDefinition(const QString& id) const override;
    QWidget* create(const PanelControl& definition, QWidget* parent, EventSink sink) const override;
    void applyDefinition(QWidget* widget, const PanelControl& definition) const override;
    void readDefinition(QWidget* widget, PanelControl& definition) const override;
    void applyRuntimeValue(QWidget* widget, const PanelControl& definition,
                           const QVariant& value) const override;
};
