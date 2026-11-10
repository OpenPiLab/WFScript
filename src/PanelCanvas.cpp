#include "PanelCanvas.h"
#include "PanelEventCatalog.h"
#include "WaveformPlot.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/RunInformationWidget.h"
#include "RunInformationSchema.h"
#include "widgets/WidgetDocumentCodec.h"
#include <QApplication>
#include <QAction>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QUndoCommand>
#include <QUndoStack>
#include <QDateTime>
#include <QJsonDocument>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFrame>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QRubberBand>
#include <QSignalBlocker>
#include <QSet>
#include <QSizePolicy>
#include <algorithm>
#include <utility>
#include <QSlider>
#include <QStyle>
#include <cmath>

namespace {
constexpr auto kDragMime = "application/x-wfscript-control";
constexpr auto kClipboardMime = "application/x-wfscript-panel-controls+json";
constexpr int kGrid = 20;

}

// QUndoStack command stores a complete design-time snapshot. Each drag/resize
// is one undo step, rather than a command for every mouse-move event.
class PanelCanvasHistoryCommand final : public QUndoCommand {
public:
    PanelCanvasHistoryCommand(PanelCanvas* canvas, const PanelCanvas::Snapshot& before,
                              const PanelCanvas::Snapshot& after, const QString& description)
        : QUndoCommand(description), m_canvas(canvas), m_before(before), m_after(after),
          m_mergeable(description==QStringLiteral("Edit properties")),
          m_timestampMs(QDateTime::currentMSecsSinceEpoch()) {}
    void undo() override { m_canvas->restoreSnapshot(m_before); }
    int id() const override { return m_mergeable ? 5613 : -1; }
    bool mergeWith(const QUndoCommand* other) override {
        if (!m_mergeable || other->id()!=id()) return false;
        const auto* next=static_cast<const PanelCanvasHistoryCommand*>(other);
        // Never merge edits to different controls, even when made quickly.
        if (m_after.selectedIds!=next->m_after.selectedIds ||
            m_after.selectedIds.size()!=1 ||
            next->m_timestampMs-m_timestampMs>600 ||
            next->m_timestampMs<m_timestampMs) return false;
        m_after=next->m_after;
        m_timestampMs=next->m_timestampMs;
        return true;
    }
    void redo() override {
        if (m_firstRedo) { m_firstRedo=false; return; }
        m_canvas->restoreSnapshot(m_after);
    }
private:
    PanelCanvas* m_canvas;
    PanelCanvas::Snapshot m_before;
    PanelCanvas::Snapshot m_after;
    bool m_firstRedo = true;
    bool m_mergeable = false;
    qint64 m_timestampMs = 0;
};

QStringList PanelCanvas::supportedTypes() {
    return WidgetRegistry::instance().typeIds();
}
PanelCanvas::PanelCanvas(QWidget* parent) : QWidget(parent) {
    setObjectName("panelCanvas");
    setFocusPolicy(Qt::StrongFocus);
    m_undoStack = new QUndoStack(this);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(m_documentSize);
    setAcceptDrops(true);
    setAutoFillBackground(true);
    m_marquee=new QRubberBand(QRubberBand::Rectangle,this);
    m_marquee->setObjectName(QStringLiteral("panelSelectionMarquee"));
    m_marquee->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_marquee->hide();
    m_outline=new QRubberBand(QRubberBand::Rectangle,this);
    m_outline->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_resize=new QFrame(this);
    m_resize->setFixedSize(13,13);
    m_resize->setCursor(Qt::SizeFDiagCursor);
    m_resize->setStyleSheet("background:#38a5e4;border:1px solid white;");
    m_resize->installEventFilter(this);
    syncSelection();
    m_lastSnapshot=snapshot();
}

QSize PanelCanvas::minimumContentSize() const {
    QSize minimum(320, 240);
    for (const auto& item : m_items) {
        const QRect geometry = item.widget->geometry();
        minimum.setWidth(qMax(minimum.width(), geometry.x() + geometry.width()));
        minimum.setHeight(qMax(minimum.height(), geometry.y() + geometry.height()));
    }
    return minimum;
}

bool PanelCanvas::setDocumentSize(const QSize& requested) {
    // The widget and saved document must have precisely the same dimensions.
    // Never clip an existing control when shrinking the document.
    if (requested.width() < 320 || requested.height() < 240 ||
        requested.width() > 8192 || requested.height() > 8192 ||
        requested.width() < minimumContentSize().width() ||
        requested.height() < minimumContentSize().height()) return false;
    if (requested == m_documentSize) return true;
    m_documentSize = requested;
    setFixedSize(m_documentSize);
    updateGeometry();
    emit documentChanged();
    recordChange(QStringLiteral("Resize panel"));
    return true;
}

void PanelCanvas::growDocumentToFit(const QRect& geometry) {
    // If code or a drag/resize moves a control past the panel boundary,
    // enlarge the ACTUAL widget and saved document together so that the
    // control is not lost after reopening the project.
    const QSize required(qMax(m_documentSize.width(), geometry.x() + geometry.width()),
                         qMax(m_documentSize.height(), geometry.y() + geometry.height()));
    if (required.width() > 8192 || required.height() > 8192 ||
        required == m_documentSize) return;
    m_documentSize = required;
    setFixedSize(m_documentSize);
    updateGeometry();
    emit documentChanged();
}

void PanelCanvas::setDesignMode(bool design) {
    if (m_designMode==design) return;
    m_designMode=design;
    if (!design) { m_marqueeSelecting=false; m_marquee->hide(); m_marqueeBase.clear(); }
    // Display-only controls remain enabled so Qt does not grey out their text/graphics.
    // Mouse interactions during design are intercepted by the event filter instead.
    for (auto& item:m_items) item.widget->setEnabled(true);
    if (!design) { m_dragging=false; m_resizing=false; }
    syncSelection();update();
}

void PanelCanvas::synchronizeRunInformationWidgets(const QString& originId) {
    const auto update=[this,&originId](const Item& item){
        if(item.def.id==originId)return; // keep current editor focus/caret intact
        if(auto* form=qobject_cast<RunInformationWidget*>(item.widget))
            form->setValues(m_runInformationInput); // signal-blocked: no recursive edits
    };
    for(const auto& item:m_items)update(item);
    for(auto it=m_inactivePages.cbegin();it!=m_inactivePages.cend();++it)
        for(const auto& item:it.value().items)update(item);
}
void PanelCanvas::setRunInformationDefinition(const QJsonObject& schema) {
    // One project-owned definition and one pending operator-value model. A
    // changed schema keeps values with existing IDs; new fields get defaults.
    const QVariantMap previous=m_runInformationInput;
    m_runInformationDefinition=schema;
    m_runInformationInput=RunInformationSchema::defaults(schema);
    for(auto it=previous.cbegin();it!=previous.cend();++it)
        if(m_runInformationInput.contains(it.key()))m_runInformationInput.insert(it.key(),it.value());
    const auto update=[this,&schema](QWidget* widget){
        if(auto* form=qobject_cast<RunInformationWidget*>(widget)) {
            form->setSchema(schema);
            for(QWidget* child:form->findChildren<QWidget*>())child->installEventFilter(this);
        }
    };
    for(auto& item:m_items)update(item.widget);
    for(auto it=m_inactivePages.begin();it!=m_inactivePages.end();++it)
        for(auto& item:it.value().items)update(item.widget);
    synchronizeRunInformationWidgets();
}

QWidget* PanelCanvas::createWidget(const PanelControl& def) {
    const auto* factory=WidgetRegistry::instance().factory(def.type);
    if(!factory)return nullptr;
    QWidget* widget=factory->create(def,this,[this,id=def.id](const QString& eventName,const QVariant& value){
        if(m_designMode)return;
        if(eventName==QStringLiteral("runInformationChanged")) {
            // The editable form is a VIEW. All other forms, including those on
            // hidden Pages, receive the same shared pending values immediately.
            const QVariantMap submitted=value.toMap();
            for(auto it=submitted.cbegin();it!=submitted.cend();++it)
                if(m_runInformationInput.contains(it.key()))m_runInformationInput.insert(it.key(),it.value());
            synchronizeRunInformationWidgets(id);
            emit controlEvent(id,eventName,m_runInformationInput);
            return;
        }
        // The factory supplies meaningful values; Canvas does not need to
        // know individual widget types or their internal 0-1000 slider scales.
        if(PanelEventCatalog::carriesPersistentState(eventName))
            if(Item* item=findAnyId(id))
                if(item->def.type!=QStringLiteral("experiment_table"))item->def.value=value.toDouble();
        emit controlEvent(id,eventName,value);
    });
    if(!widget)return nullptr;
    if(auto* form=qobject_cast<RunInformationWidget*>(widget)) {
        form->setSchema(m_runInformationDefinition);
        form->setValues(m_runInformationInput);
    }
    widget->setObjectName(def.id);
    widget->setGeometry(def.geometry);
    widget->setMinimumSize(25,22);
    widget->installEventFilter(this);
    // Compound instruments have QDoubleSpinBox/QLineEdit/QComboBox children.
    // Their mouse events must select/drag the *whole* designer widget.
    // In Run mode, the filter passes everything through unchanged.
    for(QWidget* child:widget->findChildren<QWidget*>())
        child->installEventFilter(this);
    widget->show();
    return widget;
}

bool PanelCanvas::addControl(const PanelControl& def) {
    if (!supportedTypes().contains(def.type) || def.id.isEmpty() || findId(def.id) || m_reservedIds.contains(def.id)
        || def.geometry.width()<25 || def.geometry.height()<22 ||
        def.geometry.width()>3000 || def.geometry.height()>3000 ||
        def.geometry.x()+def.geometry.width()>8192 ||
        def.geometry.y()+def.geometry.height()>8192 || def.minimum>=def.maximum
        || m_items.size()>=200) return false;
    // Validate factory-declared settings before constructing the widget.
    PanelControl validated;
    if(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(def),&validated,nullptr))return false;
    QWidget* widget=createWidget(def);
    if (!widget) return false;
    m_items.append({def,widget});
    growDocumentToFit(widget->geometry());
    const bool priorBatch=m_batchEditing;
    m_batchEditing=true; // preserve pre-add selection in Undo's before state
    selectWidget(widget);
    m_batchEditing=priorBatch;
    emit documentChanged();
    recordChange(QStringLiteral("Add %1").arg(def.type));
    return true;
}

void PanelCanvas::clearControls() {
    selectWidget(nullptr);
    for (auto& item:m_items) delete item.widget;
    m_items.clear();
    if (!m_restoringHistory) {
        m_undoStack->clear();
        m_lastSnapshot=snapshot();
    }
    emit documentChanged();
}

// A Page is a persistent set of actual QWidget instances. Switching changes
// visibility and the active designer collection; it does not reconstruct
// controls, replay event arguments, or maintain a parallel value cache.
void PanelCanvas::resetPages() {
    clearControls();
    for (auto it=m_inactivePages.begin(); it!=m_inactivePages.end(); ++it)
        for (auto& item:it.value().items) delete item.widget;
    m_inactivePages.clear();
    m_runInformationInput.clear(); // do not leak the last project's operator details
    m_runInformationDefinition=QJsonObject{};
    m_activePageId.clear();
    m_reservedIds.clear();
    resetDesignHistory();
}

void PanelCanvas::forgetPage(const QString& id) {
    // An active Page must be switched away from before it can be deleted.
    if (id==m_activePageId) return;
    auto it=m_inactivePages.find(id);
    if(it==m_inactivePages.end())return;
    for(auto& item:it.value().items)delete item.widget;
    m_inactivePages.erase(it);
}

bool PanelCanvas::activatePage(const QString& id, const QJsonArray& controls,
                               const QSize& size, QString* error) {
    if (id.isEmpty()) {
        if(error)*error=QStringLiteral("Empty Page ID");
        return false;
    }
    if (m_activePageId==id)return true;
    const QString oldPage=m_activePageId;
    // The first activation may adopt the existing demo canvas. On other
    // switches, park all existing widgets by hiding them, not deleting them.
    if(!oldPage.isEmpty()) {
        selectWidget(nullptr);
        m_marquee->hide();
        m_marqueeSelecting=false;
        m_dragging=false;
        m_resizing=false;
        m_dragOriginals.clear();
        for(auto& item:m_items)item.widget->hide();
        PageWidgets previous;
        previous.items=std::move(m_items);
        previous.size=m_documentSize;
        m_inactivePages.insert(oldPage,std::move(previous));
        m_items.clear();
    }
    if(m_inactivePages.contains(id)) {
        PageWidgets saved=m_inactivePages.take(id);
        m_items=std::move(saved.items);
        m_documentSize=saved.size;
        setFixedSize(m_documentSize);
        updateGeometry();
        for(auto& item:m_items)item.widget->show();
    } else if (oldPage.isEmpty() && !m_items.isEmpty()) {
        // Initial example document was built directly on the canvas.
        setDocumentSize(size.expandedTo(minimumContentSize()));
    } else {
        // A newly visited Page is created once from the project document.
        // Qt widgets are then retained across all subsequent Page switches.
        if(!fromJson(controls,error)) {
            if(!oldPage.isEmpty()) {
                PageWidgets saved=m_inactivePages.take(oldPage);
                m_items=std::move(saved.items);
                m_documentSize=saved.size;
                setFixedSize(m_documentSize);
                for(auto& item:m_items)item.widget->show();
            }
            return false;
        }
        setDocumentSize(size.expandedTo(minimumContentSize()));
    }
    m_activePageId=id;
    // An undo command may only operate on the Page for which it was created.
    // Discard per-canvas undo history on tab switches as the previous design did.
    resetDesignHistory();
    syncSelection();
    emit selectedChanged(false);
    emit documentChanged(); // refresh the Panel Size display after a Page switch
    return true;
}

PanelCanvas::Item* PanelCanvas::findAnyId(const QString& id) {
    if(Item* active=findId(id))return active;
    for(auto it=m_inactivePages.begin(); it!=m_inactivePages.end(); ++it)
        for(auto& item:it.value().items)
            if(item.def.id==id)return &item;
    return nullptr;
}

// Values changed during Run are not design-time defaults. Restore the saved
// document directly to the widgets when the single shared runtime stops.
void PanelCanvas::restoreDocumentValues(const QJsonArray& controls) {
    for(const auto& entry:controls) {
        const QJsonObject def=entry.toObject();
        Item* item=findAnyId(def.value(QStringLiteral("id")).toString());
        if(!item)continue;
        const QString type=def.value(QStringLiteral("type")).toString();
        const double value=def.value(QStringLiteral("value")).toDouble();
        item->def.value=value;
        if(const auto* factory=WidgetRegistry::instance().factory(item->def.type))
            factory->applyDefinition(item->widget,item->def);
    }
    if(m_designMode)resetDesignHistory();
}

PanelCanvas::Item* PanelCanvas::findItem(QWidget* widget) {
    for (auto& item:m_items) if (item.widget==widget) return &item;
    return nullptr;
}
const PanelCanvas::Item* PanelCanvas::findItem(QWidget* widget) const {
    for (const auto& item:m_items) if (item.widget==widget) return &item;
    return nullptr;
}
PanelCanvas::Item* PanelCanvas::findId(const QString& id) {
    for (auto& item:m_items) if (item.def.id==id) return &item;
    return nullptr;
}
PanelControl PanelCanvas::readItem(const Item& item) {
    PanelControl def=item.def;
    def.geometry=item.widget->geometry();
    if(const auto* factory=WidgetRegistry::instance().factory(def.type))
        factory->readDefinition(item.widget,def);
    return def;
}
QVector<PanelControl> PanelCanvas::selectedControls() const {
    QVector<PanelControl> controls;
    for (const auto& item : m_items)
        if (m_selection.contains(item.widget)) controls.append(readItem(item));
    return controls;
}
void PanelCanvas::selectRectWithBase(const QRect& rectangle, const QSet<QWidget*>& base) {
    if (!m_designMode) return;
    QSet<QWidget*> next=base;
    QWidget* primary=nullptr;
    const QRect area=rectangle.normalized();
    if (area.width()>0 && area.height()>0) {
        for (const auto& item : m_items) {
            if (item.widget->geometry().intersects(area)) {
                next.insert(item.widget);
                primary=item.widget;
            }
        }
    }
    if (next==m_selection) return;
    m_selection=next;
    if (primary) m_selected=primary;
    else if (!m_selection.contains(m_selected)) {
        m_selected=nullptr;
        for (const auto& item:m_items) if (m_selection.contains(item.widget)) {
            m_selected=item.widget;break;
        }
    }
    syncSelection();
    updateHistorySelection();
    emit selectedChanged(!m_selection.isEmpty());
}

void PanelCanvas::selectControlsInRect(const QRect& rectangle, bool additive) {
    selectRectWithBase(rectangle, additive?m_selection:QSet<QWidget*>{});
}

void PanelCanvas::selectControlById(const QString& id, bool additive) {
    if (Item* item=findId(id)) selectWidget(item->widget, additive);
}
void PanelCanvas::selectAllControls() {
    if (!m_designMode) return;
    m_selection.clear();
    for (const auto& item:m_items) m_selection.insert(item.widget);
    m_selected=m_items.isEmpty()?nullptr:m_items.first().widget;
    syncSelection(); updateHistorySelection(); emit selectedChanged(m_selected!=nullptr);
    setFocus(Qt::OtherFocusReason);
}

PanelControl PanelCanvas::selectedControl() const {
    if (const Item* item=findItem(m_selected)) return readItem(*item);
    return {};
}

void PanelCanvas::selectWidget(QWidget* w, bool additive) {
    if (!additive) {
        m_selection.clear();
        if (w) m_selection.insert(w);
        m_selected=w;
    } else if (w) {
        if (m_selection.contains(w)) {
            m_selection.remove(w);
            if (m_selected==w) m_selected=m_selection.isEmpty()?nullptr:*m_selection.cbegin();
        } else {
            m_selection.insert(w);
            m_selected=w;
        }
    }
    syncSelection();
    updateHistorySelection();
    emit selectedChanged(m_selected!=nullptr);
    if (m_designMode) setFocus(Qt::MouseFocusReason);
}
void PanelCanvas::syncSelection() {
    for (auto it=m_secondaryOutlines.begin();it!=m_secondaryOutlines.end();) {
        if (!m_designMode || !m_selection.contains(it.key()) || it.key()==m_selected) {
            delete it.value();
            it=m_secondaryOutlines.erase(it);
        } else ++it;
    }
    const bool visible=m_designMode && m_selected;
    m_outline->setVisible(visible);
    m_resize->setVisible(visible && m_selection.size()==1);
    if (!visible) return;
    for (QWidget* w : std::as_const(m_selection)) {
        if (w==m_selected) continue;
        auto* outline=m_secondaryOutlines.value(w,nullptr);
        if (!outline) {
            outline=new QRubberBand(QRubberBand::Rectangle,this);
            outline->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_secondaryOutlines.insert(w,outline);
        }
        outline->setGeometry(w->geometry().adjusted(-3,-3,3,3));
        outline->show();outline->raise();
    }
    const QRect r=m_selected->geometry();
    m_outline->setGeometry(r.adjusted(-3,-3,3,3));
    m_outline->raise();
    m_resize->move(r.right()-5,r.bottom()-5);
    if (m_selection.size()==1) m_resize->raise();
}

bool PanelCanvas::updateSelected(const PanelControl& def) {
    Item* item=findItem(m_selected);
    if (!item || def.id.isEmpty() || !supportedTypes().contains(def.type)
        || def.type!=item->def.type || def.minimum>=def.maximum ||
        def.geometry.width()<25 || def.geometry.height()<22 ||
        def.geometry.width()>3000 || def.geometry.height()>3000 ||
        def.geometry.x()+def.geometry.width()>8192 ||
        def.geometry.y()+def.geometry.height()>8192) return false;
    if(m_reservedIds.contains(def.id)) return false;
    PanelControl validated;
    if(!WidgetDocumentCodec::decode(WidgetDocumentCodec::encode(def),&validated,nullptr))return false;
    for (const auto& other:m_items) if (other.widget!=m_selected && other.def.id==def.id) return false;
    // Preserve the event configuration when a caller edits ordinary geometry/text.
    item->def=def;
    item->widget->setObjectName(def.id);
    item->widget->setGeometry(def.geometry);
    if(const auto* factory=WidgetRegistry::instance().factory(def.type))
        factory->applyDefinition(item->widget,def);
    syncSelection();
    growDocumentToFit(item->widget->geometry());
    emit documentChanged();
    recordChange(QStringLiteral("Edit properties"));
    return true;
}
bool PanelCanvas::setSelectedEventBinding(const QString& eventName,const QString& handler,bool enabled) {
    if(!m_designMode || m_selection.size()!=1) return false;
    Item* item=findItem(m_selected);
    if(!item || !PanelEventCatalog::supports(item->def.type,eventName)) return false;
    if(!handler.isEmpty() && !PanelEventCatalog::validHandler(handler)) return false;
    QJsonObject events=item->def.events;
    if(handler.isEmpty()) events.remove(eventName);
    else events.insert(eventName,QJsonObject{{"handler",handler},{"enabled",enabled}});
    if(events==item->def.events)return true;
    item->def.events=events;
    emit documentChanged();
    recordChange(QStringLiteral("Assign event handler"));
    return true;
}

void PanelCanvas::deleteSelected() {
    if (!m_designMode || m_selection.isEmpty()) return;
    const QSet<QWidget*> selected=m_selection;
    const bool priorBatch=m_batchEditing;
    m_batchEditing=true; // preserve the deleted selection for Undo
    selectWidget(nullptr);
    m_batchEditing=priorBatch;
    for (qsizetype i=m_items.size();i-->0;) {
        if (!selected.contains(m_items[i].widget)) continue;
        QWidget* widget=m_items[i].widget;
        m_items.removeAt(i);
        delete widget;
    }
    emit documentChanged();
    recordChange(QStringLiteral("Delete controls"));
}

QJsonArray PanelCanvas::toJson() const {
    QJsonArray items;
    for (const auto& item:m_items) items.append(WidgetDocumentCodec::encode(readItem(item)));
    return items;
}
bool PanelCanvas::parseControls(const QJsonArray& json,QVector<PanelControl>& definitions,QString* error) {
    return WidgetDocumentCodec::decodeList(json,&definitions,error);
}
bool PanelCanvas::validateJson(const QJsonArray& json, QString* error) {
    QVector<PanelControl> definitions;
    return parseControls(json,definitions,error);
}
bool PanelCanvas::fromJson(const QJsonArray& json,QString* error) {
    QVector<PanelControl> definitions;
    if(!parseControls(json,definitions,error))return false;
    clearControls();
    for (const auto& d:definitions) addControl(d);
    selectWidget(nullptr);
    if (!m_restoringHistory) {
        m_undoStack->clear();
        m_lastSnapshot=snapshot();
    }
    return true;
}
QVariantMap PanelCanvas::runInformationInputValues() const {
    // Works with zero, one or many Run Information views. Captures the pending
    // values even if all views are on Pages the operator hasn't visited.
    return m_runInformationInput;
}

QVariantMap PanelCanvas::initialValues() const {
    QVariantMap values;
    for (const auto& item:m_items) {
        const PanelControl c=readItem(item);
        if(const auto* f=WidgetRegistry::instance().factory(c.type))
            values.insert(c.id,f->initialRuntimeValue(c));
        else values.insert(c.id,c.value);
    }
    return values;
}
QString PanelCanvas::uniqueId(const QString& type) const {
    int i=1;
    for (;;) {
        const QString name=type+QString::number(i++);
        bool exists=m_reservedIds.contains(name);
        for (const auto& item:m_items) if (item.def.id==name) {exists=true;break;}
        if(!exists)return name;
    }
}
void PanelCanvas::applyValue(const QString& id,const QVariant& value) {
    Item* item=findAnyId(id);
    if(!item)return;
    if(const auto* factory=WidgetRegistry::instance().factory(item->def.type))
        factory->applyRuntimeValue(item->widget,item->def,value);
    // Runtime updates are not design-time defaults. Widgets retain their own
    // live values; saving the project never snapshots the script's ui.set().
}
void PanelCanvas::applyPlotCommand(const PlotCommand& command) {
    // Lookup spans active and inactive Pages; no widget reconstruction occurs.
    Item* item=findAnyId(command.plotId);
    if(!item || (item->def.type!=QStringLiteral("plot") && item->def.type!=QStringLiteral("digital_plot")))return;
    if(const auto* f=WidgetRegistry::instance().factory(item->def.type))
        f->deliverPlotCommand(item->widget,command);
}

bool PanelCanvas::eventFilter(QObject* watched,QEvent* e) {
    if (watched==m_resize && m_designMode) {
        auto* me=dynamic_cast<QMouseEvent*>(e);
        if (e->type()==QEvent::MouseButtonPress && me && me->button()==Qt::LeftButton && m_selected) {
            m_resizing=true;m_dragOrigin=me->globalPosition().toPoint();m_originalGeometry=m_selected->geometry();return true;
        }
        if (e->type()==QEvent::MouseMove && me && m_resizing && m_selected) {
            const QPoint d=me->globalPosition().toPoint()-m_dragOrigin;
            const int width=qBound(25,m_originalGeometry.width()+d.x(),
                                   qMin(3000,8192-m_originalGeometry.x()));
            const int height=qBound(22,m_originalGeometry.height()+d.y(),
                                    qMin(3000,8192-m_originalGeometry.y()));
            m_selected->setGeometry(QRect(m_originalGeometry.topLeft(),
                QSize(m_snapToGrid?qMax(25,snap(width)):width,
                      m_snapToGrid?qMax(22,snap(height)):height)));
            syncSelection();return true;
        }
        if (e->type()==QEvent::MouseButtonRelease && m_resizing) {
            m_resizing=false;
            if (m_selected) growDocumentToFit(m_selected->geometry());
            emit documentChanged();
            recordChange(QStringLiteral("Resize control"));return true;
        }
    }
    QWidget* w=qobject_cast<QWidget*>(watched);
    if (!m_designMode || !w) return QWidget::eventFilter(watched,e);
    // A composite input's spin box, line edit or combo box must participate
    // in the same selection/drag lifecycle as its top-level widget.
    QWidget* root=w;
    while(root && !findItem(root))root=root->parentWidget();
    if(!root)return QWidget::eventFilter(watched,e);
    w=root;
    // The designer never lets a child editor change the live widget value.
    if(e->type()==QEvent::Wheel || e->type()==QEvent::KeyPress ||
       e->type()==QEvent::KeyRelease) return true;
    if (e->type()==QEvent::ContextMenu) {
        auto* ce=static_cast<QContextMenuEvent*>(e);
        if (!m_selection.contains(w)) selectWidget(w);
        showDesignerContextMenu(ce->globalPos());return true;
    }
    auto* me=dynamic_cast<QMouseEvent*>(e);
    if(e->type()==QEvent::MouseButtonPress && me && me->button()==Qt::LeftButton) {
        const bool additive=(me->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier)) != Qt::NoModifier;
        if (additive) selectWidget(w,true);
        else if (!m_selection.contains(w)) selectWidget(w);
        else {m_selected=w;syncSelection();updateHistorySelection();emit selectedChanged(true);setFocus(Qt::MouseFocusReason);}
        if (!m_selection.contains(w)) return true;
        m_dragging=true;
        m_dragOrigin=me->globalPosition().toPoint();
        m_originalGeometry=w->geometry();
        m_dragOriginals.clear();
        for (QWidget* selected:std::as_const(m_selection)) m_dragOriginals.insert(selected,selected->geometry());
        return true;
    }
    if(e->type()==QEvent::MouseMove && me && m_dragging && m_dragOriginals.contains(w)) {
        QPoint delta=me->globalPosition().toPoint()-m_dragOrigin;
        if (m_snapToGrid) {
            const QPoint anchor=m_originalGeometry.topLeft();
            delta=QPoint(snap(anchor.x()+delta.x())-anchor.x(),
                         snap(anchor.y()+delta.y())-anchor.y());
        }
        int left=0,top=0;
        for (auto it=m_dragOriginals.cbegin();it!=m_dragOriginals.cend();++it) {
            left=qMin(left,it.value().x()+delta.x());
            top=qMin(top,it.value().y()+delta.y());
        }
        delta+=QPoint(-left,-top);
        // Never position controls outside the maximum persisted panel bounds.
        int maxRight=0,maxBottom=0;
        for (auto it=m_dragOriginals.cbegin();it!=m_dragOriginals.cend();++it) {
            maxRight=qMax(maxRight,it.value().x()+it.value().width());
            maxBottom=qMax(maxBottom,it.value().y()+it.value().height());
        }
        delta.setX(qMin(delta.x(),8192-maxRight));
        delta.setY(qMin(delta.y(),8192-maxBottom));
        for (auto it=m_dragOriginals.cbegin();it!=m_dragOriginals.cend();++it)
            it.key()->move(it.value().topLeft()+delta);
        syncSelection();return true;
    }
    if(e->type()==QEvent::MouseButtonRelease && m_dragging) {
        m_dragging=false;
        for (QWidget* selected:std::as_const(m_selection)) growDocumentToFit(selected->geometry());
        m_dragOriginals.clear();
        emit documentChanged();
        recordChange(QStringLiteral("Move controls"));return true;
    }
    if(e->type()==QEvent::MouseButtonDblClick) return true;
    return QWidget::eventFilter(watched,e);
}

void PanelCanvas::setPageAppearance(const QColor& background,bool light,bool showGrid) {
    m_background=background;
    m_showDesignGrid=showGrid;
    setProperty("wfPanelLight",light); // custom widgets inherit the theme through their parent
    QPalette pal=palette();pal.setColor(QPalette::Base,background);pal.setColor(QPalette::Window,background);
    setPalette(pal);
    // Style only built-in Qt controls. Native custom instrument painters use
    // InstrumentVisuals, retaining their own instance/value/plot state.
    const QString text=light?QStringLiteral("#203347"):QStringLiteral("#e4f0f9");
    const QString surface=light?QStringLiteral("#ffffff"):QStringLiteral("#263b50");
    const QString border=light?QStringLiteral("#a4b9cb"):QStringLiteral("#56718a");
    setStyleSheet(QStringLiteral(
        "QPushButton,QDoubleSpinBox,QSpinBox,QComboBox,QLineEdit {background:%1;color:%2;"
        "border:1px solid %3;border-radius:6px;padding:3px;}"
        "QPushButton:pressed {background:%3;}"
        "QPushButton:hover {border:1px solid #39a9ca;}"
        "QLabel,QCheckBox,QSlider {color:%2;}"
        "QSlider::groove:horizontal {height:5px;background:%3;border-radius:2px;}"
        "QSlider::handle:horizontal {background:#44c2d7;width:14px;margin:-5px 0;border-radius:7px;}"
    ).arg(surface,text,border));
    update();
    for(QWidget* widget:findChildren<QWidget*>()) {
        QEvent themeChanged(QEvent::ThemeChange);
        QApplication::sendEvent(widget,&themeChanged);
        widget->update();
    }
}

void PanelCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(),m_background);
    if (!m_designMode || !m_showDesignGrid)return;
    p.setPen(QPen(QColor(125,148,180,65),1));
    for(int x=0;x<width();x+=20)p.drawLine(x,0,x,height());
    for(int y=0;y<height();y+=20)p.drawLine(0,y,width(),y);
    // Visible, unambiguous boundary of the real saved panel document.
    p.setPen(QPen(QColor(92,118,153,170),1));
    p.drawRect(rect().adjusted(0,0,-1,-1));
}
void PanelCanvas::mousePressEvent(QMouseEvent* event) {
    if (m_designMode && event->button()==Qt::LeftButton) {
        m_marqueeSelecting=true;
        m_marqueeOrigin=event->position().toPoint();
        const bool additive=(event->modifiers() & (Qt::ControlModifier|Qt::ShiftModifier))!=Qt::NoModifier;
        m_marqueeBase=additive?m_selection:QSet<QWidget*>{};
        m_marquee->setGeometry(QRect(m_marqueeOrigin,QSize(1,1)));
        m_marquee->hide(); // Only show after the pointer actually moves.
        if (!additive) selectWidget(nullptr);
        setFocus(Qt::MouseFocusReason);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}
void PanelCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (m_designMode && m_marqueeSelecting && (event->buttons() & Qt::LeftButton)) {
        const QRect area=QRect(m_marqueeOrigin,event->position().toPoint()).normalized();
        if (area.width()>2 || area.height()>2) {
            m_marquee->setGeometry(area);
            m_marquee->show();
            m_marquee->raise();
            selectRectWithBase(area,m_marqueeBase);
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}
void PanelCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (m_marqueeSelecting && event->button()==Qt::LeftButton) {
        const QRect area=QRect(m_marqueeOrigin,event->position().toPoint()).normalized();
        if (area.width()>2 || area.height()>2) selectRectWithBase(area,m_marqueeBase);
        m_marquee->hide();
        m_marqueeSelecting=false;
        m_marqueeBase.clear();
        setFocus(Qt::MouseFocusReason);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
void PanelCanvas::dragEnterEvent(QDragEnterEvent* event) {
    if(m_designMode && event->mimeData()->hasFormat(kDragMime))event->acceptProposedAction();
}
void PanelCanvas::dropEvent(QDropEvent* event) {
    if(!m_designMode)return;
    const QString type=QString::fromUtf8(event->mimeData()->data(kDragMime));
    if (!supportedTypes().contains(type))return;
    const auto* factory=WidgetRegistry::instance().factory(type);
    if(!factory)return;
    PanelControl def=factory->defaultDefinition(uniqueId(type));
    const QSize size=def.geometry.size();
    def.geometry.moveTopLeft(event->position().toPoint());
    def.geometry.moveTopLeft(QPoint(qBound(0,def.geometry.x(),8192-size.width()),
                                    qBound(0,def.geometry.y(),8192-size.height())));
    if (m_snapToGrid) def.geometry.moveTopLeft(QPoint(snap(def.geometry.x()),snap(def.geometry.y())));
    def.geometry.moveTopLeft(QPoint(qBound(0,def.geometry.x(),8192-size.width()),
                                    qBound(0,def.geometry.y(),8192-size.height())));
    if (addControl(def)) event->acceptProposedAction();
}


// Design editing and history --------------------------------------------------
void PanelCanvas::resetDesignHistory() {
    m_undoStack->clear();
    m_lastSnapshot=snapshot();
}

PanelCanvas::Snapshot PanelCanvas::snapshot() const {
    Snapshot state;
    state.controls=toJson();
    state.documentSize=m_documentSize;
    for (const auto& item:m_items)
        if (m_selection.contains(item.widget)) state.selectedIds.append(item.def.id);
    return state;
}
void PanelCanvas::updateHistorySelection() {
    if (m_restoringHistory || m_batchEditing) return;
    m_lastSnapshot.selectedIds=snapshot().selectedIds;
}
void PanelCanvas::recordChange(const QString& description) {
    if (m_restoringHistory || m_batchEditing || !m_designMode) return;
    const Snapshot after=snapshot();
    if (after.documentSize==m_lastSnapshot.documentSize && after.controls==m_lastSnapshot.controls) {
        m_lastSnapshot=after;
        return;
    }
    m_undoStack->push(new PanelCanvasHistoryCommand(this,m_lastSnapshot,after,description));
    m_lastSnapshot=after;
}
void PanelCanvas::restoreSnapshot(const Snapshot& state) {
    m_restoringHistory=true;
    QString error;
    fromJson(state.controls,&error);
    // fromJson expands as needed while rebuilding; restore exact saved bounds.
    setDocumentSize(state.documentSize.expandedTo(minimumContentSize()));
    m_selection.clear();m_selected=nullptr;
    for (const QString& id:state.selectedIds) {
        if (Item* item=findId(id)) {m_selection.insert(item->widget);m_selected=item->widget;}
    }
    syncSelection();
    m_restoringHistory=false;
    m_lastSnapshot=snapshot();
    emit selectedChanged(m_selected!=nullptr);
    emit documentChanged();
}
int PanelCanvas::snap(int coordinate) const {
    return qRound(double(coordinate)/kGrid)*kGrid;
}
void PanelCanvas::cloneControls(const QVector<PanelControl>& source) {
    if (!m_designMode || source.isEmpty() || m_items.size()+source.size()>200) return;
    m_batchEditing=true;
    m_selection.clear();m_selected=nullptr;
    QVector<QWidget*> clones;
    for (PanelControl item:source) {
        item.id=uniqueId(item.type);
        item.events=QJsonObject{}; // Copies never inherit active event connections.
        item.geometry.translate(20+m_pasteOffset,20+m_pasteOffset);
        item.geometry.moveTopLeft(QPoint(qBound(0,item.geometry.x(),8192-item.geometry.width()),
                                         qBound(0,item.geometry.y(),8192-item.geometry.height())));
        if (m_snapToGrid) item.geometry.moveTopLeft(QPoint(snap(item.geometry.x()),snap(item.geometry.y())));
        item.geometry.moveTopLeft(QPoint(qBound(0,item.geometry.x(),8192-item.geometry.width()),
                                         qBound(0,item.geometry.y(),8192-item.geometry.height())));
        if (!addControl(item)) continue;
        clones.append(m_selected);
    }
    // Group the newly cloned controls as one Undo step.
    m_selection.clear();
    for(QWidget* widget:clones) m_selection.insert(widget);
    m_selected=clones.isEmpty()?nullptr:clones.constLast();
    syncSelection();
    emit selectedChanged(m_selected!=nullptr);
    m_batchEditing=false;
    recordChange(QStringLiteral("Duplicate controls"));
    m_pasteOffset=(m_pasteOffset+20)%100;
}
void PanelCanvas::copySelected() {
    const auto selected=selectedControls();
    if (!m_designMode || selected.isEmpty()) return;
    QJsonArray json;
    for (const auto& control:selected) json.append(WidgetDocumentCodec::encode(control));
    auto* mime=new QMimeData;
    mime->setData(kClipboardMime,QJsonDocument(json).toJson(QJsonDocument::Compact));
    QApplication::clipboard()->setMimeData(mime);
    m_pasteOffset=0;
}
void PanelCanvas::cutSelected() { copySelected();deleteSelected(); }
void PanelCanvas::pasteControls() {
    if (!m_designMode) return;
    const QMimeData* mime=QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasFormat(kClipboardMime)) return;
    QJsonParseError error;
    const QByteArray payload=mime->data(kClipboardMime);
    if (payload.size()>1024*1024) return; // Never parse unbounded clipboard data
    const auto doc=QJsonDocument::fromJson(payload,&error);
    if (error.error!=QJsonParseError::NoError || !doc.isArray() || doc.array().size()>200) return;
    QVector<PanelControl> controls;
    if(!WidgetDocumentCodec::decodeList(doc.array(),&controls))return;
    cloneControls(controls);
}
void PanelCanvas::duplicateSelected() { cloneControls(selectedControls()); }
void PanelCanvas::nudgeSelected(int dx, int dy) {
    if (!m_designMode || m_selection.isEmpty()) return;
    for (QWidget* w:std::as_const(m_selection))
        if (w->x()+dx<0 || w->y()+dy<0 ||
            w->x()+dx+w->width()>8192 || w->y()+dy+w->height()>8192) return;
    for (QWidget* w:std::as_const(m_selection)) {
        w->move(w->pos()+QPoint(dx,dy));
        growDocumentToFit(w->geometry());
    }
    syncSelection();emit documentChanged();recordChange(QStringLiteral("Nudge controls"));
}
void PanelCanvas::alignSelected(const QString& operation) {
    if (!m_designMode || m_selection.size()<2 || !m_selected) return;
    const QRect reference=m_selected->geometry();
    for (QWidget* w:std::as_const(m_selection)) {
        QRect r=w->geometry();
        if (operation=="left")r.moveLeft(reference.left());
        else if (operation=="right")r.moveRight(reference.right());
        else if (operation=="top")r.moveTop(reference.top());
        else if (operation=="bottom")r.moveBottom(reference.bottom());
        else if (operation=="hcenter")r.moveCenter(QPoint(reference.center().x(),r.center().y()));
        else if (operation=="vcenter")r.moveCenter(QPoint(r.center().x(),reference.center().y()));
        else if (operation=="width")r.setWidth(reference.width());
        else if (operation=="height")r.setHeight(reference.height());
        else return;
        if (r.left()<0 || r.top()<0) continue;
        w->setGeometry(r);
        growDocumentToFit(r);
    }
    syncSelection();emit documentChanged();recordChange(QStringLiteral("Align controls"));
}
void PanelCanvas::bringSelectedToFront() {
    if (!m_designMode || m_selection.isEmpty())return;
    QVector<Item> selected,unselected;
    for (const auto& item:m_items) (m_selection.contains(item.widget)?selected:unselected).append(item);
    m_items=unselected;
    for (const auto& item:selected) {m_items.append(item);item.widget->raise();}
    syncSelection();emit documentChanged();recordChange(QStringLiteral("Bring to front"));
}
void PanelCanvas::sendSelectedToBack() {
    if (!m_designMode || m_selection.isEmpty())return;
    QVector<Item> selected,unselected;
    for (const auto& item:m_items) (m_selection.contains(item.widget)?selected:unselected).append(item);
    m_items=selected;
    m_items+=unselected;
    // Rebuild QWidget stacking order to match serialized order.
    for (const auto& item:m_items) item.widget->raise();
    syncSelection();emit documentChanged();recordChange(QStringLiteral("Send to back"));
}
void PanelCanvas::showDesignerContextMenu(const QPoint& globalPosition) {
    if (!m_designMode) return;
    QMenu menu(this);
    QAction* copy=menu.addAction(QStringLiteral("Copy   Ctrl+C"));
    QAction* cut=menu.addAction(QStringLiteral("Cut   Ctrl+X"));
    QAction* paste=menu.addAction(QStringLiteral("Paste   Ctrl+V"));
    QAction* duplicate=menu.addAction(QStringLiteral("Duplicate   Ctrl+D"));
    menu.addSeparator();
    QAction* front=menu.addAction(QStringLiteral("Bring to Front"));
    QAction* back=menu.addAction(QStringLiteral("Send to Back"));
    QAction* remove=menu.addAction(QStringLiteral("Delete"));
    menu.addSeparator();
    QAction* undo=menu.addAction(m_undoStack->undoText().isEmpty()?QStringLiteral("Undo"):QStringLiteral("Undo ")+m_undoStack->undoText());
    QAction* redo=menu.addAction(m_undoStack->redoText().isEmpty()?QStringLiteral("Redo"):QStringLiteral("Redo ")+m_undoStack->redoText());
    for (QAction* action:{copy,cut,duplicate,front,back,remove})action->setEnabled(!m_selection.isEmpty());
    paste->setEnabled(QApplication::clipboard()->mimeData()->hasFormat(kClipboardMime));
    undo->setEnabled(m_undoStack->canUndo());redo->setEnabled(m_undoStack->canRedo());
    QAction* chosen=menu.exec(globalPosition);
    if(chosen==copy)copySelected();else if(chosen==cut)cutSelected();
    else if(chosen==paste)pasteControls();else if(chosen==duplicate)duplicateSelected();
    else if(chosen==front)bringSelectedToFront();else if(chosen==back)sendSelectedToBack();
    else if(chosen==remove)deleteSelected();else if(chosen==undo)m_undoStack->undo();
    else if(chosen==redo)m_undoStack->redo();
}
void PanelCanvas::contextMenuEvent(QContextMenuEvent* event) {
    showDesignerContextMenu(event->globalPos());event->accept();
}
void PanelCanvas::keyPressEvent(QKeyEvent* event) {
    if (!m_designMode) { QWidget::keyPressEvent(event);return; }
    const Qt::KeyboardModifiers mod=event->modifiers();
    const bool ctrl=mod.testFlag(Qt::ControlModifier);
    const bool shift=mod.testFlag(Qt::ShiftModifier);
    if(ctrl && event->key()==Qt::Key_Z) {if(shift)m_undoStack->redo();else m_undoStack->undo();}
    else if(ctrl && event->key()==Qt::Key_Y)m_undoStack->redo();
    else if(ctrl && event->key()==Qt::Key_A)selectAllControls();
    else if(ctrl && event->key()==Qt::Key_C)copySelected();
    else if(ctrl && event->key()==Qt::Key_X)cutSelected();
    else if(ctrl && event->key()==Qt::Key_V)pasteControls();
    else if(ctrl && event->key()==Qt::Key_D)duplicateSelected();
    else if(event->key()==Qt::Key_Delete || event->key()==Qt::Key_Backspace)deleteSelected();
    else if(event->key()==Qt::Key_Left)nudgeSelected(shift?-10:-1,0);
    else if(event->key()==Qt::Key_Right)nudgeSelected(shift?10:1,0);
    else if(event->key()==Qt::Key_Up)nudgeSelected(0,shift?-10:-1);
    else if(event->key()==Qt::Key_Down)nudgeSelected(0,shift?10:1);
    else {QWidget::keyPressEvent(event);return;}
    event->accept();
}
