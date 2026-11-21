#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QVector>
#include <QStringList>
#include <QSet>
#include <functional>
#include <utility>

class QTableWidget;
class QTableView;
class QAbstractTableModel;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPlainTextEdit;
class ScriptEditor;
class QTabWidget;
class QShowEvent;
class RunInformationEditor;
class QLabel;

// Visual editor for the same schemaVersion:1 definition consumed by native
// TestRunner.loadDefinition(). Editing never executes test code or devices.
// The self-contained .wfpanel continues to own its embedded .mjs library.
class TestSequenceEditor final : public QDialog {
public:
    explicit TestSequenceEditor(const QJsonObject& definition,
                                const QJsonObject& runInformation,
                                const QStringList& namedExports,QWidget* parent=nullptr,
                                const QJsonObject& reportStorage={},
                                const QString& libraryName={},const QString& librarySource={});
    QJsonObject definition() const { return m_definition; }
    QJsonObject runInformation() const {return m_runInformation;}
    QJsonObject reportStorage() const {return m_reportStorage;}
    QString librarySource() const {return m_librarySource;}
    using SaveHandler=std::function<bool(const QJsonObject&,const QJsonObject&,
        const QJsonObject&,const QString&)>;
    void setSaveHandler(SaveHandler handler) {m_saveHandler=std::move(handler);}

    // Only a literal JSON object assigned to `export const definition` is
    // editable. All other library code (including withSettle()) is retained.
    static bool extractModuleDefinition(const QString& source,QJsonObject* out,
                                         int* begin=nullptr,int* end=nullptr,
                                         QString* error=nullptr);
    static bool replaceModuleDefinition(const QString& source,const QJsonObject& definition,
                                         QString* result,QString* error=nullptr);
    static bool validateDefinition(const QJsonObject& definition,QString* error=nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void reject() override;

private:
    bool m_firstShow=true;
    bool m_modified=false;
    bool m_stepDirty=false;
    bool m_runInfoChanged=false;
    QJsonObject m_definition;
    QJsonObject m_runInformation;
    QJsonObject m_reportStorage;
    QVector<QJsonObject> m_steps;
    QStringList m_exports;
    QString m_libraryName,m_librarySource;
    SaveHandler m_saveHandler;
    RunInformationEditor* m_runEditor=nullptr;
    QCheckBox* m_reportAutoSave=nullptr;
    QLineEdit* m_reportDirectory=nullptr;
    QLabel* m_reportPath=nullptr;
    ScriptEditor* m_sourceEditor=nullptr;
    QTreeWidget* m_table=nullptr;
    QVector<int> m_selectedPath;
    QSet<QString> m_collapsedIds;
    QLineEdit *m_id=nullptr,*m_title=nullptr,*m_sequenceId=nullptr,
              *m_tableId=nullptr,*m_logId=nullptr,*m_summaryId=nullptr,
              *m_stepId=nullptr,*m_stepName=nullptr;
    QComboBox *m_group=nullptr,*m_type=nullptr,*m_mode=nullptr,
              *m_handler=nullptr,*m_start=nullptr,*m_poll=nullptr,*m_finish=nullptr;
    QSpinBox *m_totalTimeout=nullptr,*m_timeout=nullptr,*m_interval=nullptr,*m_duration=nullptr;
    QCheckBox* m_stopOnFailure=nullptr;
    QWidget* m_conditionDetails=nullptr;
    QWidget* m_conditionEvaluation=nullptr;
    QLineEdit *m_conditionId=nullptr,*m_conditionName=nullptr,*m_conditionPath=nullptr,*m_conditionValue=nullptr;
    QComboBox *m_conditionSource=nullptr,*m_conditionKey=nullptr,*m_conditionStep=nullptr,
              *m_conditionOperator=nullptr,*m_conditionValueType=nullptr;
    QTableWidget* m_variablesTable=nullptr;
    bool m_variablesDirty=false;
    QCheckBox* m_structuredResults=nullptr;
    QTableWidget* m_rulesTable=nullptr;
    QLabel* m_rulesHeading=nullptr;
    QDoubleSpinBox *m_limitLow=nullptr,*m_limitHigh=nullptr;
    QLineEdit *m_numericUnit=nullptr,*m_numericWidget=nullptr;
    QPlainTextEdit* m_preview=nullptr;
    QWidget* m_executionTab=nullptr;
    QTableWidget* m_executionTable=nullptr; // unchanged small-definition editor
    QTableView* m_executionVirtualTable=nullptr; // >300 rows: no per-cell widgets
    QAbstractTableModel* m_executionVirtualModel=nullptr;
    QLabel* m_executionSummary=nullptr;
    QComboBox* m_executionScope=nullptr;
    QPlainTextEdit* m_description=nullptr;
    QPlainTextEdit* m_parameters=nullptr;
    QLineEdit *m_loopId=nullptr,*m_loopName=nullptr,*m_loopIndex=nullptr;
    QPlainTextEdit* m_loopDescription=nullptr;
    QComboBox* m_loopKind=nullptr;
    QTableWidget* m_axisTable=nullptr;
    QSpinBox* m_loopRepeatCount=nullptr;
    QCheckBox* m_sweepRepeat=nullptr;
    QWidget* m_loopDetails=nullptr;
    QLabel* m_loopPreview=nullptr;
    QCheckBox* m_relativeLimits=nullptr;
    QLineEdit* m_limitParameter=nullptr;
    QDoubleSpinBox *m_lowOffset=nullptr,*m_highOffset=nullptr;
    QWidget* m_detailColumns=nullptr;
    QTabWidget* m_tabs=nullptr;
    QLabel* m_hint=nullptr;
    QLabel* m_stepOverview=nullptr;
    QLabel* m_countLabel=nullptr;
    int m_selected=-1;
    bool m_loading=false;
    void loadDocument();
    void populateTable(int selected=-1);
    void loadStep(int row);
    void loadPath(const QVector<int>& path);
    QJsonObject nodeAt(const QVector<int>& path) const;
    bool writeNode(const QVector<int>& path,const QJsonObject& node);
    QJsonArray siblingArray(const QVector<int>& path) const;
    bool writeSiblings(const QVector<int>& path,const QJsonArray& siblings);
    void selectPath(const QVector<int>& path);
    void populatePath(const QVector<int>& path);
    void addContainer(bool repeat);
    void addConditional();
    void addElseBranch();
    void refreshConditionPicker();
    bool collectVariableDeclarations(QJsonArray* result,QString* error) const;
    void addChild();
    void indentStep();
    void outdentStep();
    void duplicateStep();
    void addAxis(const QJsonObject& axis={});
    bool readAxes(QJsonObject* loop,QString* error) const;
    void setContainerVisibility();
    bool applyStep(QString* error=nullptr);
    bool collectDocument(QJsonObject* output,QString* error=nullptr);
    void showPreview();
    void showExecutionPreview();
    void openFunction(const QString& name);
    void editHandler(QComboBox* combo,const QString& role);
    void setModeVisibility();
    void acceptChanges();
    void importJson();
    void exportJson();
    void addStep();
    void removeStep();
    void moveStep(int direction);
};
