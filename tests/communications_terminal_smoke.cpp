// Native Qt widget smoke, offscreen. No serial port, network or AD3 required.
#include "widgets/CommunicationsTerminalWidget.h"
#include "widgets/CommunicationsTerminalWidgetFactory.h"
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <QPalette>
#include <QBrush>
#include <QColor>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextDocument>
#include <QDebug>
#include <memory>

#define CHECK(x) do {if(!(x)){qCritical()<<"Communications Terminal failure:"<<#x<<"at line"<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    CommunicationsTerminalWidgetFactory factory;
    CHECK(factory.typeId()==QStringLiteral("communications_terminal"));
    CHECK(factory.events().size()==1);
    CHECK(factory.events().first().name==QStringLiteral("sendRequested"));
    PanelControl def=factory.defaultDefinition(QStringLiteral("term"));
    QVariantMap sent;
    int callbacks=0;
    std::unique_ptr<QWidget> base(factory.create(def,nullptr,[&](const QString& name,const QVariant& value){
        if(name==QStringLiteral("sendRequested")){++callbacks;sent=value.toMap();}
    }));
    auto* terminal=dynamic_cast<CommunicationsTerminalWidget*>(base.get());
    CHECK(terminal);
    auto* transcript=terminal->findChild<QPlainTextEdit*>(QStringLiteral("commTerminalTranscript"));
    auto* view=terminal->findChild<QComboBox*>(QStringLiteral("commTerminalViewMode"));
    auto* mode=terminal->findChild<QComboBox*>(QStringLiteral("commTerminalSendMode"));
    auto* edit=terminal->findChild<QLineEdit*>(QStringLiteral("commTerminalSendEntry"));
    auto* send=terminal->findChild<QPushButton*>(QStringLiteral("commTerminalSend"));
    auto* clear=terminal->findChild<QPushButton*>(QStringLiteral("commTerminalClear"));
    CHECK(transcript && view && mode && edit && send && clear);
    // Regression: neither terminal dropdown may depend on hovering a row to
    // reveal its text (Windows native Qt popup palette inheritance).
    for(QComboBox* combo:{view,mode}) {
        CHECK(combo->view());
        CHECK(combo->view()->objectName().endsWith(QStringLiteral("Popup")));
        CHECK(combo->palette().color(QPalette::Text)!=combo->palette().color(QPalette::Base));
        const QPalette popup=combo->view()->palette();
        const QColor itemInk=popup.color(QPalette::Text);
        CHECK(itemInk!=popup.color(QPalette::Base));
        CHECK(popup.color(QPalette::HighlightedText)!=popup.color(QPalette::Highlight));
        for(int row=0;row<combo->count();++row) {
            const QVariant foreground=combo->itemData(row,Qt::ForegroundRole);
            CHECK(foreground.canConvert<QBrush>());
            CHECK(foreground.value<QBrush>().color()==itemInk);
        }
    }
    CHECK(transcript->document()->maximumBlockCount()==500);
    CHECK(view->currentText()==QStringLiteral("Both"));
    factory.applyRuntimeValue(terminal,def,QVariantMap{{QStringLiteral("append"),QVariantMap{
        {QStringLiteral("transport"),QStringLiteral("TCP")},{QStringLiteral("direction"),QStringLiteral("RX")},
        {QStringLiteral("data"),QVariantList{72,105,10,0,255}}}}});
    CHECK(terminal->receivedBytes()==5);
    CHECK(transcript->toPlainText().contains(QStringLiteral("TCP RX")));
    CHECK(transcript->toPlainText().contains(QStringLiteral("48 69 0A 00 FF")));
    CHECK(transcript->toPlainText().contains(QStringLiteral("\\n")));
    factory.applyRuntimeValue(terminal,def,QVariantMap{{QStringLiteral("append"),QVariantMap{
        {QStringLiteral("transport"),QStringLiteral("UDP")},{QStringLiteral("direction"),QStringLiteral("TX")},
        {QStringLiteral("peer"),QStringLiteral("127.0.0.1:9000")},
        {QStringLiteral("data"),QVariantList{0,127,128,255}}}}});
    CHECK(terminal->transmittedBytes()==4);
    CHECK(transcript->toPlainText().contains(QStringLiteral("@127.0.0.1:9000")));
    factory.applyRuntimeValue(terminal,def,QVariantMap{{QStringLiteral("displayMode"),QStringLiteral("Hex")}});
    CHECK(view->currentText()==QStringLiteral("Hex"));
    factory.applyRuntimeValue(terminal,def,QVariantMap{{QStringLiteral("append"),QVariantMap{
        {QStringLiteral("transport"),QStringLiteral("SERIAL")},{QStringLiteral("direction"),QStringLiteral("RX")},
        {QStringLiteral("data"),QVariantList{0,1,2}}}}});
    CHECK(terminal->contents().contains(QStringLiteral("00 01 02")));
    CHECK(terminal->receivedBytes()==8);
    edit->setText(QStringLiteral("SCPI *IDN?"));send->click();
    CHECK(callbacks==1 && sent.value(QStringLiteral("mode")).toString()==QStringLiteral("text"));
    CHECK(sent.value(QStringLiteral("text")).toString()==QStringLiteral("SCPI *IDN?\n"));
    CHECK(sent.value(QStringLiteral("bytes")).toList().last().toInt()==10);
    mode->setCurrentText(QStringLiteral("Hex bytes"));
    edit->setText(QStringLiteral("0x00, 7f 80 FF"));send->click();
    CHECK(callbacks==2 && sent.value(QStringLiteral("mode")).toString()==QStringLiteral("hex"));
    CHECK(sent.value(QStringLiteral("bytes")).toList()==QVariantList({0,127,128,255}));
    edit->setText(QStringLiteral("G1"));send->click();
    CHECK(callbacks==2); // reject invalid hexadecimal payload
    mode->setCurrentText(QStringLiteral("Text (raw)"));
    edit->setText(QStringLiteral("A"));send->click();
    CHECK(callbacks==3 && sent.value(QStringLiteral("text")).toString()==QStringLiteral("A"));
    def.properties.insert(QStringLiteral("maxLines"),70);
    def.properties.insert(QStringLiteral("previewBytes"),16);
    def.properties.insert(QStringLiteral("displayMode"),QStringLiteral("Text"));
    def.properties.insert(QStringLiteral("showTimestamp"),false);
    factory.applyDefinition(terminal,def);
    CHECK(transcript->document()->maximumBlockCount()==70);
    CHECK(view->currentText()==QStringLiteral("Text"));
    factory.applyRuntimeValue(terminal,def,QVariantMap{{QStringLiteral("append"),QVariantMap{
        {QStringLiteral("transport"),QStringLiteral("TCP")},{QStringLiteral("direction"),QStringLiteral("RX")},
        {QStringLiteral("data"),QVariantList{999}}}}});
    CHECK(terminal->receivedBytes()==8); // invalid bytes never wrapped
    clear->click();
    CHECK(terminal->contents().isEmpty());
    CHECK(terminal->receivedBytes()==0 && terminal->transmittedBytes()==0);
    qInfo()<<"PASS: native Communications Terminal widget, text/hex/binary events, limits, clearing";
    return 0;
}
