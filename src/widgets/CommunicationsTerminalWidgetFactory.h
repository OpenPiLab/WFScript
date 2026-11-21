#pragma once
#include "WidgetFactory.h"

class CommunicationsTerminalWidgetFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("communications_terminal");}
    QString displayName() const override {return QStringLiteral("Communications Terminal (Serial/TCP/UDP)");}
    QVector<WidgetProperty> properties() const override;
    QVector<PanelEventDefinition> events() const override {
        return {{QStringLiteral("sendRequested"),QStringLiteral("sendRequested(payload: object)"),
            QStringLiteral("User pressed Send. The payload contains mode ('text' or 'hex'), bytes (0..255 array), and text for text mode. A JavaScript event handler must route it to Serial/TCP.write or UDP.send. The widget never opens sockets or transmits automatically."),
            QStringLiteral("payload")}};
    }
    PanelControl defaultDefinition(const QString& id) const override;
    QVariant initialRuntimeValue(const PanelControl&) const override {return QVariantMap{};}
    QWidget* create(const PanelControl&,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget*,const PanelControl&) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget*,const PanelControl&,const QVariant&) const override;
};
