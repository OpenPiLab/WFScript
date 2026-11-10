#include "WidgetPropertyInspector.h"
#include "WidgetRegistry.h"
#include "EducationWidgets.h"
#include "ExperimentWidgets.h"
#include <QPixmap>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QFocusEvent>
#include <QTimer>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QToolButton>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QtGlobal>
#include <cmath>
#include <functional>

namespace {
// Multiline source editor: Enter inserts a newline. Commit after a short pause,
// or immediately on focus loss, without replacing the cursor during a refresh.
// A QPlainTextEdit is intentional: Rich Text labels edit HTML *source* here,
// not an implicit WYSIWYG document that changes the saved markup.
class MultilinePropertyEditor final : public QPlainTextEdit {
public:
    explicit MultilinePropertyEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent) {
        setPlaceholderText(QStringLiteral("Plain text (Enter for new line) or Qt HTML source"));
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
        setFixedHeight(112);
        m_commitTimer.setSingleShot(true);
        connect(this, &QPlainTextEdit::textChanged, this, [this] {
            if(!m_assigning)m_commitTimer.start(350);
        });
        connect(&m_commitTimer, &QTimer::timeout, this, [this] {
            if(onCommit)onCommit();
        });
    }
    void assignText(const QString& source) {
        m_commitTimer.stop();
        if(toPlainText()==source)return; // Do not disturb cursor/selection during property refresh.
        m_assigning=true;
        setPlainText(source);
        m_assigning=false;
    }
    std::function<void()> onCommit;
protected:
    void focusOutEvent(QFocusEvent* event) override {
        QPlainTextEdit::focusOutEvent(event);
        if(m_commitTimer.isActive()) {
            m_commitTimer.stop();
            if(onCommit)onCommit();
        }
    }
private:
    QTimer m_commitTimer;
    bool m_assigning=false;
};
QSpinBox* geometryField(int minimum,int maximum,const QString& name,QWidget* parent) {
    auto* field=new QSpinBox(parent);
    field->setRange(minimum,maximum);
    field->setKeyboardTracking(false);
    field->setObjectName(name);
    return field;
}
QString objectNameFor(const QString& id) {
    if(id=="text")return "panelPropertyText";
    if(id=="value")return "panelPropertyValue";
    if(id=="min")return "panelPropertyMinimum";
    if(id=="max")return "panelPropertyMaximum";
    return QStringLiteral("panelProperty_%1").arg(id);
}
}
WidgetPropertyInspector::WidgetPropertyInspector(QWidget* parent):QWidget(parent) {
    m_layout=new QVBoxLayout(this);
    m_layout->setContentsMargins(0,0,0,0);
    m_layout->setSpacing(5);
    m_layout->setSizeConstraint(QLayout::SetMinimumSize);
    auto* common=new QFormLayout;
    m_id=new QLineEdit(this);
    m_id->setObjectName("panelPropertyId");
    m_x=geometryField(-1000,8192,"panelPropertyX",this);
    m_y=geometryField(-1000,8192,"panelPropertyY",this);
    m_width=geometryField(25,3000,"panelPropertyWidth",this);
    m_height=geometryField(22,3000,"panelPropertyHeight",this);
    common->addRow("ID",m_id);
    common->addRow("X",m_x);
    common->addRow("Y",m_y);
    common->addRow("Width",m_width);
    common->addRow("Height",m_height);
    m_layout->addLayout(common);
    connectEditor(m_id,"string");
    for(auto* w:{m_x,m_y,m_width,m_height})connectEditor(w,"integer");
    setEditable(false);
}
void WidgetPropertyInspector::connectEditor(QWidget* editor,const QString& kind) {
    if(auto* e=qobject_cast<QLineEdit*>(editor))
        connect(e,&QLineEdit::editingFinished,this,[this]{if(!m_changing)emit edited();});
    else if(auto* e=dynamic_cast<MultilinePropertyEditor*>(editor))
        e->onCommit=[this]{if(!m_changing)emit edited();};
    else if(auto* e=qobject_cast<QDoubleSpinBox*>(editor))
        connect(e,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,
                [this](double){if(!m_changing)emit edited();});
    else if(auto* e=qobject_cast<QSpinBox*>(editor))
        connect(e,QOverload<int>::of(&QSpinBox::valueChanged),this,
                [this](int){if(!m_changing)emit edited();});
    else if(auto* e=qobject_cast<QCheckBox*>(editor))
        connect(e,&QCheckBox::toggled,this,[this](bool){if(!m_changing)emit edited();});
    else if(auto* e=qobject_cast<QComboBox*>(editor))
        connect(e,QOverload<int>::of(&QComboBox::currentIndexChanged),this,
                [this](int){if(!m_changing)emit edited();});
    Q_UNUSED(kind)
}
void WidgetPropertyInspector::rebuild(const QString& type) {
    if(m_type==type)return;
    m_type=type;
    m_fields.clear();
    m_metadata.clear();
    m_imageStatus=nullptr;
    m_imageImport=nullptr;
    m_imageClear=nullptr;
    if(m_dynamic) {m_layout->removeWidget(m_dynamic);delete m_dynamic;m_dynamic=nullptr;}
    if(type.isEmpty())return;
    m_dynamic=new QWidget(this);
    auto* sections=new QVBoxLayout(m_dynamic);
    sections->setContentsMargins(0,0,0,0);
    sections->setSpacing(4);
    sections->setSizeConstraint(QLayout::SetMinimumSize);
    QHash<QString,QFormLayout*> forms;
    auto ensureForm=[this,sections,&forms](const QString& title)->QFormLayout* {
        if(forms.contains(title))return forms.value(title);
        auto* group=new QWidget(m_dynamic);
        auto* groupLayout=new QVBoxLayout(group);
        groupLayout->setContentsMargins(0,0,0,0);
        groupLayout->setSpacing(1);
        auto* header=new QToolButton(group);
        header->setText(title);
        header->setObjectName(QStringLiteral("panelPropertyCategory_%1").arg(title).replace(' ','_'));
        header->setCheckable(true);
        header->setChecked(true);
        header->setArrowType(Qt::DownArrow);
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        groupLayout->addWidget(header);
        auto* body=new QWidget(group);
        auto* form=new QFormLayout(body);
        form->setContentsMargins(7,3,1,7);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setSizeConstraint(QLayout::SetMinimumSize);
        groupLayout->addWidget(body);
        connect(header,&QToolButton::toggled,this,[header,body](bool expanded){
            body->setVisible(expanded);
            header->setArrowType(expanded?Qt::DownArrow:Qt::RightArrow);
        });
        forms.insert(title,form);
        sections->addWidget(group);
        return form;
    };
    if(const auto* factory=WidgetRegistry::instance().factory(type))
        m_metadata=factory->properties();
    for(const WidgetProperty& prop:m_metadata) {
        // The legacy geometry field is already represented by four numeric
        // fields. id and type are never editable widget-specific properties.
        if(prop.id=="geometry" || prop.id=="id" || prop.id=="type" ||
           prop.id.isEmpty() || m_fields.contains(prop.id))continue;
        QWidget* e=nullptr;
        if(prop.kind=="string")e=new QLineEdit(m_dynamic);
        else if(prop.kind=="multiline")e=new MultilinePropertyEditor(m_dynamic);
        else if(prop.kind=="boolean")e=new QCheckBox(m_dynamic);
        else if(prop.kind=="integer") {
            auto* spin=new QSpinBox(m_dynamic);
            spin->setRange(static_cast<int>(qBound(-1000000000.0,prop.minimum,1000000000.0)),
                           static_cast<int>(qBound(-1000000000.0,prop.maximum,1000000000.0)));
            spin->setKeyboardTracking(false);
            e=spin;
        } else if(prop.kind=="double") {
            auto* spin=new QDoubleSpinBox(m_dynamic);
            spin->setRange(prop.minimum,prop.maximum);
            spin->setDecimals(qBound(0,prop.decimals,12));
            spin->setKeyboardTracking(false);
            e=spin;
        } else if(prop.kind=="choice") {
            auto* combo=new QComboBox(m_dynamic);
            combo->addItems(prop.choices);
            e=combo;
        }
        if(!e)continue; // unsupported kinds require explicit editor support
        e->setObjectName(objectNameFor(prop.id));
        const QString category=prop.category.isEmpty()?QStringLiteral("Widget Settings"):prop.category;
        auto* form=ensureForm(category);
        e->setMinimumHeight(23);
        if(prop.kind=="multiline") {
            if(prop.id==QStringLiteral("source") && type==QStringLiteral("equation")) {
                e->setToolTip(QStringLiteral("LaTeX-style subset: \\frac{top}{bottom}, \\sqrt{x}, x_{sub}, x^{power}, \\pi, \\omega, \\tau, \\cdot. No $ delimiters or external LaTeX installation."));
                if(auto* editor=dynamic_cast<MultilinePropertyEditor*>(e))
                    editor->setPlaceholderText(QStringLiteral("f_c=\\frac{1}{2\\pi RC}"));
            } else if(prop.id==QStringLiteral("body")) {
                e->setToolTip(QStringLiteral("Plain-text instructions; Enter inserts a new line."));
                if(auto* textEditor=dynamic_cast<MultilinePropertyEditor*>(e))
                    textEditor->setPlaceholderText(QStringLiteral("Write a procedure, explanation, warning or tip..."));
            } else e->setToolTip(QStringLiteral("Enter creates a newline. Rich Text uses Qt-supported HTML such as <b>, <i>, <br>, and <span style=...>."));
        }
        form->addRow(prop.label,e);
        m_fields.insert(prop.id,e);
        connectEditor(e,prop.kind);
    }
    if(type==QStringLiteral("equation")) {
        auto* preview=new QWidget(m_dynamic);
        auto* stack=new QVBoxLayout(preview);stack->setContentsMargins(7,4,7,6);
        auto* note=new QLabel(QStringLiteral("Live preview (supported TeX-style commands only):"),preview);
        stack->addWidget(note);
        auto* math=new QLabel(preview);math->setObjectName(QStringLiteral("equationLivePreview"));
        math->setMinimumHeight(82);
        math->setAlignment(Qt::AlignCenter);
        math->setStyleSheet(QStringLiteral("background:#192d41;color:white;border:1px solid #4b6a84;"));
        stack->addWidget(math);
        auto* quick=new QLabel(QStringLiteral("Examples: f_c=\\frac{1}{2\\pi RC}     \\tau=RC     H(j\\omega)=\\frac{1}{1+j\\omega RC}\nSupported: \\frac, \\sqrt, ^, _, \\alpha, \\beta, \\pi, \\omega, \\tau, \\cdot. No full LaTeX engine."),preview);
        quick->setWordWrap(true);stack->addWidget(quick);
        sections->addWidget(preview);
        auto* sourceEditor=dynamic_cast<MultilinePropertyEditor*>(m_fields.value(QStringLiteral("source"),nullptr));
        if(sourceEditor) {
            connect(sourceEditor,&QPlainTextEdit::textChanged,this,
                    [this] {refreshEquationPreview();});
            if(auto* fontSize=qobject_cast<QSpinBox*>(m_fields.value(QStringLiteral("pointSize"),nullptr)))
                connect(fontSize,QOverload<int>::of(&QSpinBox::valueChanged),this,
                        [this](int){refreshEquationPreview();});
            auto* insertBar=new QHBoxLayout;
            struct InsertOption {const char* label;const char* source;};
            const InsertOption options[]={{"Fraction","\\frac{a}{b}"},{"Subscript","x_{1}"},
                {"Square root","\\sqrt{x}"},{"Greek π","\\pi"}};
            for(const auto& option:options) {
                auto* button=new QPushButton(QString::fromUtf8(option.label),preview);
                button->setObjectName(QStringLiteral("equationInsert_%1").arg(QString::fromUtf8(option.label).replace(' ','_')));
                button->setToolTip(QStringLiteral("Insert into equation source at the caret"));
                insertBar->addWidget(button);
                const QString insertion=QString::fromLatin1(option.source);
                connect(button,&QPushButton::clicked,sourceEditor,[this,sourceEditor,insertion]{
                    if(!m_editable)return;
                    QTextCursor cursor=sourceEditor->textCursor();
                    cursor.insertText(insertion);
                    sourceEditor->setTextCursor(cursor);
                    sourceEditor->setFocus();
                });
            }
            stack->addLayout(insertBar);
        }
    }
    if(type==QStringLiteral("experiment_table")) {
        auto* note=new QLabel(QStringLiteral("Each | separates a column. Enter one initial data row per line. "
            "For live data call ui.set(tableId, [[f, predicted, measured, error], ...]). "
            "Right-click the table in Run mode to export CSV. Editable cells can fire valueChanged(rows)."),m_dynamic);
        note->setWordWrap(true);sections->addWidget(note);
    }
    if(type==QStringLiteral("image")) {
        auto* section=new QWidget(m_dynamic);
        auto* layout=new QVBoxLayout(section);
        layout->setContentsMargins(7,5,5,8);
        m_imageStatus=new QLabel(QStringLiteral("No image imported"),section);
        m_imageStatus->setWordWrap(true);
        m_imageStatus->setObjectName(QStringLiteral("imageImportStatus"));
        layout->addWidget(m_imageStatus);
        auto* buttons=new QHBoxLayout;
        m_imageImport=new QPushButton(QStringLiteral("Import Image..."),section);
        m_imageImport->setObjectName(QStringLiteral("imageImportButton"));
        m_imageClear=new QPushButton(QStringLiteral("Remove"),section);
        m_imageClear->setObjectName(QStringLiteral("imageRemoveButton"));
        buttons->addWidget(m_imageImport);
        buttons->addWidget(m_imageClear);
        layout->addLayout(buttons);
        auto* hint=new QLabel(QStringLiteral(
            "PNG/JPEG/BMP/WebP (when supported by Qt). PNG/JPEG images within the size limits "
            "are stored WITHOUT recompression. Oversized or other images may be converted "
            "to PNG/JPEG. The unchanged .wfpanel schema stores Base64 image bytes; "
            "the original file is not needed. Fit shows all; Fill crops; Stretch distorts."),section);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        sections->addWidget(section);
        connect(m_imageImport,&QPushButton::clicked,this,[this]{
            if(!m_editable)return;
            const QString file=QFileDialog::getOpenFileName(this,QStringLiteral("Embed picture in Panel"),QString(),
                        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*)"));
            if(file.isEmpty())return;
            QString error;
            const QString data=EmbeddedImage::importFile(file,&error);
            if(data.isEmpty()) {
                QMessageBox::warning(this,QStringLiteral("Image import"),error);
                return;
            }
            m_imageData=data;
            m_imageName=QFileInfo(file).fileName();
            m_imageStatus->setText(QStringLiteral("Embedded: %1\n%2")
                .arg(m_imageName,EmbeddedImage::storageDescription(m_imageData)));
            m_imageClear->setEnabled(true);
            if(!m_changing)emit edited();
        });
        connect(m_imageClear,&QPushButton::clicked,this,[this]{
            if(!m_editable)return;
            m_imageData.clear();m_imageName.clear();
            m_imageStatus->setText(QStringLiteral("No image imported"));
            m_imageClear->setEnabled(false);
            if(!m_changing)emit edited();
        });
    }
    if(type==QStringLiteral("runInformation")) {
        // This widget intentionally has no separate TestRunner binding or
        // duplicated field configuration. Explain the project-owned link in
        // the inspector where the designer will look for widget properties.
        auto* help=new QLabel(QStringLiteral(
            "AUTO-LINKED TO THIS PANEL'S TEST EXECUTIVE\n\n"
            "Define the fields with Test Sequence > Run Information...\n"
            "All Run Information widgets on all Pages share ONE pending value model. "
            "RUN / NEXT captures a frozen copy. No active-widget selection or JS binding. "
            "Read current input with TestRunner.pendingRunInfo(); "
            "read executed values with TestRunner.runInfo()."),m_dynamic);
        help->setObjectName(QStringLiteral("runInformationAssociationHelp"));
        help->setWordWrap(true);
        help->setContentsMargins(7,6,7,7);
        sections->addWidget(help);
    }
    sections->addStretch();
    m_layout->addWidget(m_dynamic);
}
void WidgetPropertyInspector::assign(QWidget* editor,const QString& kind,const QVariant& value) {
    if(!editor)return;
    const QSignalBlocker block(editor);
    if(auto* e=qobject_cast<QLineEdit*>(editor))e->setText(value.toString());
    else if(auto* e=dynamic_cast<MultilinePropertyEditor*>(editor))e->assignText(value.toString());
    else if(auto* e=qobject_cast<QDoubleSpinBox*>(editor))e->setValue(value.toDouble());
    else if(auto* e=qobject_cast<QSpinBox*>(editor))e->setValue(value.toInt());
    else if(auto* e=qobject_cast<QCheckBox*>(editor))e->setChecked(value.toBool());
    else if(auto* e=qobject_cast<QComboBox*>(editor)) {
        const int index=e->findText(value.toString());
        if(index>=0)e->setCurrentIndex(index);
    }
    Q_UNUSED(kind)
}
QVariant WidgetPropertyInspector::valueOf(QWidget* editor,const QString& kind) const {
    if(auto* e=qobject_cast<QLineEdit*>(editor))return e->text();
    if(auto* e=dynamic_cast<MultilinePropertyEditor*>(editor))return e->toPlainText();
    if(auto* e=qobject_cast<QDoubleSpinBox*>(editor))return e->value();
    if(auto* e=qobject_cast<QSpinBox*>(editor))return e->value();
    if(auto* e=qobject_cast<QCheckBox*>(editor))return e->isChecked();
    if(auto* e=qobject_cast<QComboBox*>(editor))return e->currentText();
    Q_UNUSED(kind)
    return {};
}
void WidgetPropertyInspector::setControl(const PanelControl& d,bool editable) {
    m_changing=true;
    rebuild(d.id.isEmpty()?QString():d.type);
    if(d.type==QStringLiteral("image")) {
        m_imageData=d.properties.value(QStringLiteral("imageData")).toString();
        m_imageName=d.properties.value(QStringLiteral("imageName")).toString();
        if(m_imageStatus) m_imageStatus->setText(m_imageData.isEmpty()
            ? QStringLiteral("No image imported")
            : QStringLiteral("Embedded: %1\n%2")
                  .arg(m_imageName.isEmpty()?QStringLiteral("Picture"):m_imageName,
                       EmbeddedImage::storageDescription(m_imageData)));
    } else {m_imageData.clear();m_imageName.clear();}
    assign(m_id,"string",d.id);
    assign(m_x,"integer",d.geometry.x());
    assign(m_y,"integer",d.geometry.y());
    assign(m_width,"integer",d.geometry.width());
    assign(m_height,"integer",d.geometry.height());
    for(const WidgetProperty& p:m_metadata) {
        QWidget* field=m_fields.value(p.id,nullptr);
        if(!field)continue;
        if(p.id=="text")assign(field,p.kind,d.text);
        else if(p.id=="value")assign(field,p.kind,d.value);
        else if(p.id=="min")assign(field,p.kind,d.minimum);
        else if(p.id=="max")assign(field,p.kind,d.maximum);
        else if(d.properties.contains(p.id))assign(field,p.kind,d.properties.value(p.id).toVariant());
        else if(p.defaultValue.isValid())assign(field,p.kind,p.defaultValue);
        else if(p.kind=="choice")assign(field,p.kind,p.choices.value(0));
        else if(p.kind=="boolean")assign(field,p.kind,false);
        else if(p.kind=="integer" || p.kind=="double")
            assign(field,p.kind,qBound(p.minimum,0.0,p.maximum));
        else assign(field,p.kind,QString());
    }
    // Values were assigned with signals blocked, so explicitly redraw the
    // correct selected equation (including its font size) after changing tabs.
    if(d.type==QStringLiteral("equation"))refreshEquationPreview();
    m_changing=false;
    setEditable(editable);
}
void WidgetPropertyInspector::refreshEquationPreview() {
    if(m_type!=QStringLiteral("equation") || !m_dynamic)return;
    auto* preview=m_dynamic->findChild<QLabel*>(QStringLiteral("equationLivePreview"));
    auto* sourceEditor=dynamic_cast<MultilinePropertyEditor*>(m_fields.value(QStringLiteral("source"),nullptr));
    if(!preview || !sourceEditor)return;
    const QString source=sourceEditor->toPlainText();
    QString error;
    if(!EquationMath::validSource(source,&error)){
        preview->setPixmap(QPixmap());
        preview->setText(error);
        return;
    }
    preview->clear();
    const auto* sizeEditor=qobject_cast<QSpinBox*>(m_fields.value(QStringLiteral("pointSize"),nullptr));
    const int fontPx=sizeEditor?sizeEditor->value():25;
    // A 2x antialiased render, displayed at native size, never enlarged.
    QImage image=EquationMath::render(source,fontPx*2,QColor(QStringLiteral("#eaf5ff")));
    const QSize natural(qMax(1,image.width()/2),qMax(1,image.height()/2));
    const QSize target=natural.scaled(QSize(470,80),Qt::KeepAspectRatio);
    preview->setPixmap(QPixmap::fromImage(image.scaled(target,Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation)));
}
void WidgetPropertyInspector::setEditable(bool editable) {
    m_editable=editable;
    for(QWidget* w:{static_cast<QWidget*>(m_id),static_cast<QWidget*>(m_x),
                   static_cast<QWidget*>(m_y),static_cast<QWidget*>(m_width),
                   static_cast<QWidget*>(m_height)})w->setEnabled(editable);
    for(const WidgetProperty& p:m_metadata)
        if(auto* w=m_fields.value(p.id,nullptr))w->setEnabled(editable && p.writable);
    if(m_imageImport)m_imageImport->setEnabled(editable);
    if(m_imageClear)m_imageClear->setEnabled(editable && !m_imageData.isEmpty());
}
PanelControl WidgetPropertyInspector::readControl(const PanelControl& original) const {
    PanelControl d=original;
    d.id=m_id->text().trimmed();
    d.geometry=QRect(m_x->value(),m_y->value(),m_width->value(),m_height->value());
    for(const WidgetProperty& p:m_metadata) {
        QWidget* w=m_fields.value(p.id,nullptr);
        if(!w || !p.writable)continue;
        const QVariant value=valueOf(w,p.kind);
        if(p.id=="text")d.text=value.toString();
        else if(p.id=="value")d.value=value.toDouble();
        else if(p.id=="min")d.minimum=value.toDouble();
        else if(p.id=="max")d.maximum=value.toDouble();
        else d.properties.insert(p.id,QJsonValue::fromVariant(value));
    }
    if(d.type==QStringLiteral("image")) {
        if(m_imageData.isEmpty()) {
            d.properties.remove(QStringLiteral("imageData"));
            d.properties.remove(QStringLiteral("imageName"));
        } else {
            d.properties.insert(QStringLiteral("imageData"),m_imageData);
            d.properties.insert(QStringLiteral("imageName"),m_imageName);
        }
    }
    return d;
}
