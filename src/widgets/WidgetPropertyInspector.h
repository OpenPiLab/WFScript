#pragma once
#include "WidgetDefinition.h"
#include "WidgetFactory.h"
#include <QHash>
#include <QWidget>

class QLineEdit;
class QSpinBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

// Metadata-driven property UI. Shared geometry/ID fields are fixed; every
// type-specific editor is generated from WidgetFactory::properties().
// A widget type is rebuilt only when the *selected type* changes, never on
// page switches or runtime data updates.
class WidgetPropertyInspector final : public QWidget {
    Q_OBJECT
public:
    explicit WidgetPropertyInspector(QWidget* parent=nullptr);
    void setControl(const PanelControl& control,bool editable);
    void setEditable(bool editable);
    PanelControl readControl(const PanelControl& original) const;
    QString displayedType() const {return m_type;}
    QWidget* editor(const QString& propertyId) const {return m_fields.value(propertyId,nullptr);}
signals:
    void edited();
private:
    void rebuild(const QString& type);
    void refreshEquationPreview();
    void connectEditor(QWidget* editor,const QString& kind);
    void assign(QWidget* editor,const QString& kind,const QVariant& value);
    QVariant valueOf(QWidget* editor,const QString& kind) const;
    QLineEdit* m_id=nullptr;
    QSpinBox *m_x=nullptr,*m_y=nullptr,*m_width=nullptr,*m_height=nullptr;
    QWidget* m_dynamic=nullptr;
    QVBoxLayout* m_layout=nullptr;
    QHash<QString,QWidget*> m_fields;
    QVector<WidgetProperty> m_metadata;
    // Image data is intentionally hidden from text editors: binary data is
    // imported through a file chooser and saved in imageData as Base64.
    QString m_imageData;
    QString m_imageName;
    QLabel* m_imageStatus=nullptr;
    QPushButton* m_imageImport=nullptr;
    QPushButton* m_imageClear=nullptr;
    QString m_type;
    bool m_changing=false;
    bool m_editable=false;
};
