#pragma once

#include "TestResultsRepository.h"

#include <QDialog>
#include <QJsonObject>
#include <QList>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableView;
class ArchivedRunsModel;
class ArchivedRecordRowsModel;
class QTabWidget;

// Read-only browser for completed .json execution records. It never reads or
// mutates the live TestRunner, the Panel, instrument state, or JavaScript.
class TestResultsBrowserDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TestResultsBrowserDialog(const QString& initialDirectory,QWidget* parent=nullptr);
    void setResultsDirectory(const QString& directory);

private:
    QString m_directory;
    QString m_loadedPath;
    QJsonObject m_record;
    QList<ArchivedTestSummary> m_entries;

    QLineEdit* m_folder=nullptr;
    QLineEdit* m_search=nullptr;
    QComboBox* m_verdict=nullptr;
    QLabel* m_status=nullptr;
    QLabel* m_recordHeader=nullptr;
    QTableView* m_history=nullptr;
    ArchivedRunsModel* m_historyModel=nullptr;
    QTabWidget* m_details=nullptr;
    QPlainTextEdit* m_overview=nullptr;
    QPlainTextEdit* m_json=nullptr;
    QTableView* m_steps=nullptr;
    ArchivedRecordRowsModel* m_stepsModel=nullptr;
    QTableView* m_measurements=nullptr;
    ArchivedRecordRowsModel* m_measurementsModel=nullptr;
    QTableView* m_runInfo=nullptr;
    ArchivedRecordRowsModel* m_runInfoModel=nullptr;
    bool m_jsonRendered=false;
    QPushButton* m_exportJson=nullptr;
    QPushButton* m_exportCsv=nullptr;
    QPushButton* m_openFile=nullptr;

    void refresh();
    void applyFilters();
    void selectHistory();
    void clearDetail(const QString& message);
    void showDetail(const QJsonObject& record,const QString& path);
    void renderRawJsonIfNeeded();
    void chooseFolder();
    void openFolder();
    void openFile();
    void exportSelected(const QString& kind);
};
