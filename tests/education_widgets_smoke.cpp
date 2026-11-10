// v5.6.69.1 native offscreen smoke: import, encode, image rendering, property
// inspector, image and instruction widget preservation across Canvas pages.
#include "PanelCanvas.h"
#include "widgets/EducationWidgets.h"
#include "widgets/WidgetPropertyInspector.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QImageReader>
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#define CHECK(x) do {if(!(x)){qCritical()<<"FAIL"<<#x<<"line"<<__LINE__;return 1;}}while(false)
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString filename=dir.path()+"/schematic.png";
    QImage picture(256,100,QImage::Format_ARGB32);
    picture.fill(Qt::transparent);
    picture.setPixelColor(24,24,Qt::red);
    CHECK(picture.save(filename));
    QString error;
    const QString data=EmbeddedImage::importFile(filename,&error);
    CHECK(!data.isEmpty());
    CHECK(!EmbeddedImage::decode(data).isNull());
    // A standards-compliant PNG is stored byte-for-byte and stays lossless.
    QFile pngFile(filename);
    CHECK(pngFile.open(QIODevice::ReadOnly));
    const QByteArray sourcePng=pngFile.readAll();
    CHECK(QByteArray::fromBase64(data.toLatin1())==sourcePng);
    CHECK(EmbeddedImage::storageDescription(data).contains("PNG"));
    // Regression: a compressed JPEG must not be expanded to PNG. Importing
    // it must preserve the exact JPEG bytes and stay within 4/3 overhead.
    const QString photoName=dir.path()+"/board_photo.jpg";
    QImage photo(640,420,QImage::Format_RGB32);
    for(int y=0;y<photo.height();++y)
        for(int x=0;x<photo.width();++x)
            photo.setPixelColor(x,y,QColor((x*7+y*3)%256,(x*2+y*11)%256,(x*5+y)%256));
    CHECK(photo.save(photoName,"JPEG",78));
    QFile jpegFile(photoName);
    CHECK(jpegFile.open(QIODevice::ReadOnly));
    const QByteArray sourceJpeg=jpegFile.readAll();
    CHECK(sourceJpeg.size()>1000 && sourceJpeg.size()<EmbeddedImage::MaxImageBytes);
    const QString jpegData=EmbeddedImage::importFile(photoName,&error);
    CHECK(!jpegData.isEmpty());
    CHECK(QByteArray::fromBase64(jpegData.toLatin1())==sourceJpeg);
    CHECK(jpegData.size()<=sourceJpeg.size()*4/3+4);
    CHECK(EmbeddedImage::storageDescription(jpegData).contains("JPEG"));
    CHECK(!EmbeddedImage::decode(jpegData).isNull());
    // A previously saved image is never silently migrated on decode/encode.
    // The preserved v5.6.69 PNG data and the new JPEG are both valid.
    auto jpegControl=WidgetRegistry::instance().factory("image")->defaultDefinition("photo");
    jpegControl.properties.insert("imageData",jpegData);
    PanelControl jpegRoundtrip;
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(jpegControl),&jpegRoundtrip,&error));
    CHECK(jpegRoundtrip.properties.value("imageData").toString()==jpegData);
    CHECK(EmbeddedImage::decode(QStringLiteral("not valid base64 !!")).isNull());
    CHECK(WidgetRegistry::instance().factory("image"));
    CHECK(WidgetRegistry::instance().factory("instruction_card"));
    auto d=WidgetRegistry::instance().factory("image")->defaultDefinition("schematic");
    d.properties.insert("imageData",data);
    d.properties.insert("imageName","schematic.png");
    d.properties.insert("captionVisible",true);
    PanelControl decoded;
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(d),&decoded,&error));
    CHECK(decoded.properties.value("imageData").toString()==data);
    auto wrong=WidgetDocumentCodec::encode(d);
    auto props=wrong.value("properties").toObject();
    props.insert("imageData","not valid base64 !!");
    wrong.insert("properties",props);
    CHECK(!WidgetDocumentCodec::decode(wrong,&decoded,&error));
    WidgetPropertyInspector inspector;
    inspector.setControl(d,true);
    CHECK(inspector.findChild<QPushButton*>("imageImportButton"));
    CHECK(inspector.findChild<QLabel*>("imageImportStatus"));
    CHECK(inspector.readControl(d).properties.value("imageData").toString()==data);
    auto card=WidgetRegistry::instance().factory("instruction_card")->defaultDefinition("card");
    card.properties.insert("body","A two-line\nprocedure");
    CHECK(WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(card),&decoded,&error));
    PanelCanvas canvas;
    canvas.setDocumentSize(QSize(1100,700));
    CHECK(canvas.addControl(d));
    CHECK(canvas.addControl(card));
    CHECK(dynamic_cast<PanelImageWidget*>(canvas.findChild<QWidget*>("schematic")));
    CHECK(dynamic_cast<PanelInstructionWidget*>(canvas.findChild<QWidget*>("card")));
    const auto before=canvas.toJson();
    CHECK(before.size()==2);
    CHECK(canvas.fromJson(before,&error));
    CHECK(canvas.toJson()==before);
    // A Page switch must not strip embedded data or reconstruct old widgets.
    CHECK(canvas.activatePage("main",before,QSize(1100,700),&error));
    const auto* original=dynamic_cast<PanelImageWidget*>(canvas.findChild<QWidget*>("schematic"));
    CHECK(original);
    CHECK(canvas.activatePage("next",QJsonArray{},QSize(1100,700),&error));
    CHECK(canvas.activatePage("main",before,QSize(1100,700),&error));
    CHECK(dynamic_cast<PanelImageWidget*>(canvas.findChild<QWidget*>("schematic"))==original);
    CHECK(canvas.toJson().at(0).toObject().value("properties").toObject().value("imageData").toString()==data);
    qInfo()<<"PASS: JPEG/PNG byte-preserving import, Base64, validation, inspector, canvas/Page persistence and Instruction Card";
    return 0;
}
