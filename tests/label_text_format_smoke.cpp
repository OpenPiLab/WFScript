// v5.6.52 — native Qt offscreen Label editor + display + JSON test, no AD3.
#include "widgets/PanelLabelWidget.h"
#include "widgets/WidgetPropertyInspector.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include "widgets/BuiltinWidgets.h"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QComboBox>
#include <QCheckBox>
#include <QEventLoop>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QDebug>
#include <memory>

#define CHECK(x) do{if(!(x)){qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1;}}while(false)

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    auto* factory=WidgetRegistry::instance().factory(QStringLiteral("label"));
    CHECK(factory);
    PanelControl old=factory->defaultDefinition("legacyLabel");
    old.text="Legacy <b>auto</b>\nsecond line";
    old.geometry=QRect(10,10,300,90);
    QWidget parent;
    QWidget* oldW=factory->create(old,&parent,[](const QString&,const QVariant&){});
    auto* oldLabel=dynamic_cast<PanelLabelWidget*>(oldW);
    CHECK(oldLabel && oldLabel->textFormat()==Qt::AutoText && !oldLabel->wordWrap());
    CHECK(oldLabel->text()==old.text);
    WidgetPropertyInspector editor;
    editor.setControl(old,true);
    auto* source=editor.findChild<QPlainTextEdit*>("panelPropertyText");
    auto* format=editor.findChild<QComboBox*>("panelProperty_textFormat");
    auto* wrap=editor.findChild<QCheckBox*>("panelProperty_wordWrap");
    CHECK(source && format && wrap);
    CHECK(source->toPlainText()==old.text);
    CHECK(format->currentText()=="Automatic" && !wrap->isChecked());
    const QString html=QStringLiteral("<b>PASS</b><br><span style=\"color:#36bd83\">3.300 V</span>");
    source->setPlainText(html);
    format->setCurrentText(QStringLiteral("Rich Text"));
    wrap->setChecked(true);
    PanelControl edited=editor.readControl(old);
    CHECK(edited.text==html);
    CHECK(edited.properties.value("textFormat").toString()=="Rich Text");
    CHECK(edited.properties.value("wordWrap").toBool());
    QWidget* richW=factory->create(edited,&parent,[](const QString&,const QVariant&){});
    auto* richLabel=dynamic_cast<PanelLabelWidget*>(richW);
    CHECK(richLabel && richLabel->textFormat()==Qt::RichText && richLabel->wordWrap());
    CHECK(richLabel->text()==html);
    factory->applyRuntimeValue(richLabel,edited,QStringLiteral("<i>RUNNING</i><br>Line two"));
    CHECK(richLabel->textFormat()==Qt::RichText);
    CHECK(richLabel->text()==QStringLiteral("<i>RUNNING</i><br>Line two"));
    const QJsonObject encoded=WidgetDocumentCodec::encode(edited);
    PanelControl decoded;
    QString error;
    CHECK(WidgetDocumentCodec::decode(encoded,&decoded,&error));
    CHECK(decoded.text==html && decoded.properties==edited.properties);
    decoded.properties.insert("textFormat",QStringLiteral("bad format"));
    CHECK(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(decoded),&decoded,&error));
    // Explicit Plain Text must keep markup literal. Newline edit value survives type reassign.
    editor.setControl(old,true);
    source->setPlainText(QStringLiteral("First line\nSecond <b>literal</b> line"));
    format->setCurrentText(QStringLiteral("Plain Text"));
    wrap->setChecked(false);
    const PanelControl plain=editor.readControl(old);
    CHECK(plain.text.contains('\n'));
    QWidget* plainW=factory->create(plain,&parent,[](const QString&,const QVariant&){});
    auto* plainLabel=dynamic_cast<PanelLabelWidget*>(plainW);
    CHECK(plainLabel && plainLabel->textFormat()==Qt::PlainText && !plainLabel->wordWrap());
    CHECK(plainLabel->text()==plain.text);
    editor.setControl(plain,true);
    CHECK(source->toPlainText()==plain.text);
    // Regression: every control on the Qt-embedded Test Station must satisfy
    // the *production* widget schema, not just generic JSON parsing. This
    // failed in v5.6.63 because the six Overview labels used "rich" instead
    // of the factory's exact "Rich Text" choice, so the example could not load.
    QFile embedded(QStringLiteral(":/wfscript/examples/AutomatedTestStation.wfpanel"));
    CHECK(embedded.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument example = QJsonDocument::fromJson(embedded.readAll(), &parseError);
    CHECK(parseError.error == QJsonParseError::NoError && example.isObject());
    const QJsonArray pages = example.object().value(QStringLiteral("pages")).toArray();
    // The station gained Page 06 Shared UUT View in v5.6.65.
    CHECK(pages.size() == 7);
    bool foundSharedUut = false;
    for (const auto& pageValue : pages)
        foundSharedUut |= pageValue.toObject().value(QStringLiteral("id")).toString()
                          == QStringLiteral("sharedUut");
    CHECK(foundSharedUut);
    int checkedControls = 0;
    for(const QJsonValue& pageValue : pages) {
        const QJsonObject page = pageValue.toObject();
        QVector<PanelControl> controls;
        QString decodeError;
        const bool valid = WidgetDocumentCodec::decodeList(
            page.value(QStringLiteral("controls")).toArray(), &controls, &decodeError);
        if(!valid) qCritical() << "Bundled Test Station Page"
                              << page.value(QStringLiteral("name")).toString()
                              << "invalid:" << decodeError;
        CHECK(valid);
        checkedControls += controls.size();
    }
    CHECK(checkedControls > 20);
    qInfo()<<"PASS: bundled AutomatedTestStation.wfpanel validates all"
           << checkedControls << "controls against the real widget registry";
    qInfo()<<"PASS v5.6.52 multiline source editor, auto/plain/rich, wrap, ui.set, JSON roundtrip";
    return 0;
}
