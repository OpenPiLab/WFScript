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
class QTableWidget;
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
    QTableWidget* m_history=nullptr;
    QTabWidget* m_details=nullptr;
    QPlainTextEdit* m_overview=nullptr;
    QPlainTextEdit* m_json=nullptr;
    QTableWidget* m_steps=nullptr;
    QTableWidget* m_measurements=nullptr;
    QTableWidget* m_runInfo=nullptr;
    QPushButton* m_exportJson=nullptr;
    QPushButton* m_exportCsv=nullptr;
    QPushButton* m_openFile=nullptr;

    void refresh();
    void applyFilters();
    void selectHistory();
    void clearDetail(const QString& message);
    void showDetail(const QJsonObject& record,const QString& path);
    void chooseFolder();
    void openFolder();
    void openFile();
    void exportSelected(const QString& kind);
};
