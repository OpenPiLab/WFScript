#pragma once
#include "WidgetFactory.h"

class TestSequenceWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("test_sequence");}
    QString displayName() const override {return QStringLiteral("Test Sequence / Progress");}
    QVector<WidgetProperty> properties() const override;
    PanelControl defaultDefinition(const QString& id) const override;
    QVariant initialRuntimeValue(const PanelControl& d) const override;
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink) const override;
    void applyDefinition(QWidget*,const PanelControl&) const override;
    void readDefinition(QWidget*,PanelControl&) const override;
    void applyRuntimeValue(QWidget*,const PanelControl&,const QVariant&) const override;
private:
    static QVariantList initialSteps(const PanelControl& d);
};
