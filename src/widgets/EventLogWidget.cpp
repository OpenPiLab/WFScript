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
#include <QStringList>
#include <QByteArray>
#include <QTableWidget>
#include <QTableWidgetItem>
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
    m_table=new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("eventLogTable"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Time"),QStringLiteral("Level"),
                                       QStringLiteral("Source"),QStringLiteral("Message")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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
    m_maxEntries=qBound(10,maxEntries,2000);
    m_autoScroll=autoScroll;
    m_showTimestamp=showTimestamp;
    m_showSource=showSource;
    trimToCapacity();
    m_heading->setText(m_title.isEmpty()?QStringLiteral("Event Log"):m_title);
    m_table->setColumnHidden(0,!m_showTimestamp);
    m_table->setColumnHidden(2,!m_showSource);
    const bool light=InstrumentVisuals::light(this);
    m_heading->setStyleSheet(light?QStringLiteral("color:#23384c;"):
                                    QStringLiteral("color:#e0ebf5;"));
    setStyleSheet(light?QStringLiteral(
        "QWidget#wfEventLogRoot{background:#edf4fa;border:1px solid #b8cbd9;border-radius:8px;}"
        "QTableWidget{background:#f4f8fc;color:#26394b;alternate-background-color:#e4edf5;border:1px solid #b8cbd9;}"
        "QHeaderView::section{background:#d9e8f2;color:#26394b;border:0;padding:5px;}"):
        QStringLiteral(
        "QWidget#wfEventLogRoot{background:#101e2f;border:1px solid #3a536b;border-radius:8px;}"
        "QTableWidget{background:#0e1c2b;color:#d6e4f1;alternate-background-color:#172b3e;border:1px solid #3a536b;}"
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
    refresh();
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
    if(m_entries.size()>m_maxEntries)m_entries.remove(0,m_entries.size()-m_maxEntries);
}

bool EventLogWidget::replaceEvents(const QVariantList& values) {
    if(values.size()>m_maxEntries)return false;
    QVector<Entry> parsed;parsed.reserve(values.size());
    for(const auto& value:values) {
        Entry e;
        if(!parseEntry(value,&e))return false;
        parsed.append(std::move(e));
    }
    m_entries=std::move(parsed);
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
    m_entries+=parsed;
    trimToCapacity();
    refresh();
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

void EventLogWidget::clearEvents() {m_entries.clear();refresh();}
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
void EventLogWidget::refresh() {
    if(!m_table)return;
    const QString filter=m_filter->currentText();
    auto* scroll=m_table->verticalScrollBar();
    const int oldScroll=scroll->value();
    const bool atBottom=oldScroll>=scroll->maximum()-2;
    const bool light=InstrumentVisuals::light(this);
    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(0);
    for(const auto& entry:m_entries) {
        if(filter!=QStringLiteral("All") && entry.level!=filter)continue;
        const int row=m_table->rowCount();
        m_table->insertRow(row);
        QString displayTime=entry.timestamp;
        const QDateTime t=QDateTime::fromString(displayTime,Qt::ISODateWithMs);
        if(t.isValid())displayTime=t.toString(QStringLiteral("HH:mm:ss.zzz"));
        const QStringList cells{displayTime,entry.level,entry.source,entry.message};
        for(int col=0;col<4;col++) {
            auto* item=new QTableWidgetItem(cells.at(col));
            item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            item->setToolTip(cells.at(col));
            if(col==1)item->setForeground(severityColor(entry.level,light));
            m_table->setItem(row,col,item);
        }
    }
    m_table->setUpdatesEnabled(true);
    if(m_autoScroll && atBottom)scroll->setValue(scroll->maximum());
    else scroll->setValue(qMin(oldScroll,scroll->maximum()));
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
    QSet<int> selected;
    for(const auto& item:m_table->selectedItems())selected.insert(item->row());
    QList<int> rows=selected.values();std::sort(rows.begin(),rows.end());
    for(const auto& row:rows) {
        QStringList fields;
        for(int col=0;col<4;col++) {
            auto* item=m_table->item(row,col);
            fields.append(item?item->text():QString());
        }
        out.append(fields.join('\t'));
    }
    if(!out.isEmpty())QApplication::clipboard()->setText(out.join('\n'));
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
