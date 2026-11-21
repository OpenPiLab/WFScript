#include "EventLogWidget.h"
#include "InstrumentVisuals.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QListView>
#include <QPalette>
#include <QBrush>
#include <QDateTime>
#include <QColor>
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <utility>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QShowEvent>
#include <QEvent>
#include <QStringList>
#include <QByteArray>
#include <QTableView>
#include <QAbstractTableModel>
#include <deque>
#include <cstdint>
#include <iterator>
#include <QItemSelectionModel>
#include <QVBoxLayout>
#include <QMetaType>
#include <QtGlobal>

namespace {
QString csvField(QString text) {
    // Protect spreadsheet consumers from interpreting untrusted log text as a
    // formula (CSV injection). The apostrophe is retained as plain text.
    const QString trimmed=text.trimmed();
    if(!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))
        text.prepend(QStringLiteral("'"));
    text.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+text+QStringLiteral("\"");
}
QString normalizedLevel(const QString& text) {
    const QString level=text.trimmed().toUpper();
    for(const auto& name:{"DEBUG","INFO","PASS","WARN","FAIL","ERROR"})
        if(level==QString::fromLatin1(name))return level;
    return {};
}
QColor severityColor(const QString& level,bool light) {
    if(level==QStringLiteral("PASS"))return QColor(light?"#157a50":"#50d79e");
    if(level==QStringLiteral("WARN"))return QColor(light?"#906018":"#ebc56c");
    if(level==QStringLiteral("FAIL") || level==QStringLiteral("ERROR"))
        return QColor(light?"#b02b3c":"#ff7780");
    if(level==QStringLiteral("DEBUG"))return QColor(light?"#6c7987":"#9bacc0");
    return QColor(light?"#316a94":"#87c7f3");
}
}


// Retains only data and a filtered index, never QTableWidgetItem objects.
// Qt's item view paints on demand even with tens of thousands of events.
class EventLogModel final : public QAbstractTableModel {
public:
    using Id=std::uint64_t;
    explicit EventLogModel(std::deque<EventLogWidget::Entry>* entries,Id* firstId,QObject* parent)
        :QAbstractTableModel(parent),m_entries(entries),m_firstId(firstId){}
    int rowCount(const QModelIndex& parent=QModelIndex()) const override {
        return parent.isValid()?0:int(m_visible.size());
    }
    int columnCount(const QModelIndex& parent=QModelIndex()) const override {
        return parent.isValid()?0:4;
    }
    QVariant headerData(int section,Qt::Orientation orientation,int role) const override {
        if(role==Qt::DisplayRole && orientation==Qt::Horizontal)
            return QStringList{QStringLiteral("Time"),QStringLiteral("Level"),
                QStringLiteral("Source"),QStringLiteral("Message")}.value(section);
        return {};
    }
    QVariant data(const QModelIndex& idx,int role) const override {
        if(!idx.isValid()||idx.row()<0||idx.row()>=int(m_visible.size()))return {};
        const Id id=m_visible[std::size_t(idx.row())];
        if(id<*m_firstId || id-*m_firstId>=m_entries->size())return {};
        const auto& e=(*m_entries)[std::size_t(id-*m_firstId)];
        if(role==Qt::DisplayRole||role==Qt::ToolTipRole) {
            if(idx.column()==0) {
                if(role==Qt::ToolTipRole)return e.timestamp;
                const QDateTime t=QDateTime::fromString(e.timestamp,Qt::ISODateWithMs);
                return t.isValid()?t.toString(QStringLiteral("HH:mm:ss.zzz")):e.timestamp;
            }
            if(idx.column()==1)return e.level;
            if(idx.column()==2)return e.source;
            return e.message;
        }
        if(role==Qt::ForegroundRole && idx.column()==1)
            return QBrush(severityColor(e.level,m_light));
        return {};
    }
    void setLight(bool light) {
        if(m_light==light)return;
        m_light=light;
        if(!m_visible.empty())
            emit dataChanged(index(0,1),index(rowCount()-1,1),{Qt::ForegroundRole});
    }
    void resetRows(const QString& filter,bool light) {
        beginResetModel();
        m_filter=filter;m_light=light;m_visible.clear();
        Id id=*m_firstId;
        for(const auto& e:*m_entries) {
            if(filter==QStringLiteral("All")||e.level==filter)m_visible.push_back(id);
            ++id;
        }
        endResetModel();
    }
    // Incremental eviction retains all unaffected model indexes. Both the
    // history and visible-ID deque pop from the front without shifting 50k rows.
    void dropOldest(int count) {
        if(count<=0)return;
        const Id nextFirst=*m_firstId+Id(count);
        int visibleRemoved=0;
        for(const auto id:m_visible) {
            if(id>=nextFirst)break;
            ++visibleRemoved;
        }
        if(visibleRemoved)beginRemoveRows(QModelIndex(),0,visibleRemoved-1);
        for(int i=0;i<count;++i)m_entries->pop_front();
        *m_firstId=nextFirst;
        for(int i=0;i<visibleRemoved;++i)m_visible.pop_front();
        if(visibleRemoved)endRemoveRows();
    }
    void appendRange(Id first,Id last) {
        int visibleAdded=0;
        for(Id id=first;id<last;++id) {
            const auto& e=(*m_entries)[std::size_t(id-*m_firstId)];
            if(m_filter==QStringLiteral("All")||e.level==m_filter)++visibleAdded;
        }
        if(!visibleAdded)return;
        const int start=int(m_visible.size());
        beginInsertRows(QModelIndex(),start,start+visibleAdded-1);
        for(Id id=first;id<last;++id) {
            const auto& e=(*m_entries)[std::size_t(id-*m_firstId)];
            if(m_filter==QStringLiteral("All")||e.level==m_filter)m_visible.push_back(id);
        }
        endInsertRows();
    }
    Id visibleIdAt(int row) const {
        return row>=0 && row<int(m_visible.size()) ? m_visible[std::size_t(row)] : Id(0);
    }
    int indexAtOrAfter(Id id) const {
        if(m_visible.empty())return -1;
        const auto it=std::lower_bound(m_visible.begin(),m_visible.end(),id);
        if(it==m_visible.end())return int(m_visible.size())-1;
        return int(std::distance(m_visible.begin(),it));
    }
private:
    std::deque<EventLogWidget::Entry>* m_entries;
    Id* m_firstId;
    std::deque<Id> m_visible;
    QString m_filter=QStringLiteral("All");
    bool m_light=false;
};

EventLogWidget::EventLogWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("wfEventLogRoot"));
    auto* layout=new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(6);
    auto* bar=new QHBoxLayout;
    m_heading=new QLabel(QStringLiteral("Event Log"),this);
    auto f=m_heading->font();f.setBold(true);m_heading->setFont(f);
    bar->addWidget(m_heading,1);
    m_filter=new QComboBox(this);
    m_filter->setObjectName(QStringLiteral("eventLogFilter"));
    // Use a Qt-managed popup view so the Panel theme cannot leave native
    // Windows dropdown entries with matching foreground/background colours.
    m_filter->setView(new QListView(m_filter));
    m_filter->view()->setObjectName(QStringLiteral("eventLogFilterPopup"));
    m_filter->addItems({QStringLiteral("All"),QStringLiteral("DEBUG"),QStringLiteral("INFO"),
                        QStringLiteral("PASS"),QStringLiteral("WARN"),QStringLiteral("FAIL"),
                        QStringLiteral("ERROR")});
    m_filter->setToolTip(QStringLiteral("Filter visible events; CSV export includes the complete log"));
    bar->addWidget(m_filter);
    auto* copy=new QPushButton(QStringLiteral("Copy"),this);
    copy->setToolTip(QStringLiteral("Copy selected event rows"));
    auto* clear=new QPushButton(QStringLiteral("Clear"),this);
    auto* exportButton=new QPushButton(QStringLiteral("Export CSV"),this);
    bar->addWidget(copy);bar->addWidget(clear);bar->addWidget(exportButton);
    layout->addLayout(bar);
    m_table=new QTableView(this);
    m_table->setObjectName(QStringLiteral("eventLogTable"));
    m_model=new EventLogModel(&m_entries,&m_firstId,m_table);
    m_table->setModel(m_model);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(27);
    // Do not scan retained rows to resize headers every time a row arrives.
    auto* header=m_table->horizontalHeader();
    header->setSectionResizeMode(0,QHeaderView::Interactive);
    header->setSectionResizeMode(1,QHeaderView::Interactive);
    header->setSectionResizeMode(2,QHeaderView::Interactive);
    header->setSectionResizeMode(3,QHeaderView::Stretch);
    header->resizeSection(0,118);
    header->resizeSection(1,72);
    header->resizeSection(2,138);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    layout->addWidget(m_table,1);
    connect(m_filter,&QComboBox::currentTextChanged,this,[this](const QString&){refresh();});
    connect(copy,&QPushButton::clicked,this,[this]{copySelected();});
    connect(clear,&QPushButton::clicked,this,[this]{clearEvents();});
    connect(exportButton,&QPushButton::clicked,this,[this]{exportCsv();});
    configure(m_title,500,true,true,true);
}

void EventLogWidget::configure(const QString& title,int maxEntries,bool autoScroll,
                               bool showTimestamp,bool showSource) {
    m_title=title.left(120);
    m_maxEntries=qBound(10,maxEntries,100000);
    m_autoScroll=autoScroll;
    m_showTimestamp=showTimestamp;
    m_showSource=showSource;
    trimToCapacity();
    m_heading->setText(m_title.isEmpty()?QStringLiteral("Event Log"):m_title);
    m_table->setColumnHidden(0,!m_showTimestamp);
    m_table->setColumnHidden(2,!m_showSource);
    applyTheme();
    refresh();
}

void EventLogWidget::applyTheme() {
    const bool light=InstrumentVisuals::light(this);
    m_heading->setStyleSheet(light?QStringLiteral("color:#23384c;"):
                                    QStringLiteral("color:#e0ebf5;"));
    setStyleSheet(light?QStringLiteral(
        "QWidget#wfEventLogRoot{background:#edf4fa;border:1px solid #b8cbd9;border-radius:8px;}"
        "QTableView{background:#f4f8fc;color:#26394b;alternate-background-color:#e4edf5;border:1px solid #b8cbd9;}"
        "QHeaderView::section{background:#d9e8f2;color:#26394b;border:0;padding:5px;}"):
        QStringLiteral(
        "QWidget#wfEventLogRoot{background:#101e2f;border:1px solid #3a536b;border-radius:8px;}"
        "QTableView{background:#0e1c2b;color:#d6e4f1;alternate-background-color:#172b3e;border:1px solid #3a536b;}"
        "QHeaderView::section{background:#243a4e;color:#d6e4f1;border:0;padding:5px;}"));

    // The Page canvas styles its QComboBox children, but does not supply a
    // contrasting palette for popup rows on all Windows/Qt styles. Keep both
    // the collapsed selector and the *unselected* popup rows legible; do not
    // rely on hover highlighting to reveal their labels.
    const QColor ink(light?QStringLiteral("#203347"):QStringLiteral("#e4f0f9"));
    const QColor surface(light?QStringLiteral("#ffffff"):QStringLiteral("#20384f"));
    const QColor popupSurface(light?QStringLiteral("#f4f8fc"):QStringLiteral("#16283b"));
    const QColor selection(light?QStringLiteral("#bddbe7"):QStringLiteral("#355a72"));
    const QColor border(light?QStringLiteral("#a4b9cb"):QStringLiteral("#56718a"));
    QPalette palette=m_filter->palette();
    palette.setColor(QPalette::Text,ink);
    palette.setColor(QPalette::ButtonText,ink);
    palette.setColor(QPalette::Base,surface);
    palette.setColor(QPalette::Button,surface);
    palette.setColor(QPalette::Highlight,selection);
    palette.setColor(QPalette::HighlightedText,ink);
    m_filter->setPalette(palette);
    m_filter->setStyleSheet(QStringLiteral(
        "QComboBox#eventLogFilter{color:%1;background:%2;border:1px solid %3;"
        "border-radius:5px;padding:3px 7px;}"
        "QComboBox#eventLogFilter QAbstractItemView{color:%1;background:%4;"
        "selection-color:%1;selection-background-color:%5;}")
        .arg(ink.name(),surface.name(),border.name(),popupSurface.name(),selection.name()));

    auto* popup=m_filter->view();
    QPalette popupPalette=popup->palette();
    popupPalette.setColor(QPalette::Text,ink);
    popupPalette.setColor(QPalette::WindowText,ink);
    popupPalette.setColor(QPalette::Base,popupSurface);
    popupPalette.setColor(QPalette::Window,popupSurface);
    popupPalette.setColor(QPalette::Highlight,selection);
    popupPalette.setColor(QPalette::HighlightedText,ink);
    popup->setPalette(popupPalette);
    popup->setAutoFillBackground(true);
    popup->setStyleSheet(QStringLiteral(
        "QListView#eventLogFilterPopup{background:%1;color:%2;border:1px solid %3;"
        "selection-background-color:%4;selection-color:%2;}"
        "QListView#eventLogFilterPopup::item{background:%1;color:%2;min-height:22px;}"
        "QListView#eventLogFilterPopup::item:selected{background:%4;color:%2;}"
        "QListView#eventLogFilterPopup::item:hover{background:%4;color:%2;}")
        .arg(popupSurface.name(),ink.name(),border.name(),selection.name()));
    for(int i=0;i<m_filter->count();++i)
        m_filter->setItemData(i,QBrush(ink),Qt::ForegroundRole);
    // Keep the current filtered ID index: theme changes must never rebuild
    // 50,000 rows or move the user's scroll position.
    static_cast<EventLogModel*>(m_model)->setLight(light);
}
void EventLogWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if(event->type()==QEvent::ThemeChange)applyTheme();
}

bool EventLogWidget::parseEntry(const QVariant& value,Entry* out) {
    if(!out || value.metaType().id()!=QMetaType::QVariantMap)return false;
    const QVariantMap data=value.toMap();
    if(data.isEmpty())return false;
    for(auto it=data.cbegin();it!=data.cend();++it)
        if(it.key()!=QStringLiteral("timestamp") && it.key()!=QStringLiteral("level") &&
           it.key()!=QStringLiteral("source") && it.key()!=QStringLiteral("message"))return false;
    const QVariant v=data.value(QStringLiteral("message"));
    if(v.metaType().id()!=QMetaType::QString)return false;
    out->message=v.toString();
    if(out->message.isEmpty() || out->message.size()>1024)return false;
    if(data.contains(QStringLiteral("level"))) {
        if(data.value(QStringLiteral("level")).metaType().id()!=QMetaType::QString)return false;
        out->level=normalizedLevel(data.value(QStringLiteral("level")).toString());
        if(out->level.isEmpty())return false;
    } else out->level=QStringLiteral("INFO");
    if(data.contains(QStringLiteral("source"))) {
        if(data.value(QStringLiteral("source")).metaType().id()!=QMetaType::QString)return false;
        out->source=data.value(QStringLiteral("source")).toString();
        if(out->source.size()>80)return false;
    }
    if(data.contains(QStringLiteral("timestamp"))) {
        if(data.value(QStringLiteral("timestamp")).metaType().id()!=QMetaType::QString)return false;
        const QString stamp=data.value(QStringLiteral("timestamp")).toString();
        const auto parsed=QDateTime::fromString(stamp,Qt::ISODateWithMs);
        if(!parsed.isValid())return false;
        out->timestamp=parsed.toLocalTime().toString(Qt::ISODateWithMs);
    } else out->timestamp=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    return true;
}

void EventLogWidget::trimToCapacity() {
    const int excess=int(m_entries.size())-m_maxEntries;
    for(int i=0;i<excess;++i)m_entries.pop_front();
    m_firstId+=std::uint64_t(std::max(0,excess));
}

bool EventLogWidget::replaceEvents(const QVariantList& values) {
    if(values.size()>m_maxEntries)return false;
    QVector<Entry> parsed;parsed.reserve(values.size());
    for(const auto& value:values) {
        Entry e;
        if(!parseEntry(value,&e))return false;
        parsed.append(std::move(e));
    }
    m_entries.clear();
    m_firstId=m_nextId;
    for(auto& entry:parsed)m_entries.push_back(std::move(entry));
    m_nextId+=std::uint64_t(parsed.size());
    refresh();
    return true;
}

bool EventLogWidget::appendEvents(const QVariant& value) {
    const QVariantList input=value.metaType().id()==QMetaType::QVariantList ? value.toList() : QVariantList{value};
    if(input.isEmpty() || input.size()>m_maxEntries)return false;
    QVector<Entry> parsed;parsed.reserve(input.size());
    for(const auto& element:input) {
        Entry entry;
        if(!parseEntry(element,&entry))return false;
        parsed.push_back(std::move(entry));
    }
    const int evict=std::max(0,int(m_entries.size())+int(parsed.size())-m_maxEntries);
    const bool wasVisible=isVisible();
    if(!wasVisible || m_viewDirty) {
        // Hidden pages do not send row notifications for every background event.
        for(int i=0;i<evict;++i)m_entries.pop_front();
        m_firstId+=std::uint64_t(evict);
        for(auto& entry:parsed)m_entries.push_back(std::move(entry));
        m_nextId+=std::uint64_t(parsed.size());
        m_viewDirty=true;
        if(wasVisible)refresh();
        return true;
    }

    auto* model=static_cast<EventLogModel*>(m_model);
    auto* scroll=m_table->verticalScrollBar();
    const int oldScroll=scroll->value();
    const bool atBottom=oldScroll>=scroll->maximum()-2;
    // Stable visible event ID preserves the user's browsing position even
    // after rows above it expire. If their event itself expires, anchor to
    // the oldest surviving visible row.
    const int topRow=m_table->rowAt(1);
    const auto anchorId=model->visibleIdAt(topRow>=0?topRow:0);
    model->dropOldest(evict);
    const std::uint64_t firstNew=m_nextId;
    for(auto& entry:parsed)m_entries.push_back(std::move(entry));
    m_nextId+=std::uint64_t(parsed.size());
    model->appendRange(firstNew,m_nextId);
    if(m_autoScroll && atBottom)m_table->scrollToBottom();
    else if(evict>0) {
        // Qt normally adjusts the scroll position when top rows are removed.
        // Only reposition if the event the user was reading actually moved;
        // issuing scrollTo() for every timer event would itself create churn.
        const int currentTop=m_table->rowAt(1);
        if(currentTop>=0 && model->visibleIdAt(currentTop)==anchorId) {
            // Already anchored; no viewport operation required.
        } else {
            const int anchoredRow=model->indexAtOrAfter(anchorId);
            if(anchoredRow>=0)
                m_table->scrollTo(model->index(anchoredRow,0),QAbstractItemView::PositionAtTop);
        }
    } else scroll->setValue(oldScroll);
    return true;
}

bool EventLogWidget::applyEvents(const QVariant& payload) {
    if(payload.metaType().id()==QMetaType::QVariantList)return replaceEvents(payload.toList());
    if(payload.metaType().id()!=QMetaType::QVariantMap)return false;
    const QVariantMap operation=payload.toMap();
    if(operation.size()!=1)return false;
    if(operation.value(QStringLiteral("clear")).metaType().id()==QMetaType::Bool &&
       operation.value(QStringLiteral("clear")).toBool()) {clearEvents();return true;}
    if(operation.contains(QStringLiteral("append")))return appendEvents(operation.value(QStringLiteral("append")));
    return false;
}

void EventLogWidget::clearEvents() {m_entries.clear();m_firstId=m_nextId;refresh();}
void EventLogWidget::setFilter(const QString& level) {
    const QString normalized=level.trimmed().toUpper();
    const int i=m_filter->findText(normalized.isEmpty() || normalized==QStringLiteral("ALL") ?
                                   QStringLiteral("All") : normalized);
    if(i>=0)m_filter->setCurrentIndex(i);
}
int EventLogWidget::visibleEntryCount() const {
    const QString filter=m_filter->currentText();
    if(filter==QStringLiteral("All"))return m_entries.size();
    int count=0;
    for(const auto& entry:m_entries)if(entry.level==filter)++count;
    return count;
}
void EventLogWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if(m_viewDirty)refresh();
}
void EventLogWidget::refresh() {
    if(!m_table)return;
    auto* scroll=m_table->verticalScrollBar();
    const int oldScroll=scroll->value();
    const bool atBottom=oldScroll>=scroll->maximum()-2;
    static_cast<EventLogModel*>(m_model)->resetRows(m_filter->currentText(),
                                                    InstrumentVisuals::light(this));
    if(m_autoScroll && atBottom)m_table->scrollToBottom();
    else scroll->setValue(qMin(oldScroll,scroll->maximum()));
    m_viewDirty=false;
}
QString EventLogWidget::csvText() const {
    QString result=QStringLiteral("Timestamp,Level,Source,Message\r\n");
    for(const auto& e:m_entries)
        result+=csvField(e.timestamp)+','+csvField(e.level)+','+csvField(e.source)+','+
                csvField(e.message)+QStringLiteral("\r\n");
    return result;
}
void EventLogWidget::copySelected() {
    QStringList out;
    auto rows=m_table->selectionModel()->selectedRows();
    std::sort(rows.begin(),rows.end(),[](const QModelIndex& a,const QModelIndex& b){return a.row()<b.row();});
    for(const QModelIndex& selected:rows) {
        QStringList fields;
        for(int column=0;column<4;++column)
            fields.append(m_model->index(selected.row(),column).data().toString());
        out.append(fields.join(QLatin1Char('\t')));
    }
    if(!out.isEmpty())QApplication::clipboard()->setText(out.join(QLatin1Char('\n')));
}
void EventLogWidget::exportCsv() {
    const QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Export Event Log"),
         QStringLiteral("WFScript_Event_Log.csv"),QStringLiteral("CSV Files (*.csv)"));
    if(path.isEmpty())return;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)) {qWarning("WFScript: could not create event log CSV");return;}
    const QByteArray bytes=QByteArray("\xEF\xBB\xBF")+csvText().toUtf8();
    if(file.write(bytes)!=bytes.size() || !file.commit())
        qWarning("WFScript: could not finish event log CSV export");
}
