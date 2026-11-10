#include "EducationWidgets.h"
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QColor>
#include <QtGlobal>
#include <algorithm>
#include <cstring>
#include <utility>

namespace {
constexpr int kMaxSourceFileBytes = 16 * 1024 * 1024;
constexpr qint64 kMaxPixels = 48LL * 1024 * 1024;
constexpr int kMaxDimension = 2048;

bool isLight(const QWidget* widget) {
    for (const QWidget* p=widget; p; p=p->parentWidget())
        if (p->property("wfPanelLight").isValid())return p->property("wfPanelLight").toBool();
    return widget->palette().color(QPalette::Window).lightness()>=130;
}
}

// .wfpanel image schema contract (unchanged since 5.6.69):
//   properties.imageData = Base64 of PNG or JPEG binary;
//   properties.imageName = optional original filename.
// In particular, do not re-encode compliant JPEGs as PNG. QImage's
// hasAlphaChannel() is a pixel-FORMAT property, not proof that a source JPEG
// has transparent pixels. Re-encoding photographs losslessly may inflate them
// by an order of magnitude. Previously saved PNGs are left entirely intact.
QString EmbeddedImage::importFile(const QString& filePath,QString* error) {
    QFileInfo file(filePath);
    if(!file.isFile() || file.size()>kMaxSourceFileBytes || file.size()==0) {
        if(error)*error=QStringLiteral("Select an image file of 16 MiB or less.");
        return {};
    }
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    const QSize sourceSize=reader.size();
    if(!sourceSize.isValid() || sourceSize.width()>20000 || sourceSize.height()>20000 ||
       qint64(sourceSize.width())*sourceSize.height()>kMaxPixels) {
        if(error)*error=QStringLiteral("Image dimensions are too large or unsupported.");
        return {};
    }
    const QByteArray sourceFormat=reader.format().toLower();
    const bool originalJpeg=sourceFormat=="jpeg" || sourceFormat=="jpg";
    const bool originalPng=sourceFormat=="png";
    const bool needsScale=sourceSize.width()>kMaxDimension || sourceSize.height()>kMaxDimension;
    if(needsScale)
        reader.setScaledSize(sourceSize.scaled(kMaxDimension,kMaxDimension,Qt::KeepAspectRatio));
    QImage image=reader.read();
    if(image.isNull()) {
        if(error)*error=QStringLiteral("Unable to decode this image: %1").arg(reader.errorString());
        return {};
    }
    // Preserve the actual compressed file, not QImage's in-memory pixel format.
    // The same auto-orientation is used when displaying the embedded bytes.
    // This avoids JPEG -> PNG inflation and avoids a second lossy JPEG pass.
    if(!needsScale && (originalJpeg || originalPng) && file.size()<=MaxImageBytes) {
        QFile input(filePath);
        if(input.open(QIODevice::ReadOnly)) {
            const QByteArray original=input.readAll();
            if(!original.isEmpty() && original.size()<=MaxImageBytes)
                return QString::fromLatin1(original.toBase64());
        }
        if(error)*error=QStringLiteral("Unable to read the original image bytes.");
        return {};
    }
    if(image.width()>kMaxDimension || image.height()>kMaxDimension)
        image=image.scaled(kMaxDimension,kMaxDimension,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    // Resized JPEG stays JPEG. PNG and other graphics use lossless PNG first.
    // Only after a too-large opaque PNG is JPEG allowed as a fallback.
    const bool requiresTransparency=!originalJpeg && image.hasAlphaChannel();
    const char* format=originalJpeg ? "JPEG" : "PNG";
    int quality=85;
    for(int attempt=0;attempt<7;++attempt) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if(!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer,format,quality))break;
        if(bytes.size()<=MaxImageBytes)return QString::fromLatin1(bytes.toBase64());
        if(!requiresTransparency && std::strcmp(format,"PNG")==0) {
            // An opaque image may be compressed as JPEG instead of rejecting it.
            format="JPEG";
            continue;
        }
        image=image.scaled(qMax(160,int(image.width()*0.75)),qMax(160,int(image.height()*0.75)),
                           Qt::KeepAspectRatio,Qt::SmoothTransformation);
    }
    if(error)*error=QStringLiteral("The embedded image exceeds 2 MiB after resizing. Use a smaller picture.");
    return {};
}
QImage EmbeddedImage::decode(const QString& base64) {
    if(base64.isEmpty() || base64.size()>MaxPropertyBytes)return {};
    const QByteArray bytes=QByteArray::fromBase64(base64.toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(bytes.isEmpty() || bytes.size()>MaxImageBytes)return {};
    QImageReader reader;
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    reader.setDevice(&buffer);
    reader.setAutoTransform(true); // Preserve JPEG EXIF orientation in the original file.
    const QSize size=reader.size();
    if(!size.isValid() || qint64(size.width())*size.height()>kMaxPixels)return {};
    return reader.read();
}
QString EmbeddedImage::storageDescription(const QString& base64) {
    if(base64.isEmpty())return QStringLiteral("No image imported");
    const QByteArray bytes=QByteArray::fromBase64(base64.toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(bytes.isEmpty())return QStringLiteral("Invalid embedded picture");
    const QByteArray pngMagic=QByteArray::fromHex("89504e470d0a1a0a");
    const QString format=bytes.startsWith(pngMagic) ? QStringLiteral("PNG") :
                         bytes.startsWith(QByteArray::fromHex("ffd8ff")) ? QStringLiteral("JPEG") :
                         QStringLiteral("Unknown format");
    return QStringLiteral("%1 — %2 KiB binary / %3 KiB Base64")
        .arg(format)
        .arg(QString::number(double(bytes.size())/1024.0,'f',1))
        .arg(QString::number(double(base64.size())/1024.0,'f',1));
}

PanelImageWidget::PanelImageWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("panelImageWidget"));
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
}
void PanelImageWidget::configure(const PanelControl& definition) {
    const QString data=definition.properties.value(QStringLiteral("imageData")).toString();
    m_image=EmbeddedImage::decode(data);
    m_caption=definition.text;
    m_mode=definition.properties.value(QStringLiteral("scaleMode")).toString(QStringLiteral("Fit"));
    m_captionVisible=definition.properties.value(QStringLiteral("captionVisible")).toBool(false);
    m_border=definition.properties.value(QStringLiteral("borderVisible")).toBool(true);
    setToolTip(m_image.isNull()?QStringLiteral("Choose an image with Properties > Import Image..."):m_caption);
    update();
}
void PanelImageWidget::setRuntimeCaption(const QString& caption) { m_caption=caption;update(); }
void PanelImageWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform,true);
    p.setRenderHint(QPainter::Antialiasing,true);
    const bool light=isLight(this);
    const QColor surface=light?QColor("#f5f8fb"):QColor("#152536");
    const QColor outline=light?QColor("#aabccc"):QColor("#496278");
    const QColor text=light?QColor("#203547"):QColor("#e0edf8");
    p.fillRect(rect(),surface);
    QRect imageRect=rect().adjusted(4,4,-4,-4);
    if(m_captionVisible)imageRect.setBottom(qMax(imageRect.top()+1,imageRect.bottom()-26));
    if(!m_image.isNull() && imageRect.width()>0 && imageRect.height()>0) {
        if(m_mode==QStringLiteral("Stretch"))p.drawImage(imageRect,m_image);
        else if(m_mode==QStringLiteral("Fill")) {
            const QSizeF display=m_image.size().scaled(imageRect.size(),Qt::KeepAspectRatioByExpanding);
            const QRectF dst(imageRect.center().x()-display.width()/2.0,
                             imageRect.center().y()-display.height()/2.0,
                             display.width(),display.height());
            p.save();p.setClipRect(imageRect);p.drawImage(dst,m_image,QRectF(m_image.rect()));p.restore();
        } else {
            const QSize scaled=m_image.size().scaled(imageRect.size(),Qt::KeepAspectRatio);
            const QRect target(QPoint(imageRect.center().x()-scaled.width()/2,
                                      imageRect.center().y()-scaled.height()/2),scaled);
            p.drawImage(target,m_image);
        }
    } else {
        p.setPen(text);
        p.drawText(imageRect,Qt::AlignCenter|Qt::TextWordWrap,
                   QStringLiteral("IMAGE / DIAGRAM\nSelect widget → Properties → Import Image..."));
    }
    if(m_captionVisible) {
        p.setPen(text);
        QFont font=p.font();font.setPointSize(qMax(8,font.pointSize()-1));p.setFont(font);
        p.drawText(QRect(7,height()-27,width()-14,22),Qt::AlignCenter|Qt::TextSingleLine,
                   p.fontMetrics().elidedText(m_caption,Qt::ElideRight,width()-20));
    }
    if(m_border) {p.setPen(QPen(outline,1));p.setBrush(Qt::NoBrush);p.drawRect(rect().adjusted(0,0,-1,-1));}
}

PanelInstructionWidget::PanelInstructionWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("panelInstructionWidget"));
}
void PanelInstructionWidget::configure(const PanelControl& d) {
    m_title=d.text;
    m_body=d.properties.value(QStringLiteral("body")).toString();
    m_tone=d.properties.value(QStringLiteral("tone")).toString(QStringLiteral("Information"));
    update();
}
void PanelInstructionWidget::setRuntimeText(const QString& s) { m_body=s;update(); }
void PanelInstructionWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    const bool light=isLight(this);
    QColor accent("#36b5cf");
    if(m_tone==QStringLiteral("Warning"))accent=QColor("#e2a849");
    else if(m_tone==QStringLiteral("Success"))accent=QColor("#35bb87");
    else if(m_tone==QStringLiteral("Procedure"))accent=QColor("#8e9dea");
    const QColor background=light?QColor("#ffffff"):QColor("#192e42");
    const QColor foreground=light?QColor("#203547"):QColor("#ecf5fb");
    p.setPen(QPen(light?QColor("#b5c8d7"):QColor("#4a627c"),1));
    p.setBrush(background);p.drawRoundedRect(rect().adjusted(1,1,-2,-2),8,8);
    p.fillRect(QRect(3,7,4,qMax(0,height()-14)),accent);
    p.setPen(foreground);
    QFont bold=p.font();bold.setBold(true);bold.setPointSize(qMax(9,bold.pointSize()+1));p.setFont(bold);
    const int headingHeight=qMin(45,qMax(25,height()/3));
    p.drawText(QRect(16,8,qMax(1,width()-30),headingHeight),Qt::AlignVCenter|Qt::TextSingleLine,
               p.fontMetrics().elidedText(m_title,Qt::ElideRight,qMax(1,width()-35)));
    QFont body=p.font();body.setBold(false);body.setPointSize(qMax(8,body.pointSize()-1));p.setFont(body);
    p.drawText(QRect(17,10+headingHeight,qMax(1,width()-34),qMax(0,height()-headingHeight-19)),
               Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,m_body);
}

PanelControl PanelImageFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);
    d.text=QStringLiteral("Circuit diagram");d.geometry.setSize(QSize(400,260));
    d.properties=QJsonObject{{"scaleMode","Fit"},{"captionVisible",false},{"borderVisible",true}};
    return d;
}
QVector<WidgetProperty> PanelImageFactory::properties() const {
    WidgetProperty scale{"scaleMode","Image Scaling","choice",true};
    scale.choices={"Fit","Fill","Stretch"};scale.defaultValue="Fit";scale.category="Image";
    WidgetProperty caption{"captionVisible","Show Caption","boolean",true};
    caption.defaultValue=false;caption.category="Image";
    WidgetProperty border{"borderVisible","Show Border","boolean",true};
    border.defaultValue=true;border.category="Appearance";
    WidgetProperty title{"text","Caption / Alt Text","string",true};title.category="Image";
    return {title,scale,caption,border};
}
QWidget* PanelImageFactory::create(const PanelControl& d,QWidget* parent,EventSink sink) const {
    Q_UNUSED(sink)
    auto* widget=new PanelImageWidget(parent);widget->configure(d);return widget;
}
void PanelImageFactory::applyDefinition(QWidget* w,const PanelControl& d) const {
    if(auto* img=dynamic_cast<PanelImageWidget*>(w))img->configure(d);
}
void PanelImageFactory::applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const {
    if(auto* img=dynamic_cast<PanelImageWidget*>(w))img->setRuntimeCaption(v.toString());
}
PanelControl PanelInstructionFactory::defaultDefinition(const QString& id) const {
    auto d=WidgetFactory::defaultDefinition(id);
    d.text=QStringLiteral("Experiment instructions");d.geometry.setSize(QSize(370,150));
    d.properties=QJsonObject{{"tone","Information"},
        {"body","Describe the circuit setup, the expected observation, or a safety reminder here."}};
    return d;
}
QVector<WidgetProperty> PanelInstructionFactory::properties() const {
    WidgetProperty title{"text","Heading","string",true};title.category="Content";
    WidgetProperty body{"body","Instructions","multiline",true};body.category="Content";
    WidgetProperty tone{"tone","Card Style","choice",true};
    tone.choices={"Information","Procedure","Warning","Success"};
    tone.defaultValue="Information";tone.category="Appearance";
    return {title,body,tone};
}
QWidget* PanelInstructionFactory::create(const PanelControl& d,QWidget* parent,EventSink sink) const {
    Q_UNUSED(sink)
    auto* widget=new PanelInstructionWidget(parent);widget->configure(d);return widget;
}
void PanelInstructionFactory::applyDefinition(QWidget* w,const PanelControl& d) const {
    if(auto* card=dynamic_cast<PanelInstructionWidget*>(w))card->configure(d);
}
void PanelInstructionFactory::applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const {
    if(auto* card=dynamic_cast<PanelInstructionWidget*>(w))card->setRuntimeText(v.toString());
}
