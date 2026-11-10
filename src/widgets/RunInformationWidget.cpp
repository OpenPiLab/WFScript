#include "RunInformationWidget.h"
#include "InstrumentVisuals.h"
#include "../RunInformationSchema.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFont>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QListView>
#include <QPalette>
#include <QEvent>
#include <QFrame>
#include <QColor>
#include <QBrush>
#include <utility>

// The Run Information widget is a *view* of the project's Test Executive
// field schema, not a second configuration. Operator values are sent through
// PanelCanvas -> PanelRuntime -> TestRunnerBridge, never saved in the .wfpanel.
RunInformationWidget::RunInformationWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("wfRunInformationRoot"));
    setAttribute(Qt::WA_StyledBackground,true);
    auto* outer=new QVBoxLayout(this);
    outer->setContentsMargins(12,10,12,10);
    outer->setSpacing(4);

    m_heading=new QLabel(QStringLiteral("RUN INFORMATION"),this);
    m_heading->setObjectName(QStringLiteral("wfRunInfoHeading"));
    QFont heading=m_heading->font();heading.setBold(true);m_heading->setFont(heading);
    outer->addWidget(m_heading);

    m_linkLabel=new QLabel(QStringLiteral("SHARED UUT INPUTS — ALL FORMS STAY IN SYNC"),this);
    m_linkLabel->setObjectName(QStringLiteral("wfRunInfoLink"));
    m_linkLabel->setWordWrap(true);
    outer->addWidget(m_linkLabel);

    m_hint=new QLabel(QStringLiteral("Fields: Test Sequence > Run Information  |  JS: TestRunner.pendingRunInfo()"),this);
    m_hint->setObjectName(QStringLiteral("wfRunInfoHint"));
    m_hint->setWordWrap(true);
    outer->addWidget(m_hint);

    m_scroll=new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("wfRunInfoScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setObjectName(QStringLiteral("wfRunInfoViewport"));
    m_content=new QWidget(m_scroll);
    m_content->setObjectName(QStringLiteral("wfRunInfoContent"));
    m_form=new QFormLayout(m_content);
    m_form->setContentsMargins(2,8,3,4);
    m_form->setSpacing(9);
    m_form->setHorizontalSpacing(16);
    m_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_form->setLabelAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    m_scroll->setWidget(m_content);
    outer->addWidget(m_scroll,1);
    applyTheme();
}

void RunInformationWidget::setSchema(const QJsonObject& schema) {
    if(m_schemaApplied && schema==m_schema)return;
    m_schemaApplied=true;
    m_schema=schema;
    m_inputs.clear();
    while(m_form->rowCount())m_form->removeRow(0);
    const QJsonArray fields=schema.value("fields").toArray();
    if(fields.isEmpty()) {
        auto* help=new QLabel(QStringLiteral("No fields configured. Open Test Sequence > Run Information... to define the UUT/operator fields for this project."),m_content);
        help->setObjectName(QStringLiteral("wfRunInfoEmpty"));
        help->setWordWrap(true);
        m_form->addRow(help);
        applyTheme();
        return;
    }
    for(const auto& item:fields) {
        const QJsonObject f=item.toObject();
        const QString id=f.value("id").toString();
        const QString type=f.value("type").toString();
        const QString title=f.value("label").toString()+(f.value("required").toBool()?QStringLiteral(" *"):QString());
        auto* caption=new QLabel(title,m_content);
        caption->setObjectName(QStringLiteral("wfRunInfoFieldLabel"));
        caption->setWordWrap(true);
        caption->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        QWidget* input=nullptr;
        if(type=="boolean") {
            auto* cb=new QCheckBox(m_content);
            cb->setChecked(f.value("default").toBool());
            cb->setToolTip(title);
            connect(cb,&QCheckBox::toggled,this,[this]{emitValues();});
            input=cb;
        } else if(type=="choice") {
            auto* combo=new QComboBox(m_content);
            // Explicit Qt-managed view: Windows native combobox popups do not
            // consistently inherit the Panel foreground/background palette.
            auto* popup=new QListView(combo);
            popup->setObjectName(QStringLiteral("wfRunInfoChoicePopup"));
            combo->setView(popup);
            combo->addItem(QStringLiteral("-- Select --"),QString());
            for(const auto& choice:f.value("options").toArray())
                combo->addItem(choice.toString(),choice.toString());
            const int index=combo->findData(f.value("default").toString());
            combo->setCurrentIndex(index>=0?index:0);
            connect(combo,&QComboBox::currentIndexChanged,this,[this]{emitValues();});
            input=combo;
        } else {
            auto* edit=new QLineEdit(m_content);
            edit->setText(f.value("default").toVariant().toString());
            edit->setMaxLength(type=="number"?40:200);
            if(type=="number")edit->setPlaceholderText(QStringLiteral("Enter number"));
            connect(edit,&QLineEdit::textChanged,this,[this]{emitValues();});
            input=edit;
        }
        input->setObjectName(QStringLiteral("uut_")+id);
        input->setMinimumHeight(31);
        input->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        m_inputs.insert(id,input);
        m_form->addRow(caption,input);
    }
    applyTheme();
}

QVariantMap RunInformationWidget::values() const {
    QVariantMap result;
    for(auto it=m_inputs.cbegin();it!=m_inputs.cend();++it) {
        if(const auto* cb=qobject_cast<QCheckBox*>(it.value()))result.insert(it.key(),cb->isChecked());
        else if(const auto* combo=qobject_cast<QComboBox*>(it.value()))result.insert(it.key(),combo->currentData().toString());
        else if(const auto* line=qobject_cast<QLineEdit*>(it.value()))result.insert(it.key(),line->text());
    }
    return result;
}

void RunInformationWidget::setValues(const QVariantMap& values) {
    for(auto it=m_inputs.cbegin();it!=m_inputs.cend();++it) {
        if(!values.contains(it.key()))continue;
        QSignalBlocker block(it.value());
        if(auto* cb=qobject_cast<QCheckBox*>(it.value()))cb->setChecked(values.value(it.key()).toBool());
        else if(auto* combo=qobject_cast<QComboBox*>(it.value())) {
            const int index=combo->findData(values.value(it.key()).toString());
            if(index>=0)combo->setCurrentIndex(index);
        } else if(auto* line=qobject_cast<QLineEdit*>(it.value()))line->setText(values.value(it.key()).toString());
    }
}
void RunInformationWidget::emitValues(){emit valuesChanged(values());}

bool RunInformationWidget::event(QEvent* e) {
    if(e->type()==QEvent::ThemeChange)applyTheme();
    return QWidget::event(e);
}

void RunInformationWidget::applyTheme() {
    const bool light=InstrumentVisuals::light(this);
    const QString background=light?QStringLiteral("#eef5fa"):QStringLiteral("#162638");
    const QString ink=light?QStringLiteral("#20384c"):QStringLiteral("#e5eef7");
    const QString subtle=light?QStringLiteral("#546f84"):QStringLiteral("#afc3d5");
    const QString accent=light?QStringLiteral("#087293"):QStringLiteral("#60d4ee");
    const QString surface=light?QStringLiteral("#ffffff"):QStringLiteral("#273d53");
    const QString border=light?QStringLiteral("#a8c0d1"):QStringLiteral("#55728b");
    const QString selection=light?QStringLiteral("#d2ebf5"):QStringLiteral("#325b72");

    // QScrollArea has a separate viewport and content widget. Leaving either
    // with its native (usually white) palette caused invisible light text in
    // dark Panels on Windows. Set all three palettes explicitly as well as
    // the stylesheet so native themes cannot reintroduce white rectangles.
    for(QWidget* w:{static_cast<QWidget*>(m_scroll),m_scroll->viewport(),m_content}) {
        QPalette p=w->palette();
        p.setColor(QPalette::Window,QColor(background));
        p.setColor(QPalette::Base,QColor(background));
        p.setColor(QPalette::WindowText,QColor(ink));
        p.setColor(QPalette::Text,QColor(ink));
        w->setPalette(p);
        w->setAutoFillBackground(true);
    }
    setStyleSheet(QStringLiteral(
        "QWidget#wfRunInformationRoot{background:%1;border:1px solid %2;border-radius:8px;}"
        "QScrollArea#wfRunInfoScroll{background:%1;border:0;}"
        "QWidget#wfRunInfoViewport,QWidget#wfRunInfoContent{background:%1;border:0;}"
        "QScrollBar:vertical{background:%1;width:10px;}"
        "QScrollBar::handle:vertical{background:%2;border-radius:4px;min-height:22px;}"
    ).arg(background,border));
    m_heading->setStyleSheet(QStringLiteral("QLabel{color:%1;background:transparent;border:0;font-weight:700;}").arg(ink));
    m_linkLabel->setStyleSheet(QStringLiteral("QLabel{color:%1;background:transparent;border:0;font-weight:600;font-size:11px;}").arg(accent));
    m_hint->setStyleSheet(QStringLiteral("QLabel{color:%1;background:transparent;border:0;font-size:11px;}").arg(subtle));

    for(QLabel* label:m_content->findChildren<QLabel*>()) {
        label->setStyleSheet(QStringLiteral("QLabel{color:%1;background:transparent;border:0;font-weight:500;}").arg(ink));
    }
    const QString editCss=QStringLiteral(
        "QLineEdit{background:%1;color:%2;border:1px solid %3;border-radius:5px;padding:5px 8px;selection-background-color:%4;selection-color:%2;}"
        "QLineEdit:focus{border:1px solid %5;}").arg(surface,ink,border,selection,accent);
    const QString comboCss=QStringLiteral(
        "QComboBox{background:%1;color:%2;border:1px solid %3;border-radius:5px;padding:5px 8px;selection-background-color:%4;selection-color:%2;}"
        "QComboBox:focus{border:1px solid %5;}"
        "QComboBox QAbstractItemView{background:%1;color:%2;border:1px solid %3;selection-background-color:%4;selection-color:%2;}")
        .arg(surface,ink,border,selection,accent);
    for(QWidget* input:std::as_const(m_inputs)) {
        QPalette p=input->palette();
        p.setColor(QPalette::WindowText,QColor(ink));
        p.setColor(QPalette::Text,QColor(ink));
        p.setColor(QPalette::ButtonText,QColor(ink));
        p.setColor(QPalette::Base,QColor(surface));
        p.setColor(QPalette::Button,QColor(surface));
        p.setColor(QPalette::Highlight,QColor(selection));
        p.setColor(QPalette::HighlightedText,QColor(ink));
        input->setPalette(p);
        if(auto* line=qobject_cast<QLineEdit*>(input))line->setStyleSheet(editCss);
        else if(auto* combo=qobject_cast<QComboBox*>(input)) {
            combo->setStyleSheet(comboCss);
            if(auto* popup=combo->view()) {
                popup->setPalette(p);
                popup->setStyleSheet(QStringLiteral(
                    "QListView{background:%1;color:%2;border:1px solid %3;}"
                    "QListView::item{background:%1;color:%2;min-height:24px;}"
                    "QListView::item:selected{background:%4;color:%2;}").arg(surface,ink,border,selection));
            }
            for(int row=0;row<combo->count();++row)
                combo->setItemData(row,QBrush(QColor(ink)),Qt::ForegroundRole);
        } else if(auto* check=qobject_cast<QCheckBox*>(input)) {
            check->setStyleSheet(QStringLiteral("QCheckBox{background:transparent;color:%1;border:0;spacing:8px;}"
                "QCheckBox::indicator{width:16px;height:16px;}").arg(ink));
        }
    }
}
