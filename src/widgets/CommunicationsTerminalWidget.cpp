#include "CommunicationsTerminalWidget.h"
#include <QLabel>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListView>
#include <QAbstractItemView>
#include <QPalette>
#include <QBrush>
#include <QColor>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextDocument>
#include <QTextCursor>
#include <QScrollBar>
#include <QDateTime>
#include <QRegularExpression>
#include <QFont>
#include <QByteArray>
#include <QStringList>
#include <QtGlobal>
#include <utility>

namespace {
QVariantList toList(const QByteArray& bytes) {
    QVariantList list;
    list.reserve(bytes.size());
    for(char ch:bytes)list.append(int(static_cast<unsigned char>(ch)));
    return list;
}
QString safeLabel(QString s,int maxLength=64) {
    s.replace(QLatin1Char('\n'),QLatin1Char(' '));
    s.replace(QLatin1Char('\r'),QLatin1Char(' '));
    return s.left(maxLength);
}
QString normalizeMode(const QString& mode) {
    if(mode.compare(QStringLiteral("Hex"),Qt::CaseInsensitive)==0)return QStringLiteral("Hex");
    if(mode.compare(QStringLiteral("Both"),Qt::CaseInsensitive)==0)return QStringLiteral("Both");
    return QStringLiteral("Text");
}
// Native Windows Qt styles can inherit a white popup surface while the Panel
// gives the combo light text. Force readable colours for BOTH the collapsed
// control and every unhovered popup item, not just :hover/:selected rows.
void applyTerminalComboContrast(QComboBox* combo) {
    const QColor ink(QStringLiteral("#f6f9ff"));
    const QColor surface(QStringLiteral("#203346"));
    const QColor popupSurface(QStringLiteral("#162a3c"));
    const QColor border(QStringLiteral("#57738b"));
    const QColor selection(QStringLiteral("#355a72"));
    QPalette comboPalette=combo->palette();
    comboPalette.setColor(QPalette::WindowText,ink);
    comboPalette.setColor(QPalette::Text,ink);
    comboPalette.setColor(QPalette::ButtonText,ink);
    comboPalette.setColor(QPalette::Base,surface);
    comboPalette.setColor(QPalette::Button,surface);
    comboPalette.setColor(QPalette::Highlight,selection);
    comboPalette.setColor(QPalette::HighlightedText,ink);
    combo->setPalette(comboPalette);
    combo->setStyleSheet(QStringLiteral(
        "QComboBox{background:%1;color:%2;border:1px solid %3;"
        "border-radius:4px;padding:5px 24px 5px 7px;}"
        "QComboBox QAbstractItemView{background:%4;color:%2;"
        "selection-background-color:%5;selection-color:%2;}")
        .arg(surface.name(),ink.name(),border.name(),popupSurface.name(),selection.name()));
    if(auto* popup=combo->view()) {
        QPalette popupPalette=popup->palette();
        popupPalette.setColor(QPalette::WindowText,ink);
        popupPalette.setColor(QPalette::Text,ink);
        popupPalette.setColor(QPalette::ButtonText,ink);
        popupPalette.setColor(QPalette::Base,popupSurface);
        popupPalette.setColor(QPalette::Window,popupSurface);
        popupPalette.setColor(QPalette::Highlight,selection);
        popupPalette.setColor(QPalette::HighlightedText,ink);
        popup->setPalette(popupPalette);
        popup->setAutoFillBackground(true);
        // Popup item ink is explicit even before the first mouse hover.
        popup->setStyleSheet(QStringLiteral(
            "QListView{background:%1;color:%2;border:1px solid %3;"
            "selection-background-color:%4;selection-color:%2;}"
            "QListView::item{background:%1;color:%2;min-height:23px;}"
            "QListView::item:selected{background:%4;color:%2;}"
            "QListView::item:hover{background:%4;color:%2;}")
            .arg(popupSurface.name(),ink.name(),border.name(),selection.name()));
    }
    for(int row=0;row<combo->count();++row)
        combo->setItemData(row,QBrush(ink),Qt::ForegroundRole);
}
}
CommunicationsTerminalWidget::CommunicationsTerminalWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("wfCommunicationsTerminal"));
    auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(9,9,9,9);
    layout->setSpacing(5);
    auto* head=new QHBoxLayout;
    m_title=new QLabel(this);m_title->setObjectName(QStringLiteral("commTerminalTitle"));
    m_stats=new QLabel(this);m_stats->setObjectName(QStringLiteral("commTerminalStats"));
    head->addWidget(m_title,1);head->addWidget(m_stats);
    layout->addLayout(head);

    auto* controls=new QHBoxLayout;
    auto* viewLabel=new QLabel(QStringLiteral("Display:"),this);
    m_viewMode=new QComboBox(this);m_viewMode->setObjectName(QStringLiteral("commTerminalViewMode"));
    m_viewMode->setView(new QListView(m_viewMode));
    m_viewMode->view()->setObjectName(QStringLiteral("commTerminalViewModePopup"));
    m_viewMode->addItems({QStringLiteral("Text"),QStringLiteral("Hex"),QStringLiteral("Both")});
    m_clear=new QPushButton(QStringLiteral("Clear"),this);
    m_clear->setObjectName(QStringLiteral("commTerminalClear"));
    controls->addWidget(viewLabel);controls->addWidget(m_viewMode);
    controls->addStretch(1);controls->addWidget(m_clear);
    layout->addLayout(controls);

    m_output=new QPlainTextEdit(this);
    m_output->setObjectName(QStringLiteral("commTerminalTranscript"));
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setFont(QFont(QStringLiteral("Consolas"),10));
    layout->addWidget(m_output,1);

    auto* sendRow=new QHBoxLayout;
    m_sendMode=new QComboBox(this);
    m_sendMode->setObjectName(QStringLiteral("commTerminalSendMode"));
    m_sendMode->setView(new QListView(m_sendMode));
    m_sendMode->view()->setObjectName(QStringLiteral("commTerminalSendModePopup"));
    m_sendMode->addItems({QStringLiteral("Text + LF"),QStringLiteral("Text (raw)"),QStringLiteral("Hex bytes")});
    m_entry=new QLineEdit(this);
    m_entry->setObjectName(QStringLiteral("commTerminalSendEntry"));
    m_entry->setPlaceholderText(QStringLiteral("Type text; or choose Hex bytes: 00 7F 80 FF"));
    m_send=new QPushButton(QStringLiteral("Send"),this);
    m_send->setObjectName(QStringLiteral("commTerminalSend"));
    sendRow->addWidget(m_sendMode);sendRow->addWidget(m_entry,1);sendRow->addWidget(m_send);
    layout->addLayout(sendRow);
    m_error=new QLabel(this);
    m_error->setObjectName(QStringLiteral("commTerminalMessage"));
    m_error->setWordWrap(true);
    layout->addWidget(m_error);
    connect(m_clear,&QPushButton::clicked,this,[this](){clearOutput();});
    connect(m_send,&QPushButton::clicked,this,[this](){submitSend();});
    connect(m_entry,&QLineEdit::returnPressed,this,[this](){submitSend();});
    connect(m_sendMode,&QComboBox::currentIndexChanged,this,[this](int i){
        m_error->clear();
        m_entry->setPlaceholderText(i==2 ? QStringLiteral("Hex bytes: 00 7F 80 FF")
           : QStringLiteral("Type UTF-8 text to send"));
    });
    configure(QStringLiteral("Communications Terminal"),500,256,QStringLiteral("Both"),true);
}
void CommunicationsTerminalWidget::configure(const QString& title,int maxLines,int previewBytes,
                                              const QString& displayMode,bool timestamps) {
    m_caption=title.left(100);
    m_maxLines=qBound(50,maxLines,5000);
    m_previewBytes=qBound(16,previewBytes,1024);
    m_timestamps=timestamps;
    m_title->setText(m_caption.isEmpty()?QStringLiteral("Communications Terminal"):m_caption);
    m_viewMode->setCurrentText(normalizeMode(displayMode));
    m_output->document()->setMaximumBlockCount(m_maxLines);
    updateStats();
    setStyleSheet(QStringLiteral(
        "QWidget#wfCommunicationsTerminal{background:#142435;border:1px solid #385269;border-radius:7px;}"
        "QLabel{color:#e1edf7;}QLabel#commTerminalMessage{color:#ffcc92;}"
        "QPlainTextEdit{background:#0b1520;color:#a5edce;border:1px solid #385269;}"
        "QLineEdit{background:#203346;color:#f6f9ff;border:1px solid #57738b;padding:5px;}"
        "QPushButton{background:#236b83;color:white;padding:5px 12px;border-radius:4px;}"));
    applyTerminalComboContrast(m_viewMode);
    applyTerminalComboContrast(m_sendMode);
}
void CommunicationsTerminalWidget::setSendHandler(std::function<void(const QVariantMap&)> handler) {
    m_sendHandler=std::move(handler);
}
void CommunicationsTerminalWidget::updateStats() {
    m_stats->setText(QStringLiteral("RX %1 B  |  TX %2 B").arg(m_received).arg(m_transmitted));
}
void CommunicationsTerminalWidget::clearOutput() {
    m_output->clear();m_error->clear();m_received=0;m_transmitted=0;updateStats();
}
QString CommunicationsTerminalWidget::renderText(const QByteArray& data) const {
    QString text=QString::fromUtf8(data);
    text.replace(QLatin1Char('\\'),QStringLiteral("\\\\"));
    text.replace(QLatin1Char('\r'),QStringLiteral("\\r"));
    text.replace(QLatin1Char('\n'),QStringLiteral("\\n"));
    text.replace(QLatin1Char('\t'),QStringLiteral("\\t"));
    text.replace(QLatin1Char('\0'),QStringLiteral("\\0"));
    // Keep each message on one bounded transcript line (no hidden raw control chars).
    for(int i=0;i<text.size();++i) {
        if(text.at(i).unicode()<0x20)text[i]=QChar(0x2423);
    }
    return text;
}
void CommunicationsTerminalWidget::appendEntry(const QVariantMap& entry) {
    const QString dir=safeLabel(entry.value(QStringLiteral("direction"),QStringLiteral("INFO")).toString().toUpper(),12);
    const QString transport=safeLabel(entry.value(QStringLiteral("transport"),QStringLiteral("COMM")).toString().toUpper(),12);
    const QString peer=safeLabel(entry.value(QStringLiteral("peer")).toString());
    const bool hasBytes=entry.contains(QStringLiteral("data")) || entry.contains(QStringLiteral("bytes"));
    const QVariantList values=entry.value(QStringLiteral("data"),entry.value(QStringLiteral("bytes"))).toList();
    QByteArray bytes;
    if(hasBytes) {
        // A malicious Panel can supply malformed data; reject it, never wrap integers.
        if(values.size()>65536){m_error->setText(QStringLiteral("Terminal entry exceeds 65536 bytes"));return;}
        bytes.reserve(values.size());
        for(const QVariant& v:values) {
            bool ok=false;
            const int b=v.toInt(&ok);
            if(!ok || b<0 || b>255){m_error->setText(QStringLiteral("Terminal entry contains an invalid byte"));return;}
            bytes.append(char(b));
        }
    } else if(entry.contains(QStringLiteral("text"))) {
        bytes=entry.value(QStringLiteral("text")).toString().toUtf8();
    }
    if(dir==QStringLiteral("RX"))m_received+=bytes.size();
    else if(dir==QStringLiteral("TX"))m_transmitted+=bytes.size();
    const QByteArray preview=bytes.left(m_previewBytes);
    const QString display=m_viewMode->currentText();
    QString payload;
    if(display!=QStringLiteral("Hex"))payload=renderText(preview);
    if(display!=QStringLiteral("Text")){
        QStringList hex;
        hex.reserve(preview.size());
        for(char c:preview)hex.append(QStringLiteral("%1").arg(int(static_cast<unsigned char>(c)),2,16,QLatin1Char('0')).toUpper());
        if(!payload.isEmpty())payload+=QStringLiteral("  |  ");
        payload+=hex.join(QLatin1Char(' '));
    }
    if(bytes.size()>m_previewBytes)payload+=QStringLiteral(" ... (+%1 B)").arg(bytes.size()-m_previewBytes);
    if(!hasBytes && bytes.isEmpty() && entry.contains(QStringLiteral("note")))
        payload=safeLabel(entry.value(QStringLiteral("note")).toString(),240);
    const QString header=(m_timestamps?QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz ")):QString())+
                         QStringLiteral("[%1 %2%3]").arg(transport,dir,peer.isEmpty()?QString():QStringLiteral(" @")+peer);
    auto* bar=m_output->verticalScrollBar();
    const bool follow=bar->value()>=bar->maximum()-2;
    m_output->appendPlainText(header+QStringLiteral("  ")+payload);
    if(follow)bar->setValue(bar->maximum());
    updateStats();
}
void CommunicationsTerminalWidget::applyUpdate(const QVariant& value) {
    if(!value.canConvert<QVariantMap>())return;
    const QVariantMap obj=value.toMap();
    if(obj.value(QStringLiteral("clear")).toBool())clearOutput();
    if(obj.contains(QStringLiteral("displayMode")))m_viewMode->setCurrentText(normalizeMode(obj.value(QStringLiteral("displayMode")).toString()));
    if(obj.contains(QStringLiteral("status")))m_error->setText(safeLabel(obj.value(QStringLiteral("status")).toString(),240));
    if(obj.value(QStringLiteral("append")).canConvert<QVariantMap>())appendEntry(obj.value(QStringLiteral("append")).toMap());
}
void CommunicationsTerminalWidget::submitSend() {
    if(!m_sendHandler)return;
    const QString input=m_entry->text();
    if(input.isEmpty())return;
    QVariantMap payload;
    QByteArray data;
    if(m_sendMode->currentIndex()==2) {
        const QStringList tokens=input.split(QRegularExpression(QStringLiteral("[\\s,;]+")),Qt::SkipEmptyParts);
        if(tokens.isEmpty() || tokens.size()>65536){m_error->setText(QStringLiteral("Invalid hex byte count"));return;}
        static const QRegularExpression pattern(QStringLiteral("^(?:0[xX])?([0-9a-fA-F]{1,2})$"));
        for(const QString& token:tokens) {
            const auto match=pattern.match(token);
            if(!match.hasMatch()){m_error->setText(QStringLiteral("Invalid hex byte: ")+safeLabel(token,40));return;}
            bool ok=false;const int b=match.captured(1).toInt(&ok,16);
            if(!ok){m_error->setText(QStringLiteral("Invalid hexadecimal data"));return;}
            data.append(char(b));
        }
        payload.insert(QStringLiteral("mode"),QStringLiteral("hex"));
        payload.insert(QStringLiteral("bytes"),toList(data));
    } else {
        const QString text=input+(m_sendMode->currentIndex()==0?QStringLiteral("\n"):QString());
        data=text.toUtf8();
        if(data.size()>65536){m_error->setText(QStringLiteral("Text exceeds 65536 UTF-8 bytes"));return;}
        payload.insert(QStringLiteral("mode"),QStringLiteral("text"));
        payload.insert(QStringLiteral("text"),text);
        payload.insert(QStringLiteral("bytes"),toList(data));
    }
    m_error->clear();
    // GUI-only signal: script must select the correct connection and destination.
    // Do not claim TX success or clear text until the handler handles the event.
    m_sendHandler(payload);
}
QString CommunicationsTerminalWidget::contents() const {return m_output->toPlainText();}
