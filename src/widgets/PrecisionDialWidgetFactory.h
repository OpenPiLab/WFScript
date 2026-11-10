#pragma once
#include "WidgetFactory.h"

class PrecisionDialWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("precision_dial");}
    QString displayName() const override {return QStringLiteral("Precision Dial");}
    QVector<WidgetProperty> properties() const override;
    PanelControl defaultDefinition(const QString& id) const override;
    QWidget* create(const PanelControl& definition,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget* widget,const PanelControl& definition) const override;
    void readDefinition(QWidget* widget,PanelControl& definition) const override;
    void applyRuntimeValue(QWidget* widget,const PanelControl& definition,const QVariant& value) const override;
};
