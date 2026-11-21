#include "UartTerminalWidget.h"
#include <QLabel>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>
#include <QTextDocument>
#include <QScrollBar>
#include <QVariantMap>
#include <QByteArray>
#include <QFont>
#include <QMetaType>
#include <utility>
#include <QtGlobal>

UartTerminalWidget::UartTerminalWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("wfUartTerminal"));
    auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(9,9,9,9);layout->setSpacing(5);
    auto* header=new QHBoxLayout;
    m_title=new QLabel(this);m_title->setObjectName(QStringLiteral("uartTerminalHeading"));
    m_stats=new QLabel(this);m_stats->setObjectName(QStringLiteral("uartTerminalStats"));
    header->addWidget(m_title,1);header->addWidget(m_stats,0,Qt::AlignRight);
    layout->addLayout(header);
    m_output=new QPlainTextEdit(this);
    m_output->setObjectName(QStringLiteral("uartTerminalRx"));
    m_output->setReadOnly(true);m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setFont(QFont(QStringLiteral("Consolas"),10));
    m_output->document()->setMaximumBlockCount(m_maxLines);
    layout->addWidget(m_output,1);
    auto* transmit=new QHBoxLayout;
    m_entry=new QLineEdit(this);m_entry->setObjectName(QStringLiteral("uartTerminalTx"));
    m_entry->setPlaceholderText(QStringLiteral("Type a message to transmit (Enter)"));
    m_send=new QPushButton(QStringLiteral("Send + LF"),this);
    transmit->addWidget(m_entry,1);transmit->addWidget(m_send);
    layout->addLayout(transmit);
    configure(QStringLiteral("UART Terminal"),1000,false);
}
void UartTerminalWidget::setSendHandler(std::function<void(const QString&)> handler) {
    auto action=[this,handler=std::move(handler)](){
        if(m_entry->text().isEmpty())return;
        handler(m_entry->text());m_entry->clear();
    };
    connect(m_send,&QPushButton::clicked,this,action);
    connect(m_entry,&QLineEdit::returnPressed,this,action);
}
void UartTerminalWidget::configure(const QString& title,int maxLines,bool hexMode) {
    m_caption=title.left(100);m_hex=hexMode;m_maxLines=qBound(100,maxLines,10000);
    m_title->setText(m_caption.isEmpty()?QStringLiteral("UART Terminal"):m_caption);
    m_output->document()->setMaximumBlockCount(m_maxLines);
    m_stats->setText(QStringLiteral("RX %1 bytes | overflow %2").arg(m_received).arg(m_overflows));
    setStyleSheet(QStringLiteral(
      "QWidget#wfUartTerminal{background:#152436;border:1px solid #385269;border-radius:8px;}"
      "QLabel{color:#e3edf7;}QPlainTextEdit{background:#0c1622;color:#a9efcc;border:1px solid #385269;}"
      "QLineEdit{background:#203347;color:#f4f8ff;border:1px solid #50718b;padding:5px;}"
      "QPushButton{background:#236b83;color:#fff;padding:5px 13px;border-radius:4px;}"));
}
void UartTerminalWidget::clearOutput(){m_output->clear();m_received=0;m_overflows=0;configure(m_caption,m_maxLines,m_hex);}
void UartTerminalWidget::appendChunk(const QVariantMap& payload) {
    const QVariantList bytes=payload.value(QStringLiteral("data")).toList();
    QString text;
    if(m_hex){
        for(const QVariant& item:bytes){
            if(!text.isEmpty())text+=QLatin1Char(' ');
            text+=QStringLiteral("%1").arg(item.toInt()&255,2,16,QLatin1Char('0')).toUpper();
        }
        if(!text.isEmpty())text+=QLatin1Char('\n');
    } else {
        QByteArray binary;binary.reserve(bytes.size());
        for(const QVariant& item:bytes)binary.append(char(item.toInt()&255));
        text=QString::fromUtf8(binary);
        text.replace(QLatin1Char('\0'),QChar(0x2400));
    }
    // Insert, don't appendPlainText(): a byte chunk need not end at a line
    // boundary, and appendPlainText() inserts a spurious newline per poll.
    if(!text.isEmpty()){
        auto* bar=m_output->verticalScrollBar();
        const bool stick=bar->value()>=bar->maximum()-2;
        QTextCursor cursor=m_output->textCursor();
        cursor.movePosition(QTextCursor::End);cursor.insertText(text);
        if(stick)m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
    }
    m_received+=bytes.size();
    if(payload.value(QStringLiteral("overflow")).toBool())++m_overflows;
    m_stats->setText(QStringLiteral("RX %1 bytes | SDK overflow %2").arg(m_received).arg(m_overflows));
}
void UartTerminalWidget::applyUpdate(const QVariant& value){
    if(value.metaType().id()!=QMetaType::QVariantMap)return;
    const QVariantMap payload=value.toMap();
    if(payload.value(QStringLiteral("clear")).toBool()){clearOutput();return;}
    if(payload.contains(QStringLiteral("data")))appendChunk(payload);
}
QString UartTerminalWidget::contents() const{return m_output->toPlainText();}
