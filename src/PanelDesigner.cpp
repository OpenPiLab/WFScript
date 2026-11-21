#include "PanelDesigner.h"
#include "PanelEventCatalog.h"
#include "PanelDataSchema.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/WidgetPropertyInspector.h"
#include "UiIcons.h"
#include "panel/PageAppearanceEditor.h"
#include "panel/PanelAppearanceColors.h"
#include <QApplication>
#include <QEvent>
#include <QAbstractItemView>
#include <QDrag>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include <QDialog>
#include <QPalette>
#include <QColor>
#include <QBrush>
#include <QMenu>
#include <QAction>
#include <QUndoStack>
#include <QSignalBlocker>
#include <QMetaType>
#include <QTabWidget>
#include <QTabBar>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QRegularExpression>
#include <QInputDialog>
#include <QPointer>
#include <QToolButton>
#include <QMap>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QGuiApplication>
#include <QFont>
#include <algorithm>
#include <cmath>

namespace {
// Group headings are not controls. Only leaf items may be dragged/added.
class ToolboxTree final : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    // Explicit foreground/background for category headers. Native Windows/Qt
    // styles otherwise sometimes render the collapsed category label with the
    // same colour as its background until the mouse enters the row.
    void refreshCategoryPalette() {
        const QColor base=palette().color(QPalette::Base);
        const bool dark=base.lightness()<128;
        const QColor background=dark ? QColor(QStringLiteral("#273a50"))
                                     : QColor(QStringLiteral("#e1e8f0"));
        const QColor foreground=dark ? QColor(QStringLiteral("#f4f7fc"))
                                     : QColor(QStringLiteral("#203247"));
        for(int i=0;i<topLevelItemCount();++i) {
            auto* header=topLevelItem(i);
            header->setForeground(0,QBrush(foreground));
            header->setBackground(0,QBrush(background));
        }
    }
protected:
    bool event(QEvent* event) override {
        const bool processed=QTreeWidget::event(event);
        if(event->type()==QEvent::PaletteChange ||
           event->type()==QEvent::ApplicationPaletteChange ||
           event->type()==QEvent::StyleChange)
            refreshCategoryPalette();
        return processed;
    }
    void startDrag(Qt::DropActions) override {
        auto* selected=currentItem();
        if(!selected || selected->childCount()>0 ||
           selected->data(0,Qt::UserRole).toString().isEmpty())return;
        auto* drag=new QDrag(this);
        auto* mime=new QMimeData;
        mime->setData("application/x-wfscript-control",
                      selected->data(0,Qt::UserRole).toString().toUtf8());
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
        delete drag;
    }
};

// Classification is only presentation; serialized widget type IDs and the
// central registry stay unchanged. Unrecognized third-party types go into Other.
QString toolboxCategory(const QString& type) {
    if(QStringList{"button","momentary_button","switch_button","toggle"}.contains(type))return QStringLiteral("Buttons & Switches");
    if(QStringList{"slider","rotary_input","numeric_stepper","option_selector","precision_dial","text_input","ip_address_input"}.contains(type))return QStringLiteral("Input Controls");
    if(QStringList{"image","instruction_card","equation","experiment_table"}.contains(type))return QStringLiteral("Images & Education");
    if(QStringList{"label","numeric","digital_display","gauge","status_beacon","segmented_bar","status_banner"}.contains(type))return QStringLiteral("Display & Indicators");
    if(QStringList{"plot","digital_plot"}.contains(type))return QStringLiteral("Charts & Plots");
    if(type==QStringLiteral("uart_terminal"))return QStringLiteral("Digital Instruments");
    if(type==QStringLiteral("communications_terminal"))return QStringLiteral("Communications");
    if(QStringList{"runInformation","test_sequence","measurement_result","measurement_table","event_log"}.contains(type))return QStringLiteral("Test Executive");
    return QStringLiteral("Other Widgets");
}


}

PanelDesigner::PanelDesigner(QWidget* parent):QWidget(parent) {
    auto* page=new QVBoxLayout(this);
    page->setContentsMargins(6,6,6,6);
    m_editorToolbar=new QWidget(this);
    m_editorToolbar->setObjectName(QStringLiteral("panelEditorToolbar"));
    auto* tools=new QHBoxLayout(m_editorToolbar);
    tools->setContentsMargins(0,0,0,0);
    tools->setSpacing(5);
    auto button=[this,tools](const QString& text){auto* b=new QPushButton(text,this);tools->addWidget(b);return b;};
    auto* fresh=button("New Panel");auto* load=button("Open Panel…");auto* save=button("Save/Export…");
    m_new=fresh;m_open=load;m_save=save;
    // QRC-bundled, ordinary .wfpanel projects. The menu is catalog-driven:
    // adding an example requires no new C++ branches or embedded JSON literal.
    m_examples=new QToolButton(this);
    m_examples->setObjectName(QStringLiteral("panelExamplesMenuButton"));
    m_examples->setText(QStringLiteral("Examples"));
    m_examples->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_examples->setToolTip(QStringLiteral("Open a bundled, editable Panel example"));
    m_examples->setPopupMode(QToolButton::InstantPopup);
    auto* examplesMenu=new QMenu(m_examples);
    m_examples->setMenu(examplesMenu);
    tools->insertWidget(tools->indexOf(save),m_examples);
    QFile catalog(QStringLiteral(":/wfscript/examples/catalog.json"));
    if(catalog.open(QIODevice::ReadOnly)) {
        const QJsonDocument json=QJsonDocument::fromJson(catalog.readAll());
        QMap<QString,QMenu*> groups;
        for(const QJsonValue v:json.array()) {
            if(!v.isObject())continue;
            const QJsonObject o=v.toObject();
            const QString group=o.value(QStringLiteral("category")).toString();
            const QString title=o.value(QStringLiteral("title")).toString();
            const QString stem=o.value(QStringLiteral("file")).toString();
            if(group.isEmpty() || title.isEmpty() || stem.isEmpty())continue;
            QMenu* menu=groups.value(group,nullptr);
            if(!menu) {menu=examplesMenu->addMenu(group);groups.insert(group,menu);}
            QAction* action=menu->addAction(title);
            action->setToolTip(o.value(QStringLiteral("description")).toString());
            const QString resource=QStringLiteral(":/wfscript/examples/")+stem+QStringLiteral(".wfpanel");
            connect(action,&QAction::triggered,this,[this,resource]{emit exampleRequested(resource);});
        }
    }
    if(examplesMenu->actions().isEmpty()) {
        auto* missing=examplesMenu->addAction(QStringLiteral("Examples unavailable"));
        missing->setEnabled(false);
    }

    tools->addSpacing(12);
    m_run=button("▶ Run Panel");m_stop=button("■ Stop / Design");
    auto* edit=button("Edit JavaScript");
    auto* libs=button("Libraries...");
    m_sequenceEditor=button("Test Sequence...");
    m_sequenceEditor->setObjectName(QStringLiteral("openVisualTestSequenceEditor"));
    m_sequenceEditor->setToolTip(QStringLiteral("Visually edit the self-contained TestRunner definition in TestStationSequence.mjs"));
    UiIcons::decorate(fresh, "new", QStringLiteral("Create a new panel project"));
    UiIcons::decorate(load, "open", QStringLiteral("Open a .wfpanel project"));
    UiIcons::decorate(save, "save", QStringLiteral("Save/export the current .wfpanel project"));
    UiIcons::decorate(m_run, "run", QStringLiteral("Start the panel's shared JavaScript runtime"));
    UiIcons::decorate(m_stop, "stop", QStringLiteral("Stop the panel and return to Design mode"));
    UiIcons::decorate(edit, "code", QStringLiteral("Open the main panel JavaScript in Script Editor"));
    UiIcons::decorate(libs, "libraries", QStringLiteral("Create, edit and manage reusable script libraries"));
    // These buttons stay on the same toolbar, including in Focus Canvas mode.
    tools->addStretch(1);
    m_toolboxToggle=button("Toolbox");
    m_propertiesToggle=button("Properties");
    // Fit Page defaults ON in Run mode; Design remains at exact document pixels.
    m_fitPageToggle=button("Fit Page");
    m_fitPageToggle->setObjectName(QStringLiteral("panelFitPageToggle"));
    m_fitPageToggle->setCheckable(true);
    m_fitPageToggle->setChecked(true);
    m_fitPageToggle->setEnabled(false);
    m_fitPageToggle->setToolTip(QStringLiteral("Run: display the complete Page without scrolling. Turn off for 100% size and scroll bars. Never changes the saved Page."));
    m_fitPagePercent=new QLabel(QStringLiteral("100%"),this);
    m_fitPagePercent->setObjectName(QStringLiteral("panelFitPagePercent"));
    m_fitPagePercent->setToolTip(QStringLiteral("Temporary display scale; saved Panel coordinates are unchanged."));
    m_fitPagePercent->hide();
    tools->addWidget(m_fitPagePercent);
    m_focusToggle=button("Focus Canvas");
    m_designToolsToggle=button("Design Tools");
    m_designToolsToggle->setObjectName(QStringLiteral("panelDesignToolsToggle"));
    m_designToolsToggle->setCheckable(true);
    m_designToolsToggle->setToolTip(QStringLiteral("Show movable Design Tools palette (Undo, Copy, Align, Arrange, Snap). Canvas shortcuts remain available."));
    UiIcons::decorate(m_toolboxToggle, "toolbox");
    UiIcons::decorate(m_propertiesToggle, "properties");
    UiIcons::decorate(m_focusToggle, "focus");
    for(auto* b : {m_toolboxToggle,m_propertiesToggle,m_focusToggle}) b->setCheckable(true);
    m_toolboxToggle->setChecked(true);
    m_propertiesToggle->setChecked(true);
    m_focusToggle->setChecked(false);
    m_toolboxToggle->setToolTip("Show or hide the Toolbox without affecting the panel document");
    m_propertiesToggle->setToolTip("Show or hide the Properties editor");
    m_focusToggle->setToolTip("Temporarily hide side panels; click again to restore");
    page->addWidget(m_editorToolbar);
    // Canvas instructions are available in the Design Tools palette and tooltips;
    // do not reserve a permanent header row for a tutorial paragraph.
    m_focusToggle->setToolTip(QStringLiteral("Focus Canvas: temporarily hide side panes. Design Tools and canvas keyboard shortcuts remain available."));
    auto* split=new QSplitter(Qt::Horizontal,this);
    m_splitter=split;
    auto* left=new QWidget(split);
    m_toolboxPane=left;auto* ll=new QVBoxLayout(left);
    ll->setContentsMargins(2,2,2,2);
    ll->addWidget(new QLabel("Toolbox",left));
    m_toolboxSearch=new QLineEdit(left);
    m_toolboxSearch->setObjectName(QStringLiteral("toolboxSearch"));
    m_toolboxSearch->setPlaceholderText(QStringLiteral("Find widgets..."));
    m_toolboxSearch->setClearButtonEnabled(true);
    m_toolboxSearch->setToolTip(QStringLiteral("Filter by widget name, type ID or category"));
    ll->addWidget(m_toolboxSearch);
    m_toolbox=new ToolboxTree(left);
    m_toolbox->setObjectName(QStringLiteral("groupedWidgetToolbox"));
    m_toolbox->setHeaderHidden(true);
    m_toolbox->setIndentation(15);
    m_toolbox->setRootIsDecorated(true);
    m_toolbox->setUniformRowHeights(true);
    m_toolbox->setSelectionMode(QAbstractItemView::SingleSelection);
    // Keep familiar groups in a stable order, but NEVER use a fixed group list
    // as a filter for the registered factories: future categories must appear
    // automatically when a widget factory is added to the registry.
    QStringList categories={QStringLiteral("Buttons & Switches"),
        QStringLiteral("Input Controls"),QStringLiteral("Display & Indicators"),
        QStringLiteral("Charts & Plots"),QStringLiteral("Images & Education"),
        QStringLiteral("Digital Instruments"),QStringLiteral("Communications"),
        QStringLiteral("Test Executive"),QStringLiteral("Other Widgets")};
    const QStringList types=PanelCanvas::supportedTypes();
    for(const QString& type:types) {
        const QString category=toolboxCategory(type);
        if(!categories.contains(category))categories.append(category);
    }
    for(const QString& category:categories) {
        QList<QPair<QString,QString>> members;
        for(const QString& type:types)if(toolboxCategory(type)==category) {
            const auto* factory=WidgetRegistry::instance().factory(type);
            members.append({factory?factory->displayName():type,type});
        }
        if(members.isEmpty())continue;
        std::sort(members.begin(),members.end(),[](const auto& a,const auto& b){
            return a.first.compare(b.first,Qt::CaseInsensitive)<0;
        });
        auto* header=new QTreeWidgetItem(m_toolbox,{category});
        header->setFlags(Qt::ItemIsEnabled); // never add/drag a category
        QFont font=header->font(0);font.setBold(true);header->setFont(0,font);
        for(const auto& member:members) {
            auto* leaf=new QTreeWidgetItem(header,{member.first});
            leaf->setData(0,Qt::UserRole,member.second);
            leaf->setToolTip(0,QStringLiteral("%1 (%2) — double-click or drag onto canvas")
                                .arg(member.first,member.second));
            leaf->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsDragEnabled);
        }
        // Keep the first-use Toolbox compact. Operators can expand any group;
        // search expands only groups that actually contain matches.
        header->setExpanded(category==QStringLiteral("Buttons & Switches") ||
                            category==QStringLiteral("Test Executive"));
    }
    static_cast<ToolboxTree*>(m_toolbox)->refreshCategoryPalette();
    // Search never modifies the registry, document or serialized widget IDs.
    connect(m_toolboxSearch,&QLineEdit::textChanged,m_toolbox,
            [this](const QString& text){
        const QString query=text.trimmed();
        for(int i=0;i<m_toolbox->topLevelItemCount();++i) {
            auto* header=m_toolbox->topLevelItem(i);
            bool visible=false;
            const bool categoryMatch=header->text(0).contains(query,Qt::CaseInsensitive);
            for(int j=0;j<header->childCount();++j) {
                auto* leaf=header->child(j);
                const bool match=categoryMatch || leaf->text(0).contains(query,Qt::CaseInsensitive) ||
                    leaf->data(0,Qt::UserRole).toString().contains(query,Qt::CaseInsensitive);
                leaf->setHidden(!match);
                visible|=match;
            }
            header->setHidden(!visible);
            if(!query.isEmpty() && visible)header->setExpanded(true);
        }
        if(auto* selected=m_toolbox->currentItem();selected && selected->isHidden())
            m_toolbox->setCurrentItem(nullptr);
    });
    m_toolbox->setDragEnabled(true);
    m_toolbox->setDragDropMode(QAbstractItemView::DragOnly);
    ll->addWidget(m_toolbox);
    m_add=new QPushButton("Add selected control",left);ll->addWidget(m_add);
    m_add->setEnabled(false);
    connect(m_toolbox,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem* item){
        m_add->setEnabled(!m_running && item &&
            !item->data(0,Qt::UserRole).toString().isEmpty() && !item->isHidden());
    });
    if(m_toolbox->topLevelItemCount()>0 && m_toolbox->topLevelItem(0)->childCount()>0)
        m_toolbox->setCurrentItem(m_toolbox->topLevelItem(0)->child(0));
    m_delete=new QPushButton("Delete control",left);ll->addWidget(m_delete);
    ll->addStretch();
    // The Page tabs belong to the central workspace, not the top-level
    // designer layout. Keep the Toolbox and Properties as separate splitter
    // panes so resizing/hiding them never places tabs over either pane.
    auto* centerPane=new QWidget(split);
    centerPane->setObjectName(QStringLiteral("panelCenterPane"));
    auto* centerLayout=new QVBoxLayout(centerPane);
    centerLayout->setContentsMargins(0,0,0,0);
    centerLayout->setSpacing(3);
    auto* scroll=new QScrollArea(centerPane);
    m_scrollArea=scroll;
    scroll->setObjectName(QStringLiteral("panelCanvasScrollArea"));
    // IMPORTANT: setWidgetResizable(true) stretches PanelCanvas beyond its
    // documentSize(), making the Page dimension fields disagree with the grid.
    // Design shows exact logical pixels; Run may explicitly apply Fit Page.
    // The rest is workspace, never part of the saved project.
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll->viewport()->setAutoFillBackground(true);
    QPalette workspacePalette=scroll->viewport()->palette();
    workspacePalette.setColor(QPalette::Window,workspacePalette.color(QPalette::Window).darker(108));
    scroll->viewport()->setPalette(workspacePalette);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_canvas=new PanelCanvas(scroll);
    scroll->setWidget(m_canvas);
    scroll->viewport()->installEventFilter(this);
    centerLayout->addWidget(scroll,1);
    auto* props=new QWidget(split);
    m_propertiesPane=props;
    auto* pl=new QVBoxLayout(props);
    pl->setContentsMargins(2,2,2,2);
    m_inspectorTabs=new QTabWidget(props);
    m_inspectorTabs->setObjectName(QStringLiteral("panelInspectorTabs"));
    pl->addWidget(m_inspectorTabs,1);
    auto* details=new QWidget(m_inspectorTabs);
    auto* detailsLayout=new QVBoxLayout(details);
    detailsLayout->setContentsMargins(4,6,4,4);
    detailsLayout->setSpacing(3);
    m_propertyTitle=new QLabel(QStringLiteral("Widget properties"),details);
    detailsLayout->addWidget(m_propertyTitle);
    // Keep the inspector tab/header fixed. Scroll only the content, so the
    // expanding metadata form never squeezes its editors into unreadable rows.
    auto* propertiesScroll=new QScrollArea(details);
    propertiesScroll->setObjectName(QStringLiteral("panelPropertiesScrollArea"));
    propertiesScroll->setWidgetResizable(true);
    propertiesScroll->setFrameShape(QFrame::NoFrame);
    propertiesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    propertiesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* propertiesContent=new QWidget(propertiesScroll);
    auto* propertyLayout=new QVBoxLayout(propertiesContent);
    propertyLayout->setContentsMargins(0,2,4,4);
    propertyLayout->setSpacing(8);
    propertyLayout->setSizeConstraint(QLayout::SetMinimumSize);
    m_propertyEditor=new WidgetPropertyInspector(propertiesContent);
    propertyLayout->addWidget(m_propertyEditor);
    // Optional project-wide Value binding; normal unbound ui.set() remains the
    // default. The binding is independent of a widget factory's appearance.
    m_bindingPanel=new QWidget(propertiesContent);
    auto* bindingLayout=new QVBoxLayout(m_bindingPanel);
    bindingLayout->setContentsMargins(0,2,0,4);
    bindingLayout->addWidget(new QLabel(QStringLiteral("Value Binding (optional)"),m_bindingPanel));
    m_valueBinding=new QComboBox(m_bindingPanel);
    m_valueBinding->setObjectName(QStringLiteral("panelValueBinding"));
    bindingLayout->addWidget(m_valueBinding);
    m_bindingHint=new QLabel(QStringLiteral("None: control this widget using ui.set()."),m_bindingPanel);
    m_bindingHint->setWordWrap(true);
    bindingLayout->addWidget(m_bindingHint);
    propertyLayout->addWidget(m_bindingPanel);
    connect(m_valueBinding,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){
        if(m_refreshingBinding || m_loadingProperties || m_running || index<0 ||
           m_canvas->selectionCount()!=1)return;
        const QString chosen=m_valueBinding->itemData(index).toString();
        if(chosen==QStringLiteral("__create__")) {
            const auto type=PanelDataSchema::bindingType(m_canvas->selectedControl().type);
            editDataVariable(true,type);
            refreshValueBinding();
            return;
        }
        PanelControl d=m_canvas->selectedControl();
        if(d.id.isEmpty())return;
        if(chosen.isEmpty())d.bindings.remove(QStringLiteral("value"));
        else d.bindings.insert(QStringLiteral("value"),chosen);
        if(!m_canvas->updateSelected(d))
            QMessageBox::warning(this,QStringLiteral("Value binding"),QStringLiteral("Cannot apply this binding"));
        refreshValueBinding();
    });
    m_pageAppearanceEditor=new PageAppearanceEditor(propertiesContent);
    propertyLayout->addWidget(m_pageAppearanceEditor);
    connect(m_pageAppearanceEditor,&PageAppearanceEditor::edited,this,
            &PanelDesigner::pageAppearanceEdited);
    connect(m_pageAppearanceEditor,&PageAppearanceEditor::sizeRequested,
            this,&PanelDesigner::applyRequestedPageSize);
    connect(m_pageAppearanceEditor,&PageAppearanceEditor::fillWorkspaceRequested,this,[this]{
        // maximumViewportSize() includes space recovered if scrollbars disappear.
        const QSize available=m_scrollArea->maximumViewportSize();
        applyRequestedPageSize(QSize(qBound(320,available.width(),8192),
                                     qBound(240,available.height(),8192)));
    });
    connect(m_pageAppearanceEditor,&PageAppearanceEditor::lifecycleEdited,
            this,&PanelDesigner::panelLifecycleEdited);
    connect(m_pageAppearanceEditor,&PageAppearanceEditor::lifecycleFunctionRequested,
            this,&PanelDesigner::panelLifecycleFunctionRequested);
    m_propertiesStatus=new QLabel("Select a control to edit. Changes apply automatically.",propertiesContent);
    m_propertiesStatus->setWordWrap(true);
    m_propertiesStatus->setObjectName(QStringLiteral("panelPropertiesStatus"));
    propertyLayout->addWidget(m_propertiesStatus);
    auto* info=new QLabel("Use unique IDs in JavaScript:\nui.get('slider1')\nui.set('gauge1', 42)\nui.on('button1','clicked', fn)",propertiesContent);
    m_widgetInfo=info;
    info->setWordWrap(true);propertyLayout->addWidget(info);
    propertyLayout->addStretch();
    propertiesScroll->setWidget(propertiesContent);
    detailsLayout->addWidget(propertiesScroll,1);
    m_inspectorTabs->addTab(details,QStringLiteral("Properties"));
    auto* eventPage=new QWidget(m_inspectorTabs);
    auto* eventLayout=new QVBoxLayout(eventPage);
    eventLayout->setContentsMargins(4,6,4,4);
    eventLayout->addWidget(new QLabel(QStringLiteral("Available widget events"),eventPage));
    m_eventList=new QListWidget(eventPage);
    m_eventList->setObjectName(QStringLiteral("panelEventList"));
    m_eventList->setMaximumHeight(118);
    eventLayout->addWidget(m_eventList);
    m_eventHelp=new QLabel(QStringLiteral("Select a widget to discover its supported events."),eventPage);
    m_eventHelp->setWordWrap(true);
    eventLayout->addWidget(m_eventHelp);
    m_eventEnabled=new QCheckBox(QStringLiteral("Enable designer-managed binding"),eventPage);
    m_eventEnabled->setObjectName(QStringLiteral("panelEventEnabled"));
    eventLayout->addWidget(m_eventEnabled);
    eventLayout->addWidget(new QLabel(QStringLiteral("Handler function (main panel script)"),eventPage));
    m_eventHandler=new QLineEdit(eventPage);
    m_eventHandler->setObjectName(QStringLiteral("panelEventHandler"));
    m_eventHandler->setPlaceholderText(QStringLiteral("onStartButtonClicked"));
    m_eventHandler->setToolTip(QStringLiteral("Use a named JavaScript function defined in the panel script. "
        "The runtime calls it directly and reports the original source line on errors."));
    eventLayout->addWidget(m_eventHandler);
    m_eventChoose=new QPushButton(QStringLiteral("Choose Existing Function..."),eventPage);
    m_eventChoose->setObjectName(QStringLiteral("panelEventChoose"));
    eventLayout->addWidget(m_eventChoose);
    auto* eventButtons=new QGridLayout;
    m_eventCreate=new QPushButton(QStringLiteral("Create Handler"),eventPage);
    m_eventAssign=new QPushButton(QStringLiteral("Assign Existing"),eventPage);
    m_eventGo=new QPushButton(QStringLiteral("Go to Handler"),eventPage);
    m_eventRemove=new QPushButton(QStringLiteral("Remove Binding"),eventPage);
    m_eventCreate->setObjectName(QStringLiteral("panelEventCreate"));
    m_eventAssign->setObjectName(QStringLiteral("panelEventAssign"));
    m_eventGo->setObjectName(QStringLiteral("panelEventGo"));
    m_eventRemove->setObjectName(QStringLiteral("panelEventRemove"));
    eventButtons->addWidget(m_eventCreate,0,0);eventButtons->addWidget(m_eventAssign,0,1);
    eventButtons->addWidget(m_eventGo,1,0);eventButtons->addWidget(m_eventRemove,1,1);
    eventLayout->addLayout(eventButtons);
    m_eventNotice=new QLabel(eventPage);
    m_eventNotice->setWordWrap(true);
    m_eventNotice->setObjectName(QStringLiteral("panelEventNotice"));
    eventLayout->addWidget(m_eventNotice);
    eventLayout->addStretch();
    m_inspectorTabs->addTab(eventPage,QStringLiteral("Events"));
    // Third inspector tab is project-scoped, not specific to selected widget.
    auto* dataPage=new QWidget(m_inspectorTabs);
    auto* dataLayout=new QVBoxLayout(dataPage);
    dataLayout->setContentsMargins(6,6,6,6);
    auto* intro=new QLabel(QStringLiteral("Variables shared across all Pages. Design edits change initial values; Run values are live and read-only."),dataPage);
    intro->setWordWrap(true);dataLayout->addWidget(intro);
    m_dataTree=new QTreeWidget(dataPage);
    m_dataTree->setObjectName(QStringLiteral("panelDataVariables"));
    m_dataTree->setColumnCount(3);
    m_dataTree->setHeaderLabels({QStringLiteral("Name"),QStringLiteral("Type"),QStringLiteral("Value")});
    m_dataTree->setRootIsDecorated(false);
    m_dataTree->setAlternatingRowColors(true);
    m_dataTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dataTree->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    m_dataTree->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    m_dataTree->header()->setStretchLastSection(true);
    dataLayout->addWidget(m_dataTree,1);
    auto* actions=new QHBoxLayout;
    m_dataAdd=new QPushButton(QStringLiteral("+ New"),dataPage);
    m_dataEdit=new QPushButton(QStringLiteral("Edit Default"),dataPage);
    m_dataRemove=new QPushButton(QStringLiteral("Delete"),dataPage);
    for(auto* b:{m_dataAdd,m_dataEdit,m_dataRemove})actions->addWidget(b);
    dataLayout->addLayout(actions);
    auto* dataTip=new QLabel(QStringLiteral("Data.set(name,value) writes a value. Data.computed(target,deps,formula) creates a read-only calculation; bind its target to a display."),dataPage);
    dataTip->setWordWrap(true);
    dataLayout->addWidget(dataTip);
    m_inspectorTabs->addTab(dataPage,QStringLiteral("Panel Data"));
    connect(m_dataAdd,&QPushButton::clicked,this,[this]{editDataVariable(true);});
    connect(m_dataEdit,&QPushButton::clicked,this,[this]{editDataVariable(false);});
    connect(m_dataRemove,&QPushButton::clicked,this,&PanelDesigner::removeDataVariable);
    connect(m_dataTree,&QTreeWidget::itemDoubleClicked,this,[this]{editDataVariable(false);});
    connect(m_dataTree,&QTreeWidget::itemSelectionChanged,this,[this]{
        const bool selected=m_dataTree->currentItem()!=nullptr;
        m_dataEdit->setEnabled(!m_running && selected);
        m_dataRemove->setEnabled(!m_running && selected);
    });
    connect(m_eventList,&QListWidget::currentRowChanged,this,&PanelDesigner::selectedEventChanged);
    connect(m_eventEnabled,&QCheckBox::toggled,this,[this](bool){
        if(!m_refreshingEvents)assignEvent();
    });
    connect(m_eventAssign,&QPushButton::clicked,this,[this]{assignEvent();});
    connect(m_eventRemove,&QPushButton::clicked,this,[this]{assignEvent(true);});
    connect(m_eventChoose,&QPushButton::clicked,this,&PanelDesigner::chooseEventHandler);
    connect(m_eventCreate,&QPushButton::clicked,this,[this]{createOrGoEvent(true);});
    connect(m_eventGo,&QPushButton::clicked,this,[this]{createOrGoEvent(false);});
    split->addWidget(left);split->addWidget(centerPane);split->addWidget(props);
    split->setStretchFactor(0,0);split->setStretchFactor(1,1);split->setStretchFactor(2,0);
    left->setMinimumWidth(150);
    props->setMinimumWidth(225);
    split->setChildrenCollapsible(false);
    split->setSizes({185,900,265});
    page->addWidget(split,1);
    // Movable, non-modal Design Tools palette. No permanent second toolbar row.
    // The palette is a Qt tool window, not part of the .wfpanel document.
    m_designToolsPalette=new QDialog(this,Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    m_designToolsPalette->setObjectName(QStringLiteral("panelDesignToolsPalette"));
    m_designToolsPalette->setWindowTitle(QStringLiteral("WFScript — Design Tools"));
    m_designToolsPalette->setAttribute(Qt::WA_DeleteOnClose,false);
    auto* paletteLayout=new QVBoxLayout(m_designToolsPalette);
    paletteLayout->setContentsMargins(8,8,8,8);
    auto* editing=new QGridLayout;
    auto editorButton=[this,editing](const QString& label,int row,int column){
        auto* b=new QPushButton(label,m_designToolsPalette);
        editing->addWidget(b,row,column);return b;
    };
    m_undo=editorButton(QStringLiteral("Undo"),0,0);
    m_redo=editorButton(QStringLiteral("Redo"),0,1);
    m_copy=editorButton(QStringLiteral("Copy"),1,0);
    m_paste=editorButton(QStringLiteral("Paste"),1,1);
    m_duplicate=editorButton(QStringLiteral("Duplicate"),1,2);
    m_align=editorButton(QStringLiteral("Align ▾"),2,0);
    m_arrange=editorButton(QStringLiteral("Arrange ▾"),2,1);
    m_snap=editorButton(QStringLiteral("Snap 20px"),2,2);
    m_guides=editorButton(QStringLiteral("Smart Guides"),3,0);
    UiIcons::decorate(m_undo, "undo", QStringLiteral("Undo the last panel design action (Ctrl+Z)"));
    UiIcons::decorate(m_redo, "redo", QStringLiteral("Redo the last undone action (Ctrl+Y)"));
    UiIcons::decorate(m_copy, "copy", QStringLiteral("Copy selected widgets (Ctrl+C)"));
    UiIcons::decorate(m_paste, "paste", QStringLiteral("Paste widgets (Ctrl+V)"));
    UiIcons::decorate(m_duplicate, "duplicate", QStringLiteral("Duplicate selection (Ctrl+D)"));
    UiIcons::decorate(m_align, "align", QStringLiteral("Align or match dimensions of selected widgets"));
    UiIcons::decorate(m_arrange, "arrange", QStringLiteral("Bring controls forward or send backward"));
    UiIcons::decorate(m_snap, "grid", QStringLiteral("Snap widget edits to a 20-pixel grid"));
    m_snap->setCheckable(true);
    UiIcons::decorate(m_guides, "align", QStringLiteral("Auto-align edges and centres while moving, resizing or dropping controls (Alt temporarily bypasses)"));
    m_guides->setObjectName(QStringLiteral("panelSmartGuidesToggle"));
    m_guides->setCheckable(true);
    m_guides->setChecked(m_canvas->alignmentGuidesEnabled());
    paletteLayout->addLayout(editing);
    auto* help=new QLabel(QStringLiteral("Drag: move  ·  Blue handle: resize  ·  Ctrl/Shift: multi-select\n"
        "Alt during drag: bypass Smart Guides  ·  Ctrl+Z/Y: undo/redo\n"
        "Ctrl+C/V/D: copy/paste/duplicate"),m_designToolsPalette);
    help->setWordWrap(true);
    paletteLayout->addWidget(help);
    connect(m_designToolsToggle,&QPushButton::toggled,this,[this](bool visible){
        if(visible) {
            m_designToolsPalette->show();
            m_designToolsPalette->raise();
        } else m_designToolsPalette->hide();
    });
    connect(m_designToolsPalette,&QDialog::rejected,this,[this]{
        m_designToolsToggle->setChecked(false);
    });
    // The Page strip occupies only the CENTER splitter pane, directly above
    // the canvas scroll area; it does not extend across the Toolbox or inspector.
    // Changing a tab requests a Page switch; MainWindow owns the document model.
    auto* pageRow=new QHBoxLayout;
    pageRow->setContentsMargins(0,0,0,0);
    pageRow->setSpacing(5);
    auto* pageLabel=new QLabel(QStringLiteral("Pages:"),centerPane);
    pageRow->addWidget(pageLabel);
    m_pageTabs=new QTabBar(centerPane);
    m_pageTabs->setObjectName(QStringLiteral("panelPageTabs"));
    m_pageTabs->setDocumentMode(true);
    m_pageTabs->setExpanding(false);
    m_pageTabs->setElideMode(Qt::ElideRight);
    m_pageTabs->setUsesScrollButtons(true);
    m_pageTabs->setToolTip(QStringLiteral("Pages group widgets; switching does not restart JavaScript"));
    m_pageTabs->setTabsClosable(true);
    m_pageTabs->setContextMenuPolicy(Qt::CustomContextMenu);
    // An extra "+" tab is deliberately not a Page. Keep the last real Page
    // selected while its Add Page dialog is open (or if the user cancels it).
    pageRow->addWidget(m_pageTabs,1);
    centerLayout->insertLayout(0,pageRow);
    connect(m_pageTabs,&QTabBar::currentChanged,this,[this](int index){
        if(index<0)return;
        const QString id=m_pageTabs->tabData(index).toString();
        if(id==QStringLiteral("__add_page__")) {
            {QSignalBlocker block(m_pageTabs);m_pageTabs->setCurrentIndex(m_lastRealPageTab);}
            if(!m_running)emit addPageRequested();
            return;
        }
        if(id.isEmpty())return;
        m_lastRealPageTab=index;
        emit pageSelected(id);
    });
    connect(m_pageTabs,&QTabBar::tabCloseRequested,this,[this](int index){
        if(m_running || m_pageTabs->count()<=2 || index<0 || index>=m_pageTabs->count()-1)return;
        // Close can be clicked on a non-active Page; select it first so the
        // existing MainWindow deletion flow targets the correct stable ID.
        if(index!=m_pageTabs->currentIndex())m_pageTabs->setCurrentIndex(index);
        emit deletePageRequested();
    });
    connect(m_pageTabs,&QTabBar::customContextMenuRequested,
            this,&PanelDesigner::showPageContextMenu);
    auto* alignMenu=new QMenu(m_align);
    const QVector<QPair<QString,QString>> alignOptions={
        {"Align Left","left"},{"Align Right","right"},
        {"Align Top","top"},{"Align Bottom","bottom"},
        {"Center Horizontally","hcenter"},{"Center Vertically","vcenter"},
        {"Make Same Width","width"},{"Make Same Height","height"}
    };
    for(const auto& option:alignOptions) {
        const QString operation=option.second;
        connect(alignMenu->addAction(option.first),&QAction::triggered,this,
            [this,operation]{m_canvas->alignSelected(operation);});
    }
    m_align->setMenu(alignMenu);
    auto* arrangeMenu=new QMenu(m_arrange);
    connect(arrangeMenu->addAction("Bring to Front"),&QAction::triggered,m_canvas,&PanelCanvas::bringSelectedToFront);
    connect(arrangeMenu->addAction("Send to Back"),&QAction::triggered,m_canvas,&PanelCanvas::sendSelectedToBack);
    m_arrange->setMenu(arrangeMenu);
    connect(m_undo,&QPushButton::clicked,m_canvas->undoStack(),&QUndoStack::undo);
    connect(m_redo,&QPushButton::clicked,m_canvas->undoStack(),&QUndoStack::redo);
    connect(m_copy,&QPushButton::clicked,m_canvas,&PanelCanvas::copySelected);
    connect(m_paste,&QPushButton::clicked,m_canvas,&PanelCanvas::pasteControls);
    connect(m_duplicate,&QPushButton::clicked,m_canvas,&PanelCanvas::duplicateSelected);
    connect(m_snap,&QPushButton::toggled,m_canvas,&PanelCanvas::setSnapToGrid);
    connect(m_guides,&QPushButton::toggled,m_canvas,&PanelCanvas::setAlignmentGuidesEnabled);
    connect(m_canvas->undoStack(),&QUndoStack::canUndoChanged,m_undo,&QPushButton::setEnabled);
    connect(m_canvas->undoStack(),&QUndoStack::canRedoChanged,m_redo,&QPushButton::setEnabled);
    connect(m_canvas,&PanelCanvas::selectedChanged,this,&PanelDesigner::updateSelectionActions);
    updateSelectionActions();
    m_undo->setEnabled(false);m_redo->setEnabled(false);
    connect(m_toolboxToggle,&QPushButton::clicked,this,&PanelDesigner::setToolboxShown);
    connect(m_propertiesToggle,&QPushButton::clicked,this,&PanelDesigner::setPropertiesShown);
    connect(m_focusToggle,&QPushButton::clicked,this,&PanelDesigner::setFocusMode);
    connect(m_canvas,&PanelCanvas::documentChanged,this,&PanelDesigner::refreshPageDimensions);
    refreshPageDimensions();
    connect(fresh,&QPushButton::clicked,this,&PanelDesigner::newRequested);
    connect(load,&QPushButton::clicked,this,&PanelDesigner::loadRequested);
    connect(save,&QPushButton::clicked,this,&PanelDesigner::saveRequested);
    connect(edit,&QPushButton::clicked,this,&PanelDesigner::editScriptRequested);
    connect(libs,&QPushButton::clicked,this,&PanelDesigner::librariesRequested);
    connect(m_sequenceEditor,&QPushButton::clicked,this,&PanelDesigner::testSequenceEditorRequested);
    connect(m_run,&QPushButton::clicked,this,&PanelDesigner::runRequested);
    connect(m_stop,&QPushButton::clicked,this,&PanelDesigner::stopRequested);
    connect(m_add,&QPushButton::clicked,this,&PanelDesigner::addCurrent);
    connect(m_toolbox,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){
        if(item && !item->data(0,Qt::UserRole).toString().isEmpty())addCurrent();
    });
    connect(m_delete,&QPushButton::clicked,m_canvas,&PanelCanvas::deleteSelected);
    // Text commits on Enter/focus loss. Spin controls commit on each value
    // change, never losing an edit if a different widget is selected next.
    // Rapid spin changes are coalesced by PanelCanvas's Undo command merger.
    connect(m_propertyEditor,&WidgetPropertyInspector::edited,
            this,&PanelDesigner::scheduleProperties);
    connect(m_canvas,&PanelCanvas::selectedChanged,this,&PanelDesigner::loadProperties);
    connect(m_canvas,&PanelCanvas::selectedChanged,this,&PanelDesigner::refreshEvents);
    connect(m_canvas,&PanelCanvas::documentChanged,this,&PanelDesigner::refreshEvents);
    connect(m_canvas,&PanelCanvas::documentChanged,this,[this]{
        if(m_running)scheduleFitPage(); // Refit after switching to a differently sized Page.
    });
    connect(m_fitPageToggle,&QPushButton::toggled,this,[this]{scheduleFitPage();});
    connect(m_canvas,&PanelCanvas::documentChanged,this,[this]{ if(m_canvas->selectedControl().id.size())loadProperties(); });
    qApp->installEventFilter(this);
    setRunning(false);
    loadProperties();
    applyPageAppearance();
    refreshEvents();
}
void PanelDesigner::setPageAppearance(const PanelAppearance& appearance,
                                      const QString& pageBackground,const QString& pageName) {
    m_appearance=appearance;
    m_pageBackground=pageBackground;
    m_pageName=pageName;
    if(m_pageAppearanceEditor)m_pageAppearanceEditor->setAppearance(appearance,pageBackground,pageName);
    refreshPageDimensions();
    applyPageAppearance();
    if(m_canvas && m_canvas->selectionCount()==0)loadProperties();
}
void PanelDesigner::setPanelLifecycle(const PanelLifecycleSettings& lifecycle) {
    if(m_pageAppearanceEditor)m_pageAppearanceEditor->setLifecycle(lifecycle);
}

void PanelDesigner::applyPageAppearance() {
    if(!m_canvas || !m_scrollArea)return;
    const bool light=panelThemeIsLight(m_appearance,qApp->palette());
    m_canvas->setPageAppearance(panelCanvasColor(m_appearance,qApp->palette(),m_pageBackground),
                                 light,m_appearance.showDesignGrid);
    // Keep the surrounding workspace visually distinct from the saved Page.
    const QColor workspace=light?QColor(213,221,229):QColor(8,14,23);
    auto* viewport=m_scrollArea->viewport();
    QPalette palette=viewport->palette();palette.setColor(QPalette::Window,workspace);
    viewport->setPalette(palette);
    viewport->update();
}
bool PanelDesigner::eventFilter(QObject* watched,QEvent* event) {
    if(m_scrollArea && watched==m_scrollArea->viewport() &&
       event->type()==QEvent::Resize && m_running)scheduleFitPage();
    if(watched==qApp && event->type()==QEvent::ApplicationPaletteChange &&
       m_appearance.themeMode==QLatin1String("follow"))applyPageAppearance();
    return QWidget::eventFilter(watched,event);
}
void PanelDesigner::setPageHistoryEnabled(bool canUndo,bool canRedo) {
    m_canUndoPage=canUndo;
    m_canRedoPage=canRedo;
}
void PanelDesigner::setPageTabs(const QVector<QPair<QString,QString>>& pages,const QString& activeId) {
    QSignalBlocker block(m_pageTabs);
    while(m_pageTabs->count()>0)m_pageTabs->removeTab(m_pageTabs->count()-1);
    int active=0;
    for(const auto& page:pages) {
        const int index=m_pageTabs->addTab(page.second);
        m_pageTabs->setTabData(index,page.first);
        m_pageTabs->setTabToolTip(index,QStringLiteral("Page: %1\nID: %2\nRight-click for actions")
                                     .arg(page.second,page.first));
        if(page.first==activeId)active=index;
    }
    // Last tab is an Add action, never a document Page or a close target.
    const int plus=m_pageTabs->addTab(QStringLiteral("+"));
    m_pageTabs->setTabData(plus,QStringLiteral("__add_page__"));
    m_pageTabs->setTabToolTip(plus,QStringLiteral("Add Page"));
    m_pageTabs->setTabButton(plus,QTabBar::RightSide,nullptr);
    m_pageTabs->setTabButton(plus,QTabBar::LeftSide,nullptr);
    m_pageTabs->setTabEnabled(plus,!m_running && pages.size()<64);
    m_pageTabs->setTabsClosable(!m_running && pages.size()>1);
    // Qt styles may place the close X on either side; the action tab has none.
    m_pageTabs->setTabButton(plus,QTabBar::RightSide,nullptr);
    m_pageTabs->setTabButton(plus,QTabBar::LeftSide,nullptr);
    m_lastRealPageTab=active;
    if(!pages.isEmpty())m_pageTabs->setCurrentIndex(active);
}
QString PanelDesigner::selectedPageId() const {
    const int index=m_pageTabs->currentIndex();
    if(index<0 || index>=m_pageTabs->count()-1)return QString();
    return m_pageTabs->tabData(index).toString();
}
void PanelDesigner::showPageContextMenu(const QPoint& localPosition) {
    const int index=m_pageTabs->tabAt(localPosition);
    if(index<0)return;
    const bool plus=index==m_pageTabs->count()-1;
    QMenu menu(this);
    QAction* add=menu.addAction(UiIcons::icon("new"),QStringLiteral("New Page..."));
    add->setEnabled(!m_running && m_pageTabs->count()<=64);
    QAction* rename=nullptr;
    QAction* remove=nullptr;
    if(!plus) {
        menu.addSeparator();
        rename=menu.addAction(QStringLiteral("Rename Page..."));
        remove=menu.addAction(QStringLiteral("Delete Page..."));
        rename->setEnabled(!m_running);
        remove->setEnabled(!m_running && m_pageTabs->count()>2);
    }
    menu.addSeparator();
    QAction* undo=menu.addAction(UiIcons::icon("undo"),QStringLiteral("Undo Page Change"));
    QAction* redo=menu.addAction(UiIcons::icon("redo"),QStringLiteral("Redo Page Change"));
    undo->setEnabled(!m_running && m_canUndoPage);
    redo->setEnabled(!m_running && m_canRedoPage);
    QAction* picked=menu.exec(m_pageTabs->mapToGlobal(localPosition));
    if(!picked)return;
    if(picked==add){emit addPageRequested();return;}
    if(picked==undo){emit undoPageRequested();return;}
    if(picked==redo){emit redoPageRequested();return;}
    // Context operations on an inactive tab apply to the tab clicked.
    if(index!=m_pageTabs->currentIndex())m_pageTabs->setCurrentIndex(index);
    if(picked==rename)emit renamePageRequested();
    else if(picked==remove)emit deletePageRequested();
}

void PanelDesigner::refreshPageDimensions() {
    if(m_canvas && m_pageAppearanceEditor)
        m_pageAppearanceEditor->setPageSize(m_canvas->documentSize(),m_canvas->minimumContentSize());
}

bool PanelDesigner::setPanelSize(const QSize& size) {
    const bool accepted=m_canvas->setDocumentSize(size);
    if(accepted)refreshPageDimensions();
    return accepted;
}

void PanelDesigner::applyRequestedPageSize(const QSize& size) {
    if(m_running || !m_canvas || !m_pageAppearanceEditor)return;
    if(setPanelSize(size))return;
    const QSize minimum=m_canvas->minimumContentSize();
    // Restore the actual dimensions; never leave an invalid size displayed.
    refreshPageDimensions();
    m_pageAppearanceEditor->showSizeError(QStringLiteral(
        "Cannot shrink the Page to %1 × %2. Existing widgets need at least %3 × %4 pixels. "
        "Move/resize them first, then try again.")
        .arg(size.width()).arg(size.height()).arg(minimum.width()).arg(minimum.height()));
}

bool PanelDesigner::toolboxShown() const {
    // isHidden() tracks this widget's own state even when its parent tab is inactive.
    return m_toolboxPane && !m_toolboxPane->isHidden();
}
bool PanelDesigner::propertiesShown() const {
    return m_propertiesPane && !m_propertiesPane->isHidden();
}
void PanelDesigner::setToolboxShown(bool shown) {
    if (m_focusMode) setFocusMode(false);
    m_toolboxPane->setVisible(shown);
    m_toolboxToggle->setChecked(shown);
}
void PanelDesigner::setPropertiesShown(bool shown) {
    if (m_focusMode) setFocusMode(false);
    m_propertiesPane->setVisible(shown);
    m_propertiesToggle->setChecked(shown);
}
void PanelDesigner::setFocusMode(bool focused) {
    if(m_focusMode==focused) return;
    if(focused) {
        m_beforeFocusToolbox=toolboxShown();
        m_beforeFocusProperties=propertiesShown();
    }
    m_focusMode=focused;
    m_toolboxPane->setVisible(focused ? false : m_beforeFocusToolbox);
    m_propertiesPane->setVisible(focused ? false : m_beforeFocusProperties);
    m_toolboxToggle->setChecked(toolboxShown());
    m_propertiesToggle->setChecked(propertiesShown());
    m_focusToggle->setChecked(focused);
    scheduleFitPage();
    // The palette is independent of the canvas layout; no geometry change.
}
void PanelDesigner::resetUiLayout() {
    if(m_focusMode) setFocusMode(false);
    setToolboxShown(true);
    setPropertiesShown(true);
    m_splitter->setSizes({185,900,265});
}
// Schedule after split-pane, tabs and main-window focus changes have settled.
// Never store the scale in PanelProject or modify a control definition.
void PanelDesigner::scheduleFitPage() {
    if(m_fitPageScheduled)return;
    m_fitPageScheduled=true;
    QTimer::singleShot(0,this,[this]{
        m_fitPageScheduled=false;
        updateFitPage();
    });
}

void PanelDesigner::updateFitPage() {
    if(!m_canvas || !m_scrollArea)return;
    qreal scale=1.0;
    if(m_running && m_fitPageToggle && m_fitPageToggle->isChecked()) {
        const QSize viewport=m_scrollArea->viewport()->size();
        const QSize document=m_canvas->documentSize();
        if(viewport.width()>32 && viewport.height()>32 &&
           document.width()>0 && document.height()>0) {
            const qreal horizontal=qreal(viewport.width()-4)/document.width();
            const qreal vertical=qreal(viewport.height()-4)/document.height();
            scale=qBound(0.15,qMin(horizontal,vertical),1.0);
        }
    }
    m_canvas->setPresentationScale(scale);
    if(m_fitPagePercent)m_fitPagePercent->setText(QStringLiteral("%1%").arg(qRound(100*scale)));
}

void PanelDesigner::setOperatorView(bool enabled) {
    if(m_operatorView == enabled) return;
    m_operatorView=enabled;
    // Do not reparent or recreate any Page widget: only the development toolbar
    // is hidden. Existing Page tabs stay available while the experiment runs.
    m_editorToolbar->setVisible(!enabled);
    if(auto* layout=qobject_cast<QVBoxLayout*>(this->layout()))
        layout->setContentsMargins(enabled ? 2 : 6,enabled ? 2 : 6,
                                   enabled ? 2 : 6,enabled ? 2 : 6);
    scheduleFitPage();
}

void PanelDesigner::setFitPageEnabled(bool enabled) {
    if(m_fitPageToggle) m_fitPageToggle->setChecked(enabled);
}

bool PanelDesigner::fitPageEnabled() const {
    return m_fitPageToggle && m_fitPageToggle->isChecked();
}

void PanelDesigner::setRunning(bool running) {
    m_running=running;
    m_canvas->setDesignMode(!running);
    m_fitPageToggle->setEnabled(running);
    m_fitPagePercent->setVisible(running);
    scheduleFitPage();
    m_run->setEnabled(!running);
    m_stop->setEnabled(running);
    m_add->setEnabled(!running && m_toolbox->currentItem() &&
        !m_toolbox->currentItem()->data(0,Qt::UserRole).toString().isEmpty() &&
        !m_toolbox->currentItem()->isHidden());
    m_delete->setEnabled(!running);
    m_toolbox->setEnabled(!running);
    m_undo->setEnabled(!running && m_canvas->undoStack()->canUndo());
    m_redo->setEnabled(!running && m_canvas->undoStack()->canRedo());
    m_snap->setEnabled(!running);
    m_guides->setEnabled(!running);
    updateSelectionActions();
    m_new->setEnabled(!running);m_open->setEnabled(!running);m_save->setEnabled(!running);
    m_examples->setEnabled(!running);
    m_sequenceEditor->setEnabled(!running);
    // While Running, Pages remain navigable, but Add/Delete/Rename are locked.
    m_pageTabs->setTabsClosable(!running && m_pageTabs->count()>2);
    const int plus=m_pageTabs->count()-1;
    if(plus>=0) {
        m_pageTabs->setTabEnabled(plus,!running && m_pageTabs->count()<=64);
        m_pageTabs->setTabButton(plus,QTabBar::RightSide,nullptr);
        m_pageTabs->setTabButton(plus,QTabBar::LeftSide,nullptr);
    }
    m_propertyEditor->setEditable(!running && m_canvas->selectionCount()==1);
    m_pageAppearanceEditor->setEditable(!running);
    refreshDataTab();
    refreshValueBinding();
    refreshEvents();
    // Run/Design guidance is available on buttons and the palette; no fixed hint row.
    m_designToolsToggle->setEnabled(!running);
    if(running)m_designToolsToggle->setChecked(false);
}
void PanelDesigner::addCurrent() {
    if (m_running || !m_toolbox->currentItem())return;
    const QString type=m_toolbox->currentItem()->data(0,Qt::UserRole).toString();
    if(type.isEmpty())return; // selected category heading is not a widget
    const auto* factory=WidgetRegistry::instance().factory(type);
    if(!factory)return;
    PanelControl d=factory->defaultDefinition(m_canvas->uniqueId(type));
    const int offset=int(m_canvas->toJson().size()%8)*22;
    d.geometry.moveTopLeft(QPoint(30+offset,30+offset));
    if(!m_canvas->addControl(d)) QMessageBox::warning(this,"Add control","Unable to add widget");
}
void PanelDesigner::loadProperties() {
    if(m_loadingProperties)return;
    const PanelControl d=m_canvas->selectedControl();
    const bool editable=!m_running && !d.id.isEmpty() && m_canvas->selectionCount()==1;
    m_loadingProperties=true;
    const bool showPage=m_canvas->selectionCount()==0;
    m_propertyTitle->setText(showPage?QStringLiteral("Panel / Page Properties"):
                            QStringLiteral("Widget properties"));
    m_pageAppearanceEditor->setVisible(showPage);
    if(showPage)refreshPageDimensions();
    m_propertyEditor->setVisible(!showPage);
    m_widgetInfo->setVisible(!showPage);
    refreshValueBinding();
    m_pageAppearanceEditor->setEditable(!m_running);
    m_propertyEditor->setControl(d,editable);
    m_loadingProperties=false;
    if(!editable) {
        m_propertiesStatus->setText(m_canvas->selectionCount()>1
            ? QStringLiteral("%1 controls selected. Use Align, Arrange, or drag the group.").arg(m_canvas->selectionCount())
            : QStringLiteral("Page dimensions, appearance and Panel lifecycle are editable here."));
    } else {
        m_propertiesStatus->setText(QStringLiteral("Changes apply automatically. Ctrl+Z to undo."));
    }
}
void PanelDesigner::scheduleProperties() {
    if(m_loadingProperties || m_running || m_canvas->selectionCount()!=1)return;
    applyProperties();
}
void PanelDesigner::applyProperties() {
    if(m_loadingProperties || m_running || m_canvas->selectionCount()!=1)return;
    PanelControl d=m_canvas->selectedControl();
    if(d.id.isEmpty())return;
    d=m_propertyEditor->readControl(d);
    if(!m_canvas->updateSelected(d)) {
        m_propertiesStatus->setText(QStringLiteral("Invalid properties: ID must be unique, minimum < maximum, and bounds must be valid. Edit the fields to correct them."));
        m_propertiesStatus->setToolTip(QStringLiteral("Your changes were not applied. Correct the invalid field, or select the control again to reload saved values."));
        return;
    }
    m_propertiesStatus->setToolTip({});
}

void PanelDesigner::updateSelectionActions() {
    if (!m_canvas) return;
    const int count=m_canvas->selectionCount();
    const bool editing=!m_running;
    if(m_copy)m_copy->setEnabled(editing && count>0);
    if(m_duplicate)m_duplicate->setEnabled(editing && count>0);
    if(m_align)m_align->setEnabled(editing && count>1);
    if(m_arrange)m_arrange->setEnabled(editing && count>0);
    if(m_paste)m_paste->setEnabled(editing);
    if(m_delete)m_delete->setEnabled(editing && count>0);
    // Property fields are managed by loadProperties(), not an Apply button.
}

void PanelDesigner::setPanelData(const QJsonArray& variables) {
    m_panelData=variables;
    m_runtimeData.clear();
    for(const auto& entry:variables) {
        const auto d=entry.toObject();
        m_runtimeData.insert(d.value(QStringLiteral("name")).toString(),PanelDataSchema::initialValue(d));
    }
    refreshDataTab();
    refreshValueBinding();
}
void PanelDesigner::setPanelDataRuntimeValue(const QString& name,const QVariant& value) {
    // Only project-defined names are allowed. Recover safely if an edit was
    // accepted before the runtime cache was seeded (e.g. a newly created
    // variable). Ignore unknown worker messages rather than adding ghost rows.
    if(!m_runtimeData.contains(name)) {
        bool defined=false;
        for(const auto& entry:m_panelData) {
            if(entry.toObject().value(QStringLiteral("name")).toString()==name) {
                defined=true;break;
            }
        }
        if(!defined)return;
    }
    m_runtimeData.insert(name,value);
    if(!m_running || !m_dataTree)return;
    // A running simulation may publish several variables every 250 ms.
    // Update only the affected row: clearing/rebuilding the QTreeWidget on
    // each tick loses selection, scroll position and can visibly flicker.
    const QString display=value.metaType().id()==QMetaType::Bool
        ?(value.toBool()?QStringLiteral("true"):QStringLiteral("false"))
        :value.toString();
    for(int i=0;i<m_dataTree->topLevelItemCount();++i) {
        QTreeWidgetItem* row=m_dataTree->topLevelItem(i);
        if(row->data(0,Qt::UserRole).toString()==name) {
            if(row->text(2)!=display)row->setText(2,display);
            break;
        }
    }
}
void PanelDesigner::refreshDataTab() {
    if(!m_dataTree)return;
    m_dataTree->headerItem()->setText(2,m_running
        ?QStringLiteral("Live Value"):QStringLiteral("Initial Value"));
    const QString prior=m_dataTree->currentItem()?m_dataTree->currentItem()->data(0,Qt::UserRole).toString():QString();
    QSignalBlocker block(m_dataTree);
    m_dataTree->clear();
    for(const auto& entry:m_panelData) {
        const auto d=entry.toObject();
        const QString name=d.value(QStringLiteral("name")).toString();
        const QString type=d.value(QStringLiteral("type")).toString();
        const QVariant value=m_running?m_runtimeData.value(name):PanelDataSchema::initialValue(d);
        auto* row=new QTreeWidgetItem(m_dataTree,{name,type,
            type==QStringLiteral("boolean")?(value.toBool()?QStringLiteral("true"):QStringLiteral("false")):
            value.toString()});
        row->setData(0,Qt::UserRole,name);
        if(name==prior)m_dataTree->setCurrentItem(row);
    }
    m_dataAdd->setEnabled(!m_running && m_panelData.size()<128);
    const bool hasSelection=m_dataTree->currentItem()!=nullptr;
    m_dataEdit->setEnabled(!m_running && hasSelection);
    m_dataRemove->setEnabled(!m_running && hasSelection);
}
void PanelDesigner::refreshValueBinding() {
    if(!m_valueBinding || !m_canvas)return;
    const PanelControl d=m_canvas->selectedControl();
    const QString type=(m_canvas->selectionCount()==1)?PanelDataSchema::bindingType(d.type):QString();
    m_bindingPanel->setVisible(!type.isEmpty());
    if(type.isEmpty())return;
    m_refreshingBinding=true;
    QSignalBlocker block(m_valueBinding);
    m_valueBinding->clear();
    m_valueBinding->addItem(QStringLiteral("None — use ui.set()"),QString());
    for(const auto& v:m_panelData) {
        const auto entry=v.toObject();
        if(entry.value(QStringLiteral("type")).toString()==type)
            m_valueBinding->addItem(entry.value(QStringLiteral("name")).toString(),entry.value(QStringLiteral("name")).toString());
    }
    m_valueBinding->addItem(QStringLiteral("+ Create New Variable..."),QStringLiteral("__create__"));
    const QString current=d.bindings.value(QStringLiteral("value")).toString();
    const int selection=m_valueBinding->findData(current);
    m_valueBinding->setCurrentIndex(selection<0?0:selection);
    m_valueBinding->setEnabled(!m_running);
    m_bindingHint->setText(current.isEmpty()?QStringLiteral("Unbound: this widget uses ui.set()."):
        QStringLiteral("Bound to %1. Update it with Data.set(). Direct ui.set() is disabled for this Value.").arg(current));
    m_refreshingBinding=false;
}
void PanelDesigner::editDataVariable(bool create,const QString& forcedType) {
    if(m_running || (create && m_panelData.size()>=128))return;
    QString oldName;
    QJsonObject original;
    if(!create) {
        if(!m_dataTree->currentItem())return;
        oldName=m_dataTree->currentItem()->data(0,Qt::UserRole).toString();
        for(const auto& item:m_panelData)if(item.toObject().value(QStringLiteral("name")).toString()==oldName)
            original=item.toObject();
        if(original.isEmpty())return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(create?QStringLiteral("Create Panel Data Variable"):QStringLiteral("Edit Panel Data Default"));
    auto* layout=new QFormLayout(&dialog);
    auto* nameEdit=new QLineEdit(create?QString():oldName,&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("Motor.Speed"));
    nameEdit->setEnabled(create); // rename could silently break references in JS; explicit deletion instead
    layout->addRow(QStringLiteral("Name"),nameEdit);
    auto* typeSelect=new QComboBox(&dialog);
    typeSelect->addItem(QStringLiteral("Number"),QStringLiteral("number"));
    typeSelect->addItem(QStringLiteral("Boolean"),QStringLiteral("boolean"));
    typeSelect->addItem(QStringLiteral("Text"),QStringLiteral("text"));
    const QString type=create?(forcedType.isEmpty()?QStringLiteral("number"):forcedType):original.value(QStringLiteral("type")).toString();
    typeSelect->setCurrentIndex(qMax(0,typeSelect->findData(type)));
    typeSelect->setEnabled(create && forcedType.isEmpty());
    layout->addRow(QStringLiteral("Type"),typeSelect);
    auto* initial=new QLineEdit(&dialog);
    initial->setText(create?(type==QStringLiteral("boolean")?QStringLiteral("false"):
        type==QStringLiteral("number")?QStringLiteral("0"):QString()):
        PanelDataSchema::initialValue(original).toString());
    layout->addRow(QStringLiteral("Initial value"),initial);
    if(create && forcedType.isEmpty())
        connect(typeSelect,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[typeSelect,initial](int){
            const QString current=typeSelect->currentData().toString();
            initial->setText(current==QStringLiteral("boolean")?QStringLiteral("false"):
                current==QStringLiteral("number")?QStringLiteral("0"):QString());
        });
    auto* message=new QLabel(QStringLiteral("Initial values are saved in the .wfpanel; Run-time values are not."),&dialog);
    message->setWordWrap(true);layout->addRow(message);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addRow(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        const QString name=nameEdit->text().trimmed();
        const QString chosenType=typeSelect->currentData().toString();
        if(!PanelDataSchema::validName(name)) {
            QMessageBox::warning(&dialog,QStringLiteral("Invalid name"),QStringLiteral("Use a name such as Motor.Speed (letters, numbers, underscores and dots)."));return;
        }
        if(create)for(const auto& v:m_panelData)if(v.toObject().value(QStringLiteral("name")).toString()==name) {
            QMessageBox::warning(&dialog,QStringLiteral("Duplicate name"),name+QStringLiteral(" already exists."));return;
        }
        QJsonValue value;
        if(chosenType==QStringLiteral("number")) {
            bool ok=false;const double n=initial->text().trimmed().toDouble(&ok);
            if(!ok || !std::isfinite(n)) {
                QMessageBox::warning(&dialog,QStringLiteral("Invalid number"),QStringLiteral("Enter a finite numeric initial value."));return;
            }
            value=n;
        } else if(chosenType==QStringLiteral("boolean")) {
            const QString str=initial->text().trimmed().toLower();
            if(str!=QStringLiteral("true") && str!=QStringLiteral("false")) {
                QMessageBox::warning(&dialog,QStringLiteral("Invalid Boolean"),QStringLiteral("Enter true or false."));return;
            }
            value=(str==QStringLiteral("true"));
        } else {
            if(initial->text().size()>4096) {
                QMessageBox::warning(&dialog,QStringLiteral("Text too long"),QStringLiteral("Maximum 4096 characters."));return;
            }
            value=initial->text();
        }
        original=QJsonObject{{QStringLiteral("name"),name},{QStringLiteral("type"),chosenType},
                              {QStringLiteral("initial"),value}};
        dialog.accept();
    });
    if(dialog.exec()!=QDialog::Accepted)return;
    QJsonArray next;
    if(create) {next=m_panelData;next.append(original);} else {
        for(const auto& v:m_panelData)
            next.append(v.toObject().value(QStringLiteral("name")).toString()==oldName?QJsonValue(original):v);
    }
    m_panelData=next;
    emit panelDataEdited(next);
    refreshDataTab();refreshValueBinding();
    for(int i=0;i<m_dataTree->topLevelItemCount();++i)
        if(m_dataTree->topLevelItem(i)->data(0,Qt::UserRole).toString()==original.value(QStringLiteral("name")).toString())
            m_dataTree->setCurrentItem(m_dataTree->topLevelItem(i));
    if(create && !forcedType.isEmpty() && m_canvas->selectionCount()==1) {
        PanelControl d=m_canvas->selectedControl();
        d.bindings.insert(QStringLiteral("value"),original.value(QStringLiteral("name")).toString());
        m_canvas->updateSelected(d);
        refreshValueBinding();
    }
}
void PanelDesigner::removeDataVariable() {
    if(m_running || !m_dataTree->currentItem())return;
    const QString name=m_dataTree->currentItem()->data(0,Qt::UserRole).toString();
    if(QMessageBox::question(this,QStringLiteral("Delete Panel Data variable"),
         QStringLiteral("Delete %1? This is refused if a widget still has a binding to it. JavaScript references cannot be automatically updated.").arg(name),
         QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    QJsonArray next;
    for(const auto& v:m_panelData)if(v.toObject().value(QStringLiteral("name")).toString()!=name)next.append(v);
    m_panelData=next;
    emit panelDataEdited(next);
    refreshDataTab();refreshValueBinding();
}

void PanelDesigner::setPanelScriptSource(const QString& source) {
    m_panelScriptSource=source;
    refreshEvents();
}

void PanelDesigner::refreshEvents() {
    if(!m_eventList)return;
    const QString prior=m_eventList->currentItem()
        ?m_eventList->currentItem()->data(Qt::UserRole).toString():QString();
    const PanelControl control=m_canvas->selectedControl();
    const bool selected=m_canvas->selectionCount()==1 && !control.id.isEmpty();
    m_refreshingEvents=true;
    m_eventList->clear();
    if(selected) {
        for(const auto& definition:PanelEventCatalog::forType(control.type)) {
            auto* item=new QListWidgetItem(definition.signature,m_eventList);
            item->setData(Qt::UserRole,definition.name);
            const QJsonObject binding=control.events.value(definition.name).toObject();
            if(!binding.isEmpty())item->setText(definition.signature+
                (binding.value(QStringLiteral("enabled")).toBool()?QStringLiteral("  • Bound"):QStringLiteral("  • Disabled")));
        }
    }
    for(int i=0;i<m_eventList->count();++i)
        if(m_eventList->item(i)->data(Qt::UserRole).toString()==prior) {
            m_eventList->setCurrentRow(i);break;
        }
    if(m_eventList->count()>0 && m_eventList->currentRow()<0)m_eventList->setCurrentRow(0);
    m_refreshingEvents=false;
    selectedEventChanged();
}

void PanelDesigner::selectedEventChanged() {
    if(m_refreshingEvents)return;
    const PanelControl control=m_canvas->selectedControl();
    const QString event=m_eventList->currentItem()
        ?m_eventList->currentItem()->data(Qt::UserRole).toString():QString();
    const bool valid=!m_running && m_canvas->selectionCount()==1 && !event.isEmpty();
    m_refreshingEvents=true;
    QString description;
    for(const auto& def:PanelEventCatalog::forType(control.type))
        if(def.name==event)description=def.description;
    m_eventHelp->setText(event.isEmpty()
        ?QStringLiteral("Select one interactive widget to see its supported events. Display-only widgets have no events.")
        :description);
    const QJsonObject binding=control.events.value(event).toObject();
    const QString existing=binding.value(QStringLiteral("handler")).toString();
    QString suggestion=QStringLiteral("on")+control.id.left(1).toUpper()+control.id.mid(1)
        +event.left(1).toUpper()+event.mid(1);
    suggestion.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_$]")),QStringLiteral("_"));
    m_eventHandler->setText(existing.isEmpty()?suggestion:existing);
    m_eventEnabled->setChecked(!existing.isEmpty() && binding.value(QStringLiteral("enabled")).toBool());
    m_refreshingEvents=false;
    for(auto* button:{m_eventCreate,m_eventAssign,m_eventGo,m_eventRemove})button->setEnabled(valid);
    m_eventEnabled->setEnabled(valid && !existing.isEmpty());
    m_eventChoose->setEnabled(valid);
    m_eventHandler->setEnabled(valid);
    const bool scriptManaged=m_panelScriptSource.contains(QRegularExpression(
        QStringLiteral("\\bui\\.on\\s*\\(\\s*['\"]%1['\"]\\s*,\\s*['\"]%2['\"]")
          .arg(QRegularExpression::escape(control.id),QRegularExpression::escape(event))));
    m_eventNotice->setText(!valid?QStringLiteral("Select one interactive control in Design mode.")
        :scriptManaged ? QStringLiteral("A literal ui.on() registration also appears in the panel script. "
                                        "Adding a designer binding will invoke both handlers. Reuse carefully.")
        :existing.isEmpty()?QStringLiteral("No designer binding assigned. Create or assign a named global function.")
        :QStringLiteral("Handler: %1 — %2. Disabling keeps its source code.")
             .arg(existing,binding.value(QStringLiteral("enabled")).toBool()?QStringLiteral("enabled"):QStringLiteral("disabled")));
}

void PanelDesigner::assignEvent(bool remove) {
    if(m_refreshingEvents || m_running || m_canvas->selectionCount()!=1 || !m_eventList->currentItem())return;
    const QString event=m_eventList->currentItem()->data(Qt::UserRole).toString();
    const QString handler=remove?QString():m_eventHandler->text().trimmed();
    if(!remove && !PanelEventCatalog::validHandler(handler)) {
        m_eventNotice->setText(QStringLiteral("Enter a valid JavaScript global function name (letters, digits, _ or $)."));
        return;
    }
    const bool exists=m_canvas->selectedControl().events.contains(event);
    const bool enable=!remove && (!exists || m_eventEnabled->isChecked());
    if(!m_canvas->setSelectedEventBinding(event,handler,enable)) {
        m_eventNotice->setText(QStringLiteral("Unable to assign this event handler."));
        return;
    }
    refreshEvents();
}

void PanelDesigner::createOrGoEvent(bool create) {
    if(m_running || m_canvas->selectionCount()!=1 || !m_eventList->currentItem())return;
    const QString handler=m_eventHandler->text().trimmed();
    if(!PanelEventCatalog::validHandler(handler)) {
        m_eventNotice->setText(QStringLiteral("Specify a valid named JavaScript function first."));return;
    }
    const QString event=m_eventList->currentItem()->data(Qt::UserRole).toString();
    const QString id=m_canvas->selectedControl().id;
    if(create) {
        if(!m_canvas->setSelectedEventBinding(event,handler,true))return;
    }
    emit eventHandlerRequested(id,event,handler,create);
}

void PanelDesigner::chooseEventHandler() {
    if(m_running || m_canvas->selectionCount()!=1 || !m_eventList->currentItem())return;
    // Discovery is intentionally conservative: named declarations in the main
    // panel source. The user can still type any named global function manually.
    static const QRegularExpression declaration(
        QStringLiteral("\\bfunction\\s+([A-Za-z_$][A-Za-z0-9_$]*)\\s*\\("));
    QStringList names;
    auto it=declaration.globalMatch(m_panelScriptSource);
    while(it.hasNext()) {
        const QString name=it.next().captured(1);
        if(PanelEventCatalog::validHandler(name) && !names.contains(name))names.append(name);
    }
    if(names.isEmpty()) {
        m_eventNotice->setText(QStringLiteral("No named main-script functions were found. "
                             "Use Create Handler or enter a function name manually."));
        return;
    }
    names.sort(Qt::CaseInsensitive);
    bool accepted=false;
    const QString chosen=QInputDialog::getItem(this,QStringLiteral("Assign Existing Handler"),
        QStringLiteral("Select a named JavaScript function:"),names,0,false,&accepted);
    if(!accepted || chosen.isEmpty())return;
    m_eventHandler->setText(chosen);
    assignEvent();
}
