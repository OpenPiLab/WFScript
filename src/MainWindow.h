#pragma once

#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <QPair>
#include <QVector>
#include <QList>
#include <QByteArray>
#include <QVariant>
#include "ScopeFrame.h"
#include "PanelProject.h"
#include <QJsonArray>

class QTabWidget;
class QSplitter;
class QDockWidget;
class QTreeWidget;
class ApiExplorer;
class QPlainTextEdit;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QAction;
class QWidget;
class QThread;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QTabBar;
class QTextDocument;
class QTimer;
class QTreeWidgetItem;
class WaveformPlot;
class ScopeAcquisitionWorker;
class ScriptEditor;
class ScriptWorker;
class PanelDesigner;
class PanelRuntime;
class DeviceWorkspace;
class TestResultsBrowserDialog;
class TestSequenceEditor;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QPointer<TestResultsBrowserDialog> m_resultsBrowser;
    QPointer<TestSequenceEditor> m_testSequenceEditor;
    QTabWidget* m_tabs = nullptr;
    QSplitter* m_scriptWorkspace = nullptr;
    QWidget* m_apiPane = nullptr;
    QAction* m_apiAction = nullptr;
    ApiExplorer* m_apiExplorer = nullptr;
    QDockWidget* m_problemsDock = nullptr;
    QTreeWidget* m_problems = nullptr;
    struct RuntimeProblem {
        QString source;
        int line=0;
        int column=0;
        QString context;
        QString message;
        QString stack;
    };
    QHash<ScriptEditor*, RuntimeProblem> m_runtimeErrors;
    QVector<RuntimeProblem> m_panelProblems;
    QTabWidget* m_workspaces = nullptr;
    DeviceWorkspace* m_deviceWorkspace = nullptr;
    PanelDesigner* m_panelDesigner = nullptr;
    QPointer<PanelRuntime> m_panelRuntime;
    QPointer<QThread> m_panelThread;
    bool m_panelStopPending=false;
    // Run Panel temporarily focuses the canvas; preserve the prior layout.
    bool m_panelAutoFocusedForRun=false;
    // True only after PanelRuntime released its resources on the worker thread.
    // A running QThread must still finish before the panel is fully stopped.
    bool m_panelWorkerCleanupComplete=false;
    // Prevent a watchdog from an earlier run from inspecting a newer panel.
    quint64 m_panelRunGeneration=0;
    // PanelProject owns the document independent of widgets/editor tabs.
    // Page tabs select groups of widgets; all pages share one script runtime.
    PanelProject m_panelProject;
    bool m_loadingPanelPage=false;
    struct PageSnapshot {
        QVector<PanelPage> pages;
        QString activeId;
    };
    struct PageHistoryEntry {
        PageSnapshot before,after;
    };
    QVector<PageHistoryEntry> m_pageHistory;
    int m_pageHistoryIndex=0;
    QPointer<ScriptEditor> m_panelEditor;
    QHash<ScriptEditor*,QString> m_embeddedEditors;
    bool m_panelDirty = false;
    WaveformPlot* m_scopePlot = nullptr;
    QLabel* m_scopeMetrics = nullptr;
    QLabel* m_cursorReadout = nullptr;
    QLabel* m_scopeInfo = nullptr;
    QPushButton* m_startScope = nullptr;
    QPushButton* m_singleScope = nullptr;
    QPushButton* m_stopScope = nullptr;
    QPushButton* m_detectButton = nullptr;
    QDoubleSpinBox* m_scopeRate = nullptr;
    QSpinBox* m_scopeSamples = nullptr;
    QDoubleSpinBox* m_scopeRange = nullptr;
    QCheckBox* m_showCh1 = nullptr;
    QCheckBox* m_showCh2 = nullptr;
    QCheckBox* m_softwareAlignment = nullptr;
    QComboBox* m_triggerMode = nullptr;
    QComboBox* m_triggerSource = nullptr;
    QComboBox* m_triggerSlope = nullptr;
    QDoubleSpinBox* m_triggerLevel = nullptr;
    QDoubleSpinBox* m_triggerHysteresis = nullptr;
    QDoubleSpinBox* m_triggerPosition = nullptr;
    QDoubleSpinBox* m_triggerAutoTimeout = nullptr;
    QComboBox* m_scaleMode = nullptr;
    QDoubleSpinBox* m_ch1Div = nullptr;
    QDoubleSpinBox* m_ch2Div = nullptr;
    QDoubleSpinBox* m_ch1Center = nullptr;
    QDoubleSpinBox* m_ch2Center = nullptr;
    bool m_scopeContinuous = false;
    bool m_pendingTriggerRestart = false;
    bool m_closing = false;
    bool m_skipWavegenConfirmationOnce = false;
    QCheckBox* m_testWavegen = nullptr;
    QPointer<ScopeAcquisitionWorker> m_acquisitionWorker;
    QPointer<QThread> m_acquisitionThread;
    ScopeFrame m_lastFrame;
    QComboBox* m_runtimeMode = nullptr;
    QSpinBox* m_deviceIndex = nullptr;
    QSpinBox* m_configIndex = nullptr;
    QPlainTextEdit* m_console = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QWidget* m_operatorBar = nullptr;
    QLabel* m_operatorTitle = nullptr;
    QLabel* m_operatorStatus = nullptr;
    QPushButton* m_operatorConsoleButton = nullptr;
    QPushButton* m_operatorWatchButton = nullptr;
    // Operator tools are QDockWidgets owned by MainWindow, never by the
    // script worker. A second QPlainTextEdit shares the Console document.
    QDockWidget* m_operatorConsoleDock = nullptr;
    QPlainTextEdit* m_operatorConsoleOutput = nullptr;
    // The normal Console remains the sole log writer. A filtered view uses
    // its own document and never changes or truncates the original history.
    QTextDocument* m_operatorFilteredDocument = nullptr;
    QComboBox* m_operatorLogScope = nullptr;
    QComboBox* m_operatorLogLevel = nullptr;
    QCheckBox* m_operatorLogWrap = nullptr;
    bool m_operatorConsoleRefreshQueued = false;
    QString m_operatorSessionMarker;
    int m_operatorDockPreferredWidth = 340;
    bool m_operatorPreferredConsoleFloating = false;
    bool m_operatorPreferredWatchFloating = false;
    Qt::DockWidgetArea m_operatorPreferredConsoleArea = Qt::RightDockWidgetArea;
    Qt::DockWidgetArea m_operatorPreferredWatchArea = Qt::RightDockWidgetArea;
    QByteArray m_operatorConsoleFloatGeometry;
    QByteArray m_operatorWatchFloatGeometry;
    QPushButton* m_operatorConsoleFloat = nullptr;
    QDockWidget* m_panelDataWatchDock = nullptr;
    QTreeWidget* m_panelDataWatchTree = nullptr;
    QLineEdit* m_panelDataWatchFilter = nullptr;
    QLabel* m_panelDataWatchStatus = nullptr;
    QPushButton* m_panelDataWatchFloat = nullptr;
    QCheckBox* m_panelDataWatchPause = nullptr;
    QHash<QString,QTreeWidgetItem*> m_panelDataWatchRows;
    QHash<QString,QVariant> m_panelDataWatchValues;
    QHash<QString,QVariant> m_panelDataWatchShown;
    QHash<QString,int> m_panelDataWatchChanges;
    QHash<QString,qint64> m_panelDataWatchHighlightUntil;
    QTimer* m_panelDataWatchHighlightTimer = nullptr;
    bool m_panelDataWatchRefreshQueued = false;
    bool m_operatorWatchOpen = false;
    bool m_operatorRestoringDocks = false;
    bool m_operatorConsolePlaced = false;
    bool m_operatorWatchPlaced = false;
    QPushButton* m_operatorFitButton = nullptr;
    QPushButton* m_operatorStopButton = nullptr;
    QAction* m_operatorViewAction = nullptr;
    QAction* m_operatorAutoAction = nullptr;
    bool m_operatorViewActive = false;
    bool m_operatorConsoleOpen = false;
    bool m_operatorHasError = false;
    bool m_operatorSavedConsoleVisible = false;
    bool m_operatorSavedMenuVisible = true;
    bool m_operatorSavedStatusVisible = true;
    bool m_operatorSavedFindVisible = false;
    bool m_operatorSavedProblemsVisible = false;
    bool m_operatorSavedWorkspaceTabsVisible = true;
    bool m_operatorSavedMaximized = false;
    QByteArray m_operatorSavedGeometry;
    QList<int> m_operatorSavedSplitterSizes;
    QWidget* m_operatorPreviousWorkspace = nullptr;
    QWidget* m_findBar = nullptr;
    QLineEdit* m_findInput = nullptr;
    QLineEdit* m_replaceInput = nullptr;
    QCheckBox* m_matchCase = nullptr;
    QAction* m_runAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_hintsAction = nullptr;
    QAction* m_darkAction = nullptr;
    QPointer<ScriptWorker> m_worker;
    QPointer<QThread> m_thread;
    QHash<ScriptEditor*, QString> m_paths;
    bool m_dark = true;
    int m_untitledCounter = 0;

    ScriptEditor* currentEditor() const;
    ScriptEditor* newTab(const QString& title = QString(), const QString& content = QString());
    void updateTabTitle(ScriptEditor* editor);
    void rebuildProblems();
    void updateStatus();
    bool confirmDiscard(ScriptEditor* editor);
    bool saveEditor(ScriptEditor* editor, bool saveAs);
    void openFile();
    ScriptEditor* openScriptPath(const QString& path);
    void navigateToProblem(const QString& source, int line, int column, bool panelOwned);
    void closeTab(int index);
    void runScript();
    void stopScript();
    void setRunning(bool running);
    void showFind(bool replace);
    bool findText(bool backwards);
    void replaceOnce();
    void replaceAll();
    void toggleTheme();
    void applyApplicationTheme();
    void buildScopeWorkspace();
    void startCapture(bool continuous);
    void triggerControlChanged();
    void scaleControlChanged();
    void stopCapture();
    void showFrame(const ScopeFrame& frame);
    void refreshRunControls();
    void saveScopeCsv();
    void buildPanelWorkspace();
    void makeExamplePanel();
    void loadBuiltInExample(const QString& resource);
    bool activatePanelProject(PanelProject loaded,bool fromResource,const QString& sourceName);
    void syncVisiblePage();
    bool preparePageWidgetsForRun();
    bool showPanelPage(const QString& id);
    void refreshPageTabs();
    QString operatorPanelDisplayName() const;
    void refreshOperatorTitle();
    void addPanelPage();
    void renamePanelPage();
    void deletePanelPage();
    void undoPanelPage();
    void redoPanelPage();
    PageSnapshot pageSnapshot() const;
    void recordPageHistory(const PageSnapshot& before);
    void applyPageSnapshot(const PageSnapshot& snapshot);
    void resetPageHistory();
    void showPanelScriptEditor();
    void showPanelLibraryEditor(const QString& name);
    void manageLibraries();
    void editTestSequence();
    void exportReadablePanelSources();
    void openPanelLifecycleFunction(const QString& name);
    void openPanelEventHandler(const QString& controlId,const QString& eventName,
                               const QString& handler,bool create);
    void closeEmbeddedEditors();
    bool setEmbeddedLibrary(const QString& name,const QString& source);
    void replacePanelScript(const QString& tabName, const QString& source);
    bool savePanelProjectToFile(bool saveAs);
    bool confirmPanelChanges(const QString& operation);
    void newPanelProject();
    void loadPanelProject();
    void savePanelProject();
    void runPanel();
    void stopPanel();
    void enterOperatorView();
    void leaveOperatorView();
    void toggleOperatorView();
    void setOperatorConsoleVisible(bool visible);
    void setOperatorWatchVisible(bool visible);
    void buildOperatorDocks();
    void refreshOperatorConsole();
    void queueOperatorConsoleRefresh();
    void restoreOperatorDock(QDockWidget* dock, bool console);
    void toggleOperatorDockFloating(QDockWidget* dock, bool console);
    void saveOperatorDiagnosticsSettings();
    void resetPanelDataWatch();
    void setPanelDataWatchValue(const QString& name,const QVariant& value);
    void flushPanelDataWatch();
    void filterPanelDataWatch();
    void setOperatorError(const QString& detail);
    void updateOperatorHeader(const QString& state);
    // Finalize a panel only after QThread::wait(0) confirms actual thread exit.
    void finalizePanelThread(QThread* thread);
};
