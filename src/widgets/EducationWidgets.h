#pragma once
#include "WidgetFactory.h"
#include <QImage>
#include <QWidget>
#include <QString>

// Images are embedded as Base64-encoded PNG/JPEG data, never as filesystem URLs.
// Project files remain portable even when source pictures are moved or deleted.
namespace EmbeddedImage {
constexpr int MaxImageBytes = 2 * 1024 * 1024;
constexpr int MaxPropertyBytes = 3 * 1024 * 1024;
QString importFile(const QString& filePath, QString* error = nullptr);
// Display the actual stored format and both binary/Base64 sizes in Properties.
QString storageDescription(const QString& base64);
QImage decode(const QString& base64);
}

class PanelImageWidget final : public QWidget {
public:
    explicit PanelImageWidget(QWidget* parent=nullptr);
    void configure(const PanelControl& definition);
    void setRuntimeCaption(const QString& caption);
    QSize sizeHint() const override { return QSize(380,250); }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QImage m_image;
    QString m_caption;
    QString m_mode=QStringLiteral("Fit");
    bool m_captionVisible=false;
    bool m_border=true;
};

class PanelInstructionWidget final : public QWidget {
public:
    explicit PanelInstructionWidget(QWidget* parent=nullptr);
    void configure(const PanelControl& definition);
    void setRuntimeText(const QString& body);
    QSize sizeHint() const override { return QSize(360,150); }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QString m_title,m_body,m_tone;
};

class PanelImageFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("image");}
    QString displayName() const override {return QStringLiteral("Image / Diagram");}
    PanelControl defaultDefinition(const QString& id) const override;
    QVector<WidgetProperty> properties() const override;
    QVariant initialRuntimeValue(const PanelControl& d) const override {return d.text;}
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget* w,const PanelControl& d) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override;
};
class PanelInstructionFactory final : public WidgetFactory {
public:
    QString typeId() const override {return QStringLiteral("instruction_card");}
    QString displayName() const override {return QStringLiteral("Instruction Card");}
    PanelControl defaultDefinition(const QString& id) const override;
    QVector<WidgetProperty> properties() const override;
    QVariant initialRuntimeValue(const PanelControl& d) const override {
        return d.properties.value("body").toString();
    }
    QWidget* create(const PanelControl& d,QWidget* parent,EventSink sink) const override;
    void applyDefinition(QWidget* w,const PanelControl& d) const override;
    void readDefinition(QWidget*,PanelControl&) const override {}
    void applyRuntimeValue(QWidget* w,const PanelControl&,const QVariant& v) const override;
};
