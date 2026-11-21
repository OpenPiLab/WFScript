#include "MeasurementTableWidget.h"
#include "InstrumentVisuals.h"
#include "../TestSequenceCapacity.h"
#include <QAbstractTableModel>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QFont>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QColor>
#include <QVariantMap>
#include <QBrush>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QListView>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QSaveFile>
#include <QTextStream>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QFileInfo>
#include <QPalette>
#include <QDebug>
#include <QEvent>
#include <cmath>
#include <utility>
#include <algorithm>

namespace {
struct Row {QString name,actual,limits,status;};
class MeasurementsModel final : public QAbstractTableModel {
public:
    explicit MeasurementsModel(QObject* parent):QAbstractTableModel(parent){}
    QVector<Row> rows;
    QStringList headers={QStringLiteral("Measurement"),QStringLiteral("Actual"),
                          QStringLiteral("Limits"),QStringLiteral("Result")};
    int rowCount(const QModelIndex& parent=QModelIndex()) const override {
        return parent.isValid()?0:rows.size();
    }
    int columnCount(const QModelIndex& parent=QModelIndex()) const override {
        return parent.isValid()?0:4;
    }
    QVariant headerData(int section,Qt::Orientation orientation,int role) const override {
        if(role==Qt::DisplayRole && orientation==Qt::Horizontal)return headers.value(section);
        return {};
    }
    QVariant data(const QModelIndex& index,int role) const override {
        if(!index.isValid() || index.row()<0 || index.row()>=rows.size() || index.column()<0 || index.column()>3)return {};
        const Row& r=rows.at(index.row());
        if(role==Qt::DisplayRole || role==Qt::ToolTipRole) {
            switch(index.column()) {case 0:return r.name;case 1:return r.actual;case 2:return r.limits;default:return r.status;}
        }
        if(role==Qt::TextAlignmentRole)return int(index.column()==0?Qt::AlignLeft|Qt::AlignVCenter:Qt::AlignCenter);
        if(role==Qt::ForegroundRole && index.column()==3)
            return QBrush(r.status==QLatin1String("PASS")?QColor(56,207,139):
                (r.status==QLatin1String("FAIL")||r.status==QLatin1String("ERROR"))?
                    QColor(255,114,115):QColor(168,181,193));
        if(role==Qt::FontRole && index.column()==3) {QFont f;f.setBold(true);return f;}
        return {};
    }
    void setHeaders(const QStringList& text) {
        headers=text;emit headerDataChanged(Qt::Horizontal,0,3);
    }
    void replace(QVector<Row>&& next) {beginResetModel();rows=std::move(next);endResetModel();}
    void append(QVector<Row>&& added) {
        if(added.isEmpty())return;
        const int first=rows.size(),last=first+added.size()-1;
        beginInsertRows(QModelIndex(),first,last);
        for(auto& r:added)rows.push_back(std::move(r));
        endInsertRows();
    }
};

// This proxy is only installed when the operator actively selects a result
// filter. The unfiltered 100k streaming path remains direct to source model.
class ResultFilterProxy final : public QSortFilterProxyModel {
public:
    explicit ResultFilterProxy(QObject* parent=nullptr):QSortFilterProxyModel(parent){}
    void setStatus(const QString& status) {m_status=status;invalidateRowsFilter();}
protected:
    bool filterAcceptsRow(int row,const QModelIndex& parent) const override {
        if(m_status==QLatin1String("All"))return true;
        const auto* source=sourceModel();
        return source && source->index(row,3,parent).data(Qt::DisplayRole).toString()==m_status;
    }
private:
    QString m_status=QStringLiteral("All");
};

bool parseRow(const QVariant& value,int decimals,Row* out) {
    if(value.metaType().id()!=QMetaType::QVariantMap)return false;
    const QVariantMap data=value.toMap();
    if(data.isEmpty() || !data.contains(QStringLiteral("name")))return false;
    out->name=data.value(QStringLiteral("name")).toString().left(160);
    const QString unit=data.value(QStringLiteral("unit")).toString().left(16);
    const QString forced=data.value(QStringLiteral("status")).toString().toUpper();
    if(forced==QLatin1String("ERROR") || forced==QLatin1String("SKIP")) {
        out->status=forced;out->actual=QStringLiteral("—");out->limits=QStringLiteral("—");return true;
    }
    if(!data.contains(QStringLiteral("value")) || data.value(QStringLiteral("value")).isNull()) {
        out->status=QStringLiteral("NOT TESTED");out->actual=QStringLiteral("—");out->limits=QStringLiteral("—");return true;
    }
    bool a=false,l=false,h=false;
    const double actual=data.value(QStringLiteral("value")).toDouble(&a);
    const double low=data.value(QStringLiteral("low")).toDouble(&l);
    const double high=data.value(QStringLiteral("high")).toDouble(&h);
    if(!a || !l || !h || !std::isfinite(actual) || !std::isfinite(low) ||
       !std::isfinite(high) || low>high)return false;
    const QString suffix=unit.isEmpty()?QString():QStringLiteral(" ")+unit;
    out->actual=InstrumentVisuals::number(actual,decimals)+suffix;
    out->limits=InstrumentVisuals::number(low,decimals)+QStringLiteral(" .. ")+
                InstrumentVisuals::number(high,decimals)+suffix;
    out->status=actual>=low && actual<=high?QStringLiteral("PASS"):QStringLiteral("FAIL");
    return true;
}

QString csvCell(QString value) {
    // Prevent spreadsheet formula execution when a user opens exported CSV.
    const QString trimmed=value.trimmed();
    if(!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))
        value.prepend(QLatin1Char('\''));
    value.replace(QLatin1Char('"'),QStringLiteral("\"\""));
    return QStringLiteral("\"")+value+QStringLiteral("\"");
}
}

MeasurementTableWidget::MeasurementTableWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("wfMeasurementTableRoot"));
    auto* outer=new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->setSpacing(3);
    m_toolbar=new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("wfMeasurementToolbar"));
    auto* bar=new QHBoxLayout(m_toolbar);
    bar->setContentsMargins(8,4,8,4);
    bar->setSpacing(5);
    m_heading=new QLabel(QStringLiteral("Measurement results"),m_toolbar);
    m_heading->setObjectName(QStringLiteral("measurementTableTitle"));
    auto font=m_heading->font();font.setBold(true);m_heading->setFont(font);
    bar->addWidget(m_heading,1);
    m_count=new QLabel(m_toolbar);
    m_count->setObjectName(QStringLiteral("measurementTableCount"));
    bar->addWidget(m_count);
    m_filter=new QComboBox(m_toolbar);
    m_filter->setObjectName(QStringLiteral("measurementTableFilter"));
    m_filter->setView(new QListView(m_filter));
    m_filter->addItems({QStringLiteral("All"),QStringLiteral("PASS"),QStringLiteral("FAIL"),
                        QStringLiteral("ERROR"),QStringLiteral("SKIP"),QStringLiteral("NOT TESTED")});
    m_filter->setToolTip(QStringLiteral("Filter the visible results; CSV export always includes the full history"));
    bar->addWidget(m_filter);
    m_copy=new QPushButton(QStringLiteral("Copy"),m_toolbar);
    m_copy->setObjectName(QStringLiteral("measurementTableCopy"));
    m_copy->setToolTip(QStringLiteral("Copy selected result rows as tab-separated text"));
    bar->addWidget(m_copy);
    m_clear=new QPushButton(QStringLiteral("Clear View"),m_toolbar);
    m_clear->setObjectName(QStringLiteral("measurementTableClear"));
    m_clear->setToolTip(QStringLiteral("Clear this display only; does NOT clear the Test Executive's report or its measurements"));
    bar->addWidget(m_clear);
    m_export=new QPushButton(QStringLiteral("Export CSV"),m_toolbar);
    m_export->setObjectName(QStringLiteral("measurementTableExport"));
    m_export->setToolTip(QStringLiteral("Export all displayed-history measurements, independent of active filter"));
    bar->addWidget(m_export);
    outer->addWidget(m_toolbar);

    m_view=new QTableView(this);
    m_view->setObjectName(QStringLiteral("measurementTableView"));
    m_source=new MeasurementsModel(m_view);
    m_view->setModel(m_source);
    m_proxy=new ResultFilterProxy(m_view);
    m_proxy->setSourceModel(m_source);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setFocusPolicy(Qt::StrongFocus);m_view->setWordWrap(false);
    m_view->setAlternatingRowColors(true);m_view->setShowGrid(false);
    m_view->verticalHeader()->setVisible(false);
    m_view->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    m_view->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Interactive);
    m_view->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Interactive);
    m_view->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Interactive);
    m_view->setColumnWidth(1,140);m_view->setColumnWidth(2,230);m_view->setColumnWidth(3,100);
    m_view->horizontalHeader()->setMinimumSectionSize(65);
    m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_view->verticalHeader()->setDefaultSectionSize(27);
    outer->addWidget(m_view,1);
    m_countTimer=new QTimer(this);
    m_countTimer->setObjectName(QStringLiteral("measurementCountTimer"));
    m_countTimer->setSingleShot(true);
    m_countTimer->setInterval(100); // never update label for each row in 100k stream
    connect(m_countTimer,&QTimer::timeout,this,[this]{refreshCount();});
    const auto schedule=[this]{if(!m_countTimer->isActive())m_countTimer->start();};
    connect(m_source,&QAbstractItemModel::rowsInserted,this,schedule);
    connect(m_source,&QAbstractItemModel::modelReset,this,[this]{refreshCount();});
    connect(m_filter,&QComboBox::currentTextChanged,this,[this](const QString& filter){setResultFilter(filter);});
    connect(m_copy,&QPushButton::clicked,this,[this]{copySelection();});
    connect(m_clear,&QPushButton::clicked,this,[this]{clearView();});
    connect(m_export,&QPushButton::clicked,this,[this]{exportDialog();});
    configure(3,100,m_headers);
}

void MeasurementTableWidget::configure(int decimals,int maxRows,const QString& headers,
                                       const QString& title,bool showToolbar,bool showRowCount,
                                       bool showFilter,bool showCopy,bool showExport,bool showClear,
                                       bool autoScroll) {
    m_decimals=qBound(0,decimals,6);
    m_legacyRows=qBound(1,maxRows,100);
    m_autoScroll=autoScroll;
    const QStringList titles=headers.split(QLatin1Char('|'));
    if(titles.size()==4) {
        m_headers=headers;
        static_cast<MeasurementsModel*>(m_source)->setHeaders(titles);
    }
    m_heading->setText(title.isEmpty()?QStringLiteral("Measurement results"):title.left(120));
    m_toolbar->setVisible(showToolbar);
    m_count->setVisible(showRowCount);
    m_filter->setVisible(showFilter);
    m_copy->setVisible(showCopy);
    m_export->setVisible(showExport);
    m_clear->setVisible(showClear);
    applyTheme();
    refreshCount();
}

void MeasurementTableWidget::applyTheme() {
    const bool light=InstrumentVisuals::light(this);
    m_heading->setStyleSheet(light?QStringLiteral("color:#22394c;"):QStringLiteral("color:#e0ebf5;"));
    m_count->setStyleSheet(light?QStringLiteral("color:#536b7c;"):QStringLiteral("color:#aebecf;"));
    setStyleSheet(light?QStringLiteral(
        "QWidget#wfMeasurementTableRoot{background:#edf4fa;border:1px solid #b8cbd9;border-radius:8px;}"
        "QWidget#wfMeasurementToolbar{background:transparent;border:0;}"
        "QTableView{background:#f3f8fc;color:#263b50;alternate-background-color:#e4edf5;border:1px solid #b3c6d5;}"
        "QHeaderView::section{background:#d8e7f1;color:#263b50;padding:5px;border:0px;}"
        "QPushButton{color:#263b50;background:#ffffff;border:1px solid #b3c6d5;border-radius:4px;padding:4px 8px;}"
        "QComboBox{color:#263b50;background:#ffffff;border:1px solid #b3c6d5;padding:4px;}"):
        QStringLiteral(
        "QWidget#wfMeasurementTableRoot{background:#101e2f;border:1px solid #3a536b;border-radius:8px;}"
        "QWidget#wfMeasurementToolbar{background:transparent;border:0;}"
        "QTableView{background:#0e1c2b;color:#d5e4f2;alternate-background-color:#16293b;border:1px solid #3c566e;}"
        "QHeaderView::section{background:#243a4e;color:#d5e4f2;padding:5px;border:0px;}"
        "QPushButton{color:#d5e4f2;background:#1b3349;border:1px solid #4a667c;border-radius:4px;padding:4px 8px;}"
        "QComboBox{color:#d5e4f2;background:#1b3349;border:1px solid #4a667c;padding:4px;}"));
    // Explicit popup palette: inherited Panel styles can otherwise make
    // unselected items invisible on Windows (dark text on dark popup).
    const QColor ink(light?QStringLiteral("#203347"):QStringLiteral("#e4f0f9"));
    const QColor surface(light?QStringLiteral("#ffffff"):QStringLiteral("#20384f"));
    const QColor popupSurface(light?QStringLiteral("#f4f8fc"):QStringLiteral("#16283b"));
    const QColor selection(light?QStringLiteral("#bddbe7"):QStringLiteral("#355a72"));
    QPalette palette=m_filter->palette();
    palette.setColor(QPalette::Text,ink);
    palette.setColor(QPalette::ButtonText,ink);
    palette.setColor(QPalette::Base,surface);
    palette.setColor(QPalette::Window,surface);
    palette.setColor(QPalette::HighlightedText,ink);
    palette.setColor(QPalette::Highlight,selection);
    m_filter->setPalette(palette);
    QPalette popup=m_filter->view()->palette();
    popup.setColor(QPalette::Text,ink);
    popup.setColor(QPalette::Base,popupSurface);
    popup.setColor(QPalette::Highlight,selection);
    popup.setColor(QPalette::HighlightedText,ink);
    m_filter->view()->setPalette(popup);
    // No model reset, selection change, or re-creation of result rows.
}
void MeasurementTableWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if(event->type()==QEvent::ThemeChange)applyTheme();
}

QAbstractItemModel* MeasurementTableWidget::model() const {return m_source;}
QHeaderView* MeasurementTableWidget::horizontalHeader() const {return m_view->horizontalHeader();}
QScrollBar* MeasurementTableWidget::verticalScrollBar() const {return m_view->verticalScrollBar();}
void MeasurementTableWidget::scrollToBottom() {m_view->scrollToBottom();}
int MeasurementTableWidget::displayedRows() const {return m_source->rowCount();}
QString MeasurementTableWidget::resultFilter() const {return m_filter->currentText();}
QString MeasurementTableWidget::tableTitle() const {return m_heading->text();}
QString MeasurementTableWidget::countText() const {return m_count->text();}
void MeasurementTableWidget::setResultFilter(const QString& status) {
    const int index=m_filter->findText(status,Qt::MatchFixedString);
    if(index<0)return;
    if(m_filter->currentIndex()!=index) {
        QSignalBlocker noRecursion(m_filter);
        m_filter->setCurrentIndex(index);
    }
    const QString next=m_filter->itemText(index);
    if(next==QLatin1String("All")) {
        if(m_view->model()!=m_source)m_view->setModel(m_source);
    } else {
        static_cast<ResultFilterProxy*>(m_proxy)->setStatus(next);
        if(m_view->model()!=m_proxy)m_view->setModel(m_proxy);
    }
    refreshCount();
}
void MeasurementTableWidget::refreshCount() {
    const int total=displayedRows();
    const int visible=m_view->model()->rowCount();
    m_count->setText(visible==total?QStringLiteral("%1 results").arg(total):
        QStringLiteral("%1 / %2 results").arg(visible).arg(total));
}

bool MeasurementTableWidget::setMeasurements(const QVariant& input) {
    auto* table=static_cast<MeasurementsModel*>(m_source);
    if(input.metaType().id()==QMetaType::QVariantMap) {
        const QVariantMap operation=input.toMap();
        if(operation.size()==1 && operation.value(QStringLiteral("reset")).toBool()) {
            table->replace({});refreshCount();return true;
        }
        if(operation.size()!=1 || !operation.contains(QStringLiteral("append")))return false;
        const QVariant value=operation.value(QStringLiteral("append"));
        const QVariantList rows=(value.metaType().id()==QMetaType::QVariantList)?value.toList():QVariantList{value};
        if(table->rows.size()+rows.size()>TestSequenceCapacity::maxMeasurementsPerRun)return false;
        QVector<Row> parsed;parsed.reserve(rows.size());
        for(const QVariant& item:rows) {Row row;if(!parseRow(item,m_decimals,&row))return false;parsed.push_back(std::move(row));}
        const QScrollBar* bar=verticalScrollBar();
        const bool atBottom=m_autoScroll && bar->value()>=bar->maximum()-2;
        table->append(std::move(parsed));
        if(atBottom)scrollToBottom();
        return true;
    }
    if(input.metaType().id()!=QMetaType::QVariantList &&
       input.metaType().id()!=QMetaType::QStringList)return false;
    const QVariantList rows=input.toList();
    if(rows.size()>TestSequenceCapacity::maxMeasurementsPerRun)return false;
    QVector<Row> parsed;parsed.reserve(rows.size());
    for(const QVariant& item:rows) {Row row;if(!parseRow(item,m_decimals,&row))return false;parsed.push_back(std::move(row));}
    table->replace(std::move(parsed));refreshCount();
    return true;
}
void MeasurementTableWidget::copySelection() {
    const auto selection=m_view->selectionModel()->selectedRows();
    if(selection.isEmpty())return;
    auto indexes=selection;
    std::sort(indexes.begin(),indexes.end(),[](const QModelIndex& a,const QModelIndex& b){return a.row()<b.row();});
    QString output;
    // Large bulk copies should be performed using Export CSV, not by
    // constructing unbounded clipboard content on the GUI thread.
    constexpr int maxClipboardRows=10000;
    const int n=std::min<int>(int(indexes.size()),maxClipboardRows);
    for(int i=0;i<n;++i) {
        if(i)output+=QLatin1Char('\n');
        for(int col=0;col<4;++col) {
            if(col)output+=QLatin1Char('\t');
            output+=m_view->model()->index(indexes.at(i).row(),col).data().toString();
        }
    }
    QApplication::clipboard()->setText(output);
}
void MeasurementTableWidget::clearView() {
    // This explicitly clears only the widget's presentation history.
    // The native TestRunner report, limits, and saved result are unchanged.
    setMeasurements(QVariantMap{{QStringLiteral("reset"),true}});
}
void MeasurementTableWidget::exportDialog() {
    const QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Export measurement results"),
                                                   QStringLiteral("measurements.csv"),
                                                   QStringLiteral("CSV files (*.csv);;All files (*)"));
    if(path.isEmpty())return;
    QString error;
    if(!exportCsv(path,&error))qWarning("WFScript Measurement Table: CSV export failed: %s",qPrintable(error));
}
bool MeasurementTableWidget::exportCsv(const QString& path,QString* error) const {
    if(error)error->clear();
    if(path.trimmed().isEmpty()) {if(error)*error=QStringLiteral("Missing export filename");return false;}
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
        if(error) {
            *error=file.errorString();
        }
        return false;
    }
    QTextStream stream(&file);
    auto* source=static_cast<const MeasurementsModel*>(m_source);
    for(int col=0;col<4;++col) {
        if(col)stream<<QLatin1Char(',');
        stream<<csvCell(source->headers.value(col));
    }
    stream<<QLatin1Char('\n');
    for(const Row& row:source->rows) {
        stream<<csvCell(row.name)<<QLatin1Char(',')<<csvCell(row.actual)<<QLatin1Char(',')
              <<csvCell(row.limits)<<QLatin1Char(',')<<csvCell(row.status)<<QLatin1Char('\n');
    }
    if(stream.status()!=QTextStream::Ok) {
        if(error) {
            *error=QStringLiteral("CSV write failed");
        }
        file.cancelWriting();
        return false;
    }
    if(!file.commit()) {if(error)*error=file.errorString();return false;}
    return true;
}
