#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QVector>
#include <QStringList>

class QTableWidget;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPlainTextEdit;
class QLabel;

// Visual editor for the same schemaVersion:1 definition consumed by native
// TestRunner.loadDefinition(). Editing never executes test code or devices.
// The self-contained .wfpanel continues to own its embedded .mjs library.
class TestSequenceEditor final : public QDialog {
public:
    explicit TestSequenceEditor(const QJsonObject& definition,
                                const QJsonObject& runInformation,
                                const QStringList& namedExports,QWidget* parent=nullptr,
                                const QJsonObject& reportStorage={});
    QJsonObject definition() const { return m_definition; }
    QJsonObject runInformation() const {return m_runInformation;}
    QJsonObject reportStorage() const {return m_reportStorage;}

    // Only a literal JSON object assigned to `export const definition` is
    // editable. All other library code (including withSettle()) is retained.
    static bool extractModuleDefinition(const QString& source,QJsonObject* out,
                                         int* begin=nullptr,int* end=nullptr,
                                         QString* error=nullptr);
    static bool replaceModuleDefinition(const QString& source,const QJsonObject& definition,
                                         QString* result,QString* error=nullptr);
    static bool validateDefinition(const QJsonObject& definition,QString* error=nullptr);

private:
    QJsonObject m_definition;
    QJsonObject m_runInformation;
    QJsonObject m_reportStorage;
    QVector<QJsonObject> m_steps;
    QStringList m_exports;
    QTableWidget* m_table=nullptr;
    QLineEdit *m_id=nullptr,*m_title=nullptr,*m_sequenceId=nullptr,
              *m_tableId=nullptr,*m_logId=nullptr,*m_summaryId=nullptr,
              *m_stepId=nullptr,*m_stepName=nullptr,*m_description=nullptr;
    QComboBox *m_group=nullptr,*m_type=nullptr,*m_mode=nullptr,
              *m_handler=nullptr,*m_start=nullptr,*m_poll=nullptr,*m_finish=nullptr;
    QSpinBox *m_totalTimeout=nullptr,*m_timeout=nullptr,*m_interval=nullptr,*m_duration=nullptr;
    QCheckBox* m_stopOnFailure=nullptr;
    QDoubleSpinBox *m_limitLow=nullptr,*m_limitHigh=nullptr;
    QLineEdit *m_numericUnit=nullptr,*m_numericWidget=nullptr;
    QPlainTextEdit* m_preview=nullptr;
    QLabel* m_hint=nullptr;
    int m_selected=-1;
    bool m_loading=false;
    void loadDocument();
    void populateTable(int selected=-1);
    void loadStep(int row);
    bool applyStep(QString* error=nullptr);
    bool collectDocument(QJsonObject* output,QString* error=nullptr);
    void showPreview();
    void setModeVisibility();
    void acceptChanges();
    void importJson();
    void exportJson();
    void addStep();
    void removeStep();
    void moveStep(int direction);
};
