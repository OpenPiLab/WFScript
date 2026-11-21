#pragma once

// General-purpose communication terminal for native Serial, TCP and UDP.
// GUI thread only. Does not open a port, own a socket, or access QJSEngine.
// JavaScript routes its receive events via ui.set(widgetId,{append:{...}})
// and can transmit from the sendRequested event using any connection.
#include <QWidget>
#include <QVariant>
#include <QVariantMap>
#include <QString>
#include <functional>
class QLabel;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QComboBox;

class CommunicationsTerminalWidget final : public QWidget {
public:
    explicit CommunicationsTerminalWidget(QWidget* parent=nullptr);
    void configure(const QString& title,int maxLines,int previewBytes,
                   const QString& displayMode,bool timestamps);
    void setSendHandler(std::function<void(const QVariantMap&)> handler);
    void applyUpdate(const QVariant& value);
    QString contents() const;
    void clearOutput();
    qint64 receivedBytes() const {return m_received;}
    qint64 transmittedBytes() const {return m_transmitted;}
private:
    void appendEntry(const QVariantMap& entry);
    void submitSend();
    QString renderText(const QByteArray& data) const;
    void updateStats();
    QLabel* m_title=nullptr;
    QLabel* m_stats=nullptr;
    QLabel* m_error=nullptr;
    QPlainTextEdit* m_output=nullptr;
    QLineEdit* m_entry=nullptr;
    QComboBox* m_viewMode=nullptr;
    QComboBox* m_sendMode=nullptr;
    QPushButton* m_send=nullptr;
    QPushButton* m_clear=nullptr;
    std::function<void(const QVariantMap&)> m_sendHandler;
    QString m_caption;
    qint64 m_received=0;
    qint64 m_transmitted=0;
    int m_maxLines=500;
    int m_previewBytes=256;
    bool m_timestamps=true;
};
