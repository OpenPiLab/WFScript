#pragma once
#include <QWidget>
#include <QVariant>
#include <QString>
#include <functional>
class QLabel;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;

// UI-only component. Never holds an SDK handle or calls QJSEngine.
// The native PanelRuntime forwards ProtocolUartPort::dataReceived via queued
// controlUpdate to this widget, even on a hidden Page. TX is an optional
// operator event delivered to JavaScript; RX does not require script code.
class UartTerminalWidget final : public QWidget {
public:
    explicit UartTerminalWidget(QWidget* parent=nullptr);
    void configure(const QString& title,int maxLines,bool hexMode);
    void setSendHandler(std::function<void(const QString&)> handler);
    void applyUpdate(const QVariant& value);
    QString contents() const;
    qint64 receivedBytes() const {return m_received;}
    void clearOutput();
private:
    void appendChunk(const QVariantMap& payload);
    QLabel* m_title=nullptr;
    QLabel* m_stats=nullptr;
    QPlainTextEdit* m_output=nullptr;
    QLineEdit* m_entry=nullptr;
    QPushButton* m_send=nullptr;
    QString m_caption;
    qint64 m_received=0;
    qint64 m_overflows=0;
    bool m_hex=false;
    int m_maxLines=1000;
};
