#include "PageAppearanceEditor.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PageAppearanceEditor::PageAppearanceEditor(QWidget* parent):QWidget(parent) {
    auto* v=new QVBoxLayout(this);
    v->setContentsMargins(0,0,0,0);
    v->setSpacing(9);
    m_name=new QLabel(this);
    QFont heading=m_name->font();heading.setBold(true);m_name->setFont(heading);
    v->addWidget(m_name);

    auto* dimensions=new QGroupBox(tr("Page — Dimensions"),this);
    auto* dimensionsLayout=new QVBoxLayout(dimensions);
    auto* dimensionForm=new QFormLayout;
    m_width=new QSpinBox(dimensions);
    m_width->setObjectName(QStringLiteral("panelPageWidth"));
    m_width->setRange(320,8192);
    m_width->setKeyboardTracking(false);
    m_width->setSuffix(QStringLiteral(" px"));
    m_height=new QSpinBox(dimensions);
    m_height->setObjectName(QStringLiteral("panelPageHeight"));
    m_height->setRange(240,8192);
    m_height->setKeyboardTracking(false);
    m_height->setSuffix(QStringLiteral(" px"));
    dimensionForm->addRow(tr("Width"),m_width);
    dimensionForm->addRow(tr("Height"),m_height);
    dimensionsLayout->addLayout(dimensionForm);
    m_fillWorkspace=new QPushButton(tr("Fill Available Workspace"),dimensions);
    m_fillWorkspace->setObjectName(QStringLiteral("panelFillWorkspace"));
    m_fillWorkspace->setToolTip(tr("Set this Page's dimensions to the available canvas viewport; this is not zoom."));
    dimensionsLayout->addWidget(m_fillWorkspace);
    m_sizeHint=new QLabel(dimensions);
    m_sizeHint->setWordWrap(true);
    dimensionsLayout->addWidget(m_sizeHint);
    m_sizeError=new QLabel(dimensions);
    m_sizeError->setObjectName(QStringLiteral("panelSizeValidation"));
    m_sizeError->setWordWrap(true);
    m_sizeError->setStyleSheet(QStringLiteral("color:#bf4242;"));
    m_sizeError->hide();
    dimensionsLayout->addWidget(m_sizeError);
    v->addWidget(dimensions);
    const auto commitSize=[this] {
        if(!m_loading)emit sizeRequested(QSize(m_width->value(),m_height->value()));
    };
    connect(m_width,&QSpinBox::editingFinished,this,commitSize);
    connect(m_height,&QSpinBox::editingFinished,this,commitSize);
    connect(m_fillWorkspace,&QPushButton::clicked,this,&PageAppearanceEditor::fillWorkspaceRequested);

    auto* appearance=new QGroupBox(tr("Panel & Page — Appearance"),this);
    auto* appearanceLayout=new QVBoxLayout(appearance);
    auto* form=new QFormLayout;
    m_theme=new QComboBox(appearance);
    m_theme->addItem(tr("Follow Application"),QStringLiteral("follow"));
    m_theme->addItem(tr("Always Dark"),QStringLiteral("dark"));
    m_theme->addItem(tr("Always Light"),QStringLiteral("light"));
    form->addRow(tr("Panel theme"),m_theme);
    auto colorField=[this,form,appearance](const QString& label,QLineEdit*& field,const QString& hint) {
        auto* row=new QWidget(appearance);
        auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);
        field=new QLineEdit(row);
        field->setPlaceholderText(hint);
        field->setToolTip(tr("Leave blank to inherit the theme; otherwise use #RRGGBB."));
        auto* pick=new QPushButton(QStringLiteral("…"),row);
        pick->setFixedWidth(28);
        layout->addWidget(field,1);layout->addWidget(pick);
        form->addRow(label,row);
        connect(field,&QLineEdit::editingFinished,this,&PageAppearanceEditor::commit);
        connect(pick,&QPushButton::clicked,this,[this,field] {
            const QColor initial=QColor(field->text().trimmed());
            const QColor chosen=QColorDialog::getColor(initial.isValid()?initial:QColor(25,39,57),this,
                                                       tr("Choose Panel Background"));
            if(chosen.isValid()){field->setText(chosen.name());commit();}
        });
    };
    colorField(tr("Panel background"),m_panelColor,tr("Theme default"));
    colorField(tr("Page background"),m_pageColor,tr("Inherit Panel"));
    m_grid=new QCheckBox(tr("Show grid in Design mode"),appearance);
    form->addRow(QString(),m_grid);
    appearanceLayout->addLayout(form);
    auto* help=new QLabel(tr("Panel colours apply across all Pages. Page background affects only this Page. The grid is hidden in Run mode."),appearance);
    help->setWordWrap(true);appearanceLayout->addWidget(help);
    v->addWidget(appearance);
    connect(m_theme,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){commit();});
    connect(m_grid,&QCheckBox::toggled,this,[this](bool){commit();});

    auto* lifecycle=new QGroupBox(tr("Panel — JavaScript Lifecycle"),this);
    auto* lifecycleLayout=new QVBoxLayout(lifecycle);
    m_initialize=new QCheckBox(tr("Enable onInitialize()"),lifecycle);
    m_initialize->setObjectName(QStringLiteral("enablePanelInitialize"));
    lifecycleLayout->addWidget(m_initialize);
    auto* initInfo=new QLabel(tr("Called once after the script and libraries load, before Panel events are enabled. An enabled hook must exist."),lifecycle);
    initInfo->setWordWrap(true);lifecycleLayout->addWidget(initInfo);
    m_editInitialize=new QPushButton(tr("Create / Go to onInitialize()"),lifecycle);
    m_editInitialize->setObjectName(QStringLiteral("editPanelInitialize"));
    lifecycleLayout->addWidget(m_editInitialize);
    m_shutdown=new QCheckBox(tr("Enable onShutdown()"),lifecycle);
    m_shutdown->setObjectName(QStringLiteral("enablePanelShutdown"));
    lifecycleLayout->addWidget(m_shutdown);
    auto* shutdownInfo=new QLabel(tr("Called once during normal Stop before instrument cleanup. Not called after forced interruption or failed startup."),lifecycle);
    shutdownInfo->setWordWrap(true);lifecycleLayout->addWidget(shutdownInfo);
    m_editShutdown=new QPushButton(tr("Create / Go to onShutdown()"),lifecycle);
    m_editShutdown->setObjectName(QStringLiteral("editPanelShutdown"));
    lifecycleLayout->addWidget(m_editShutdown);
    v->addWidget(lifecycle);
    v->addStretch();
    const auto commitLifecycle=[this](bool){
        if(!m_loading)emit lifecycleEdited(m_initialize->isChecked(),m_shutdown->isChecked());
    };
    connect(m_initialize,&QCheckBox::toggled,this,commitLifecycle);
    connect(m_shutdown,&QCheckBox::toggled,this,commitLifecycle);
    connect(m_editInitialize,&QPushButton::clicked,this,[this]{emit lifecycleFunctionRequested(QStringLiteral("onInitialize"));});
    connect(m_editShutdown,&QPushButton::clicked,this,[this]{emit lifecycleFunctionRequested(QStringLiteral("onShutdown"));});
}

void PageAppearanceEditor::setAppearance(const PanelAppearance& appearance,
                                         const QString& pageBackground,const QString& pageName) {
    m_loading=true;
    m_name->setText(tr("Page: %1").arg(pageName));
    const int index=m_theme->findData(appearance.themeMode);
    m_theme->setCurrentIndex(index<0?0:index);
    m_panelColor->setText(appearance.background);
    m_pageColor->setText(pageBackground);
    m_grid->setChecked(appearance.showDesignGrid);
    m_panelColor->setStyleSheet(QString());m_pageColor->setStyleSheet(QString());
    m_loading=false;
}

void PageAppearanceEditor::setPageSize(const QSize& size,const QSize& minimumContent) {
    m_loading=true;
    m_width->setValue(size.width());
    m_height->setValue(size.height());
    m_sizeHint->setText(tr("Current Page dimensions (not zoom). Existing widgets require at least %1 × %2 pixels. Valid limits: 320–8192 × 240–8192.")
                            .arg(minimumContent.width()).arg(minimumContent.height()));
    m_loading=false;
    showSizeError(QString());
}

void PageAppearanceEditor::setLifecycle(const PanelLifecycleSettings& lifecycle) {
    m_loading=true;
    m_initialize->setChecked(lifecycle.initializeEnabled);
    m_shutdown->setChecked(lifecycle.shutdownEnabled);
    m_loading=false;
}

void PageAppearanceEditor::setEditable(bool editable) {
    m_theme->setEnabled(editable);
    m_panelColor->setEnabled(editable);
    m_pageColor->setEnabled(editable);
    m_grid->setEnabled(editable);
    m_width->setEnabled(editable);
    m_height->setEnabled(editable);
    m_fillWorkspace->setEnabled(editable);
    m_initialize->setEnabled(editable);
    m_shutdown->setEnabled(editable);
    m_editInitialize->setEnabled(editable);
    m_editShutdown->setEnabled(editable);
    for(QPushButton* b : findChildren<QPushButton*>())b->setEnabled(editable);
}

void PageAppearanceEditor::showSizeError(const QString& message) {
    m_sizeError->setText(message);
    m_sizeError->setVisible(!message.isEmpty());
}

void PageAppearanceEditor::commit() {
    if(m_loading)return;
    const QString panel=m_panelColor->text().trimmed();
    const QString page=m_pageColor->text().trimmed();
    m_panelColor->setStyleSheet(PanelAppearance::validColor(panel)?QString():QStringLiteral("border:1px solid #d94444"));
    m_pageColor->setStyleSheet(PanelAppearance::validColor(page)?QString():QStringLiteral("border:1px solid #d94444"));
    if(!PanelAppearance::validColor(panel) || !PanelAppearance::validColor(page))return;
    emit edited(m_theme->currentData().toString(),panel,page,m_grid->isChecked());
}
