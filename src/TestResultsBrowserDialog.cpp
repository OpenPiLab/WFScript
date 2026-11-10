#include "TestResultsBrowserDialog.h"

#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString displayValue(const QJsonValue& value) {
    if(value.isUndefined() || value.isNull())return {};
    if(value.isString())return value.toString();
    if(value.isBool())return value.toBool()?QStringLiteral("true"):QStringLiteral("false");
    if(value.isDouble())return QString::number(value.toDouble(),'g',15);
    return QString::fromUtf8(QJsonDocument::fromVariant(value.toVariant()).toJson(QJsonDocument::Compact));
}
QString timeLocal(const QString& iso) {
    const QDateTime date=QDateTime::fromString(iso,Qt::ISODateWithMs);
    return date.isValid()?date.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")):iso;
}
QString stateLabel(const QString& state) {
    if(state==QStringLiteral("passed"))return QStringLiteral("PASS");
    if(state==QStringLiteral("failed"))return QStringLiteral("FAIL");
    if(state==QStringLiteral("error"))return QStringLiteral("ERROR");
    if(state==QStringLiteral("aborted"))return QStringLiteral("ABORTED");
    return state.toUpper();
}
QColor stateColor(const QString& state) {
    if(state==QStringLiteral("passed"))return QColor(QStringLiteral("#2b9b70"));
    if(state==QStringLiteral("failed") || state==QStringLiteral("error"))
        return QColor(QStringLiteral("#d95b69"));
    return QColor(QStringLiteral("#c18a37"));
}
QTableWidget* makeTable(const QString& objectName,const QStringList& headers,QWidget* parent) {
    auto* table=new QTableWidget(parent);
    table->setObjectName(objectName);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}
void cell(QTableWidget* table,int row,int column,const QString& value) {
    auto* item=new QTableWidgetItem(value);
    item->setToolTip(value);
    table->setItem(row,column,item);
}
QString summarize(const QJsonObject& record,const QString& path) {
    const QJsonObject instrument=record.value(QStringLiteral("instrument")).toObject();
    const QJsonObject summary=record.value(QStringLiteral("summary")).toObject();
    const QStringList lines={
        QStringLiteral("Result: %1").arg(stateLabel(record.value(QStringLiteral("state")).toString())),
        QStringLiteral("Test: %1").arg(record.value(QStringLiteral("title")).toString()),
        QStringLiteral("Project: %1").arg(record.value(QStringLiteral("projectName")).toString()),
        QStringLiteral("Execution ID: %1").arg(record.value(QStringLiteral("executionId")).toString()),
        QStringLiteral("Started (local): %1").arg(timeLocal(record.value(QStringLiteral("startedAtUtc")).toString())),
        QStringLiteral("Finished (local): %1").arg(timeLocal(record.value(QStringLiteral("finishedAtUtc")).toString())),
        QStringLiteral("Elapsed: %1 ms").arg(displayValue(record.value(QStringLiteral("elapsedMs")))),
        QStringLiteral("Definition ID: %1").arg(displayValue(record.value(QStringLiteral("definitionId")))),
        QStringLiteral("Definition SHA-256: %1").arg(displayValue(record.value(QStringLiteral("definitionSha256")))),
        QStringLiteral("Instrument mode: %1").arg(displayValue(instrument.value(QStringLiteral("mode")))),
        QStringLiteral("Instrument: %1").arg(displayValue(instrument.value(QStringLiteral("name")))),
        QStringLiteral("Device serial: %1").arg(displayValue(instrument.value(QStringLiteral("serial")))),
        QStringLiteral("Stored report: %1").arg(path),
        QString(),
        QStringLiteral("Execution summary:"),
        QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Indented))
    };
    return lines.join(QLatin1Char('\n'));
}
}

TestResultsBrowserDialog::TestResultsBrowserDialog(const QString& initialDirectory,QWidget* parent)
    :QDialog(parent) {
    setObjectName(QStringLiteral("testResultsBrowserDialog"));
    setWindowTitle(QStringLiteral("Test Results Browser — WFScript"));
    setWindowFlag(Qt::Window,true);
    setModal(false);
    resize(1190,780);
    setMinimumSize(760,500);

    auto* root=new QVBoxLayout(this);
    auto* folderRow=new QHBoxLayout;
    folderRow->addWidget(new QLabel(QStringLiteral("Results folder:"),this));
    m_folder=new QLineEdit(this);
    m_folder->setObjectName(QStringLiteral("historyFolder"));
    m_folder->setReadOnly(true);
    folderRow->addWidget(m_folder,1);
    auto* browse=new QPushButton(QStringLiteral("Choose..."),this);
    auto* reveal=new QPushButton(QStringLiteral("Open Folder"),this);
    auto* refreshButton=new QPushButton(QStringLiteral("Refresh"),this);
    folderRow->addWidget(browse);folderRow->addWidget(reveal);folderRow->addWidget(refreshButton);
    root->addLayout(folderRow);
    connect(browse,&QPushButton::clicked,this,&TestResultsBrowserDialog::chooseFolder);
    connect(reveal,&QPushButton::clicked,this,&TestResultsBrowserDialog::openFolder);
    connect(refreshButton,&QPushButton::clicked,this,&TestResultsBrowserDialog::refresh);

    auto* filterRow=new QHBoxLayout;
    m_search=new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("historySearch"));
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(QStringLiteral("Search UUT, operator, test, execution ID, or any Run Information field"));
    filterRow->addWidget(m_search,1);
    m_verdict=new QComboBox(this);
    m_verdict->setObjectName(QStringLiteral("historyVerdict"));
    m_verdict->addItem(QStringLiteral("All results"),QStringLiteral("all"));
    m_verdict->addItem(QStringLiteral("PASS"),QStringLiteral("passed"));
    m_verdict->addItem(QStringLiteral("FAIL"),QStringLiteral("failed"));
    m_verdict->addItem(QStringLiteral("ERROR"),QStringLiteral("error"));
    m_verdict->addItem(QStringLiteral("ABORTED"),QStringLiteral("aborted"));
    filterRow->addWidget(m_verdict);
    root->addLayout(filterRow);
    connect(m_search,&QLineEdit::textChanged,this,&TestResultsBrowserDialog::applyFilters);
    connect(m_verdict,QOverload<int>::of(&QComboBox::currentIndexChanged),this,
            [this](int){applyFilters();});

    m_status=new QLabel(this);
    m_status->setObjectName(QStringLiteral("historyStatus"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* splitter=new QSplitter(Qt::Vertical,this);
    m_history=makeTable(QStringLiteral("historyTable"),
        {QStringLiteral("Completed (local)"),QStringLiteral("UUT"),QStringLiteral("Result"),
         QStringLiteral("Test"),QStringLiteral("Duration"),QStringLiteral("Mode"),
         QStringLiteral("Execution ID")},splitter);
    m_history->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    m_history->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    m_history->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    m_history->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
    m_history->horizontalHeader()->setSectionResizeMode(4,QHeaderView::ResizeToContents);
    m_history->horizontalHeader()->setSectionResizeMode(5,QHeaderView::ResizeToContents);
    connect(m_history,&QTableWidget::itemSelectionChanged,this,&TestResultsBrowserDialog::selectHistory);
    splitter->addWidget(m_history);

    auto* inspection=new QWidget(splitter);
    auto* inspectionLayout=new QVBoxLayout(inspection);
    inspectionLayout->setContentsMargins(0,2,0,0);
    m_recordHeader=new QLabel(QStringLiteral("Select a completed execution to inspect its archived record."),inspection);
    m_recordHeader->setObjectName(QStringLiteral("historyRecordHeader"));
    m_recordHeader->setTextFormat(Qt::PlainText);
    inspectionLayout->addWidget(m_recordHeader);

    m_details=new QTabWidget(inspection);
    m_details->setObjectName(QStringLiteral("historyDetailTabs"));
    m_overview=new QPlainTextEdit(m_details);
    m_overview->setReadOnly(true);
    m_overview->setObjectName(QStringLiteral("historyOverview"));
    m_details->addTab(m_overview,QStringLiteral("Overview"));
    m_steps=makeTable(QStringLiteral("historySteps"),
         {QStringLiteral("Group"),QStringLiteral("Step"),QStringLiteral("Type"),
          QStringLiteral("Status"),QStringLiteral("ms"),QStringLiteral("Detail")},m_details);
    m_details->addTab(m_steps,QStringLiteral("Sequence Steps"));
    m_measurements=makeTable(QStringLiteral("historyMeasurements"),
         {QStringLiteral("Name"),QStringLiteral("Step ID"),QStringLiteral("Actual"),
          QStringLiteral("Low"),QStringLiteral("High"),QStringLiteral("Unit"),
          QStringLiteral("Result")},m_details);
    m_details->addTab(m_measurements,QStringLiteral("Measurements"));
    m_runInfo=makeTable(QStringLiteral("historyRunInfo"),
        {QStringLiteral("Run Information field"),QStringLiteral("Value")},m_details);
    m_details->addTab(m_runInfo,QStringLiteral("Run Information"));
    m_json=new QPlainTextEdit(m_details);
    m_json->setObjectName(QStringLiteral("historyRawJson"));
    m_json->setReadOnly(true);
    m_details->addTab(m_json,QStringLiteral("Original JSON"));
    inspectionLayout->addWidget(m_details,1);
    splitter->addWidget(inspection);
    splitter->setStretchFactor(0,3);
    splitter->setStretchFactor(1,4);
    root->addWidget(splitter,1);

    auto* bottom=new QHBoxLayout;
    bottom->addStretch();
    m_openFile=new QPushButton(QStringLiteral("Open JSON File"),this);
    m_exportJson=new QPushButton(QStringLiteral("Export JSON Copy..."),this);
    m_exportCsv=new QPushButton(QStringLiteral("Export CSV..."),this);
    auto* close=new QPushButton(QStringLiteral("Close"),this);
    bottom->addWidget(m_openFile);bottom->addWidget(m_exportJson);
    bottom->addWidget(m_exportCsv);bottom->addWidget(close);
    root->addLayout(bottom);
    connect(m_openFile,&QPushButton::clicked,this,&TestResultsBrowserDialog::openFile);
    connect(m_exportJson,&QPushButton::clicked,this,[this]{exportSelected(QStringLiteral("json"));});
    connect(m_exportCsv,&QPushButton::clicked,this,[this]{exportSelected(QStringLiteral("csv"));});
    connect(close,&QPushButton::clicked,this,&QDialog::close);
    clearDetail(QStringLiteral("Select a completed execution to view its saved results."));
    setResultsDirectory(initialDirectory);
}

void TestResultsBrowserDialog::setResultsDirectory(const QString& directory) {
    m_directory=directory.isEmpty()?QString():QDir::cleanPath(directory);
    m_folder->setText(m_directory);
    refresh();
}

void TestResultsBrowserDialog::refresh() {
    const QString previous=m_loadedPath;
    QString error;
    m_entries=TestResultsHistory::scan(m_directory,5000,&error);
    m_status->setProperty("scanWarning",error);
    applyFilters();
    // Keep the previous selection when refreshing an archive folder.
    if(!previous.isEmpty()) {
        for(int row=0;row<m_history->rowCount();++row) {
            const auto* item=m_history->item(row,0);
            if(item && item->data(Qt::UserRole).toString()==previous) {
                m_history->selectRow(row);
                return;
            }
        }
    }
}
void TestResultsBrowserDialog::applyFilters() {
    const QString needle=m_search->text().trimmed();
    const QString verdict=m_verdict->currentData().toString();
    const QString selected=m_loadedPath;
    m_history->blockSignals(true);
    m_history->setSortingEnabled(false);
    m_history->clearContents();
    m_history->setRowCount(0);
    int visible=0;
    int pass=0,fail=0,error=0,aborted=0;
    for(const ArchivedTestSummary& entry:m_entries) {
        if((verdict!=QStringLiteral("all") && entry.state!=verdict) ||
           (!needle.isEmpty() && !entry.searchText.contains(needle,Qt::CaseInsensitive)))continue;
        const int row=m_history->rowCount();
        m_history->insertRow(row);
        cell(m_history,row,0,timeLocal(entry.finishedAtUtc));
        m_history->item(row,0)->setData(Qt::UserRole,entry.path);
        cell(m_history,row,1,entry.uutId);
        cell(m_history,row,2,stateLabel(entry.state));
        m_history->item(row,2)->setForeground(stateColor(entry.state));
        cell(m_history,row,3,entry.testName);
        cell(m_history,row,4,QString::number(entry.elapsedMs)+QStringLiteral(" ms"));
        cell(m_history,row,5,entry.instrumentMode);
        cell(m_history,row,6,entry.executionId);
        ++visible;
        if(entry.state==QStringLiteral("passed"))++pass;
        else if(entry.state==QStringLiteral("failed"))++fail;
        else if(entry.state==QStringLiteral("error"))++error;
        else if(entry.state==QStringLiteral("aborted"))++aborted;
    }
    m_history->setSortingEnabled(true);
    m_history->sortItems(0,Qt::DescendingOrder);
    m_history->blockSignals(false);
    bool restored=false;
    for(int row=0;row<m_history->rowCount();++row) {
        const auto* item=m_history->item(row,0);
        if(item && item->data(Qt::UserRole).toString()==selected) {
            m_history->selectRow(row);restored=true;break;
        }
    }
    if(!restored) {
        if(m_history->rowCount()>0)m_history->selectRow(0);
        else clearDetail(QStringLiteral("No matching archived executions."));
    }
    const QString warning=m_status->property("scanWarning").toString();
    QString summary=QStringLiteral("%1 of %2 archived runs shown  |  %3 PASS, %4 FAIL, %5 ERROR, %6 ABORTED")
        .arg(visible).arg(m_entries.size()).arg(pass).arg(fail).arg(error).arg(aborted);
    if(!warning.isEmpty())summary+=QStringLiteral("\n")+warning;
    if(!QDir(m_directory).exists() && !m_directory.isEmpty())
        summary+=QStringLiteral("\nThe folder will appear after the first archived run.");
    m_status->setText(summary);
}
void TestResultsBrowserDialog::selectHistory() {
    const auto selected=m_history->selectedItems();
    if(selected.isEmpty())return;
    const int row=selected.first()->row();
    const auto* item=m_history->item(row,0);
    if(!item)return;
    const QString path=item->data(Qt::UserRole).toString();
    if(path==m_loadedPath && !m_record.isEmpty())return;
    QJsonObject record;
    QString error;
    if(!TestResultsHistory::read(path,&record,&error)) {
        clearDetail(error);return;
    }
    showDetail(record,path);
}
void TestResultsBrowserDialog::clearDetail(const QString& message) {
    m_record={};m_loadedPath.clear();
    m_recordHeader->setText(message);
    m_overview->clear();m_json->clear();
    m_steps->setRowCount(0);m_measurements->setRowCount(0);m_runInfo->setRowCount(0);
    m_openFile->setEnabled(false);m_exportJson->setEnabled(false);m_exportCsv->setEnabled(false);
}
void TestResultsBrowserDialog::showDetail(const QJsonObject& record,const QString& path) {
    m_record=record;m_loadedPath=path;
    m_recordHeader->setText(QStringLiteral("%1 — %2 — %3")
        .arg(stateLabel(record.value(QStringLiteral("state")).toString()),
             record.value(QStringLiteral("title")).toString(),
             record.value(QStringLiteral("executionId")).toString()));
    m_overview->setPlainText(summarize(record,path));
    const QJsonArray steps=record.value(QStringLiteral("steps")).toArray();
    m_steps->setRowCount(steps.size());
    for(int row=0;row<steps.size();++row) {
        const QJsonObject s=steps.at(row).toObject();
        for(int col=0;col<6;++col) {
            const QStringList keys={QStringLiteral("group"),QStringLiteral("name"),QStringLiteral("type"),
                QStringLiteral("status"),QStringLiteral("elapsedMs"),QStringLiteral("detail")};
            cell(m_steps,row,col,displayValue(s.value(keys.at(col))));
        }
    }
    const QJsonArray measurements=record.value(QStringLiteral("measurements")).toArray();
    m_measurements->setRowCount(measurements.size());
    const QStringList measurementFields={QStringLiteral("name"),QStringLiteral("stepId"),QStringLiteral("value"),
        QStringLiteral("low"),QStringLiteral("high"),QStringLiteral("unit"),QStringLiteral("status")};
    for(int row=0;row<measurements.size();++row) {
        const QJsonObject measurement=measurements.at(row).toObject();
        for(int col=0;col<measurementFields.size();++col)
            cell(m_measurements,row,col,displayValue(measurement.value(measurementFields.at(col))));
    }
    const QJsonObject info=record.value(QStringLiteral("runInfo")).toObject();
    m_runInfo->setRowCount(info.size());
    int row=0;
    for(auto it=info.constBegin();it!=info.constEnd();++it,++row) {
        cell(m_runInfo,row,0,it.key());
        cell(m_runInfo,row,1,displayValue(it.value()));
    }
    m_json->setPlainText(QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Indented)));
    m_openFile->setEnabled(true);m_exportJson->setEnabled(true);m_exportCsv->setEnabled(true);
}
void TestResultsBrowserDialog::chooseFolder() {
    const QString selected=QFileDialog::getExistingDirectory(this,QStringLiteral("Select archived test results folder"),m_directory);
    if(!selected.isEmpty())setResultsDirectory(selected);
}
void TestResultsBrowserDialog::openFolder() {
    if(!QDir(m_directory).exists()) {
        QMessageBox::information(this,QStringLiteral("Test Results"),QStringLiteral("Results folder does not exist yet."));
        return;
    }
    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(m_directory)))
        QMessageBox::warning(this,QStringLiteral("Test Results"),QStringLiteral("Could not open the results folder."));
}
void TestResultsBrowserDialog::openFile() {
    if(m_loadedPath.isEmpty())return;
    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(m_loadedPath)))
        QMessageBox::warning(this,QStringLiteral("Test Results"),QStringLiteral("Could not open the JSON report."));
}
void TestResultsBrowserDialog::exportSelected(const QString& kind) {
    if(m_loadedPath.isEmpty() || m_record.isEmpty())return;
    const QString base=QFileInfo(m_loadedPath).completeBaseName();
    const QString suggestion=QDir(QFileInfo(m_loadedPath).absolutePath())
        .filePath(base+QStringLiteral("_export.")+kind);
    const QString title=kind==QStringLiteral("csv")?QStringLiteral("Export historical measurements and steps"):
            QStringLiteral("Export historical execution JSON copy");
    const QString filter=kind==QStringLiteral("csv")?QStringLiteral("CSV (*.csv)"):QStringLiteral("JSON (*.json)");
    QString destination=QFileDialog::getSaveFileName(this,title,suggestion,filter);
    if(destination.isEmpty())return;
    if(QFileInfo(destination).suffix().isEmpty())destination+=QStringLiteral(".")+kind;
    // Never overwrite the immutable source archive from within the browser.
    if(QDir::cleanPath(QFileInfo(destination).absoluteFilePath()).compare(
          QDir::cleanPath(QFileInfo(m_loadedPath).absoluteFilePath()),
          Qt::CaseInsensitive)==0) {
        QMessageBox::warning(this,QStringLiteral("Export"),
            QStringLiteral("Cannot overwrite the original archived report. Choose another filename."));
        return;
    }
    QString error;
    if(!TestResultsHistory::exportRecord(m_record,destination,kind,&error)) {
        QMessageBox::warning(this,QStringLiteral("Export failed"),error);
    } else {
        QMessageBox::information(this,QStringLiteral("Export complete"),
            QStringLiteral("Historical report exported to:\n%1").arg(destination));
    }
}
