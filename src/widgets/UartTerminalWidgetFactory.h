#pragma once
#include "WidgetFactory.h"
class UartTerminalWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("uart_terminal");}
    QString displayName() const override {return QStringLiteral("UART Terminal (native RX)");}
    QVector<WidgetProperty> properties() const override;
    QVector<PanelEventDefinition> events() const override {
        return {{QStringLiteral("sendRequested"),QStringLiteral("sendRequested(text: string)"),
            QStringLiteral("Operator pressed Send; handler transmits text. Receive display is C++-driven."),
            QStringLiteral("text")}};
    }
    PanelControl defaultDefinition(const QString& id) const override;
    QVariant initialRuntimeValue(const PanelControl&) const override {return QVariantMap{};}
    QWidget* create(const PanelControl&,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget*,const PanelControl&) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget*,const PanelControl&,const QVariant&) const override;
};
