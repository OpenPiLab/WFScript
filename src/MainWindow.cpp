#include "panel/ScopeMetricsFormatter.h"
#include "widgets/WidgetInitialValues.h"
#include "MainWindow.h"
#include "ScriptEditor.h"
#include "ScriptWorker.h"
#include "DwfRuntime.h"
#include "device/DeviceWorkspace.h"
#include "ScopeAcquisitionWorker.h"
#include "WaveformPlot.h"
#include "ScopeMath.h"
#include "PanelDesigner.h"
#include "PanelRuntime.h"
#include "ScriptLibraryBridge.h"
#include "TestSequenceEditor.h"
#include "TestReportStorage.h"
#include "TestResultsBrowserDialog.h"
#include "ModuleExportCatalog.h"
#include "ApiExplorer.h"
#include "ApiDiagnostics.h"
#include "UiIcons.h"
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QListWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QMap>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaObject>
#include <QTextStream>
#include <QStringConverter>
#include <QGridLayout>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QKeySequence>
#include <QPalette>
#include <QColor>
#include <QWidget>
#include <functional>
#include <cmath>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace {
const char* kSample = R"JS(// WaveForms-style, independently implemented Qt6/C++ SDK script adapter.
// Default is Simulation. Select Physical Device on the Device tab for AD3 access.
print("Device:", Device.name, "Serial:", Device.SN);

Wavegen1.Channel1.Simple.Type.text = "Sine";
Wavegen1.Channel1.Simple.Frequency.value = 1000;
Wavegen1.Channel1.Simple.Amplitude.value = 1.0;
Wavegen1.Channel1.Simple.Offset.value = 0;
if (!Wavegen1.run()) throw new Error(Wavegen1.lastError);

Scope1.Time.Samples.value = 4096;
Scope1.Time.Rate.value = 1000000;
Scope1.Channel1.Range.value = 5;
if (!Scope1.single()) throw new Error(Scope1.lastError);
if (!Scope1.wait(5)) throw new Error(Scope1.lastError);
const t0 = Date.now();
const values = Scope1.Channel1.data;
let min = Infinity, max = -Infinity;
for (const v of values) { min = Math.min(min, v); max = Math.max(max, v); }
print("JavaScript data loop:", Date.now() - t0, "ms for", values.length, "samples");
const native = Scope1.Channel1.statistics();
print("Native stats: min", native.min.toFixed(4), "max", native.max.toFixed(4),
      "RMS", native.rms.toFixed(4), "V; count", native.count);
print("JavaScript min", min.toFixed(4), "max", max.toFixed(4));
Wavegen1.stop();
// Ctrl+Space after Scope1. shows documented members.
)JS";

QAction* addActionTo(QMenu* menu, const QString& text, const QKeySequence& shortcut,
                     QObject* receiver, const std::function<void()>& onTriggered) {
    QAction* action = menu->addAction(text);
    if (!shortcut.isEmpty()) action->setShortcut(shortcut);
    QObject::connect(action, &QAction::triggered, receiver, onTriggered);
    return action;
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("WFScript v0.1.0"));
    resize(1400, 850);

    auto* root = new QWidget(this);
    auto* vertical = new QVBoxLayout(root);
    vertical->setContentsMargins(0, 0, 0, 0);
    vertical->setSpacing(0);

    m_findBar = new QWidget(root);
    auto* findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(8, 5, 8, 5);
    m_findInput = new QLineEdit(m_findBar);
    m_findInput->setPlaceholderText(QStringLiteral("Find text"));
    m_findInput->setMinimumWidth(120);
    m_replaceInput = new QLineEdit(m_findBar);
    m_replaceInput->setPlaceholderText(QStringLiteral("Replace with"));
    m_replaceInput->setMinimumWidth(120);
    m_matchCase = new QCheckBox(QStringLiteral("Match case"), m_findBar);
    auto* prev = new QPushButton(QStringLiteral("Previous"), m_findBar);
    auto* next = new QPushButton(QStringLiteral("Next"), m_findBar);
    auto* replace = new QPushButton(QStringLiteral("Replace"), m_findBar);
    auto* all = new QPushButton(QStringLiteral("Replace All"), m_findBar);
    auto* hide = new QPushButton(QStringLiteral("×"), m_findBar);
    hide->setToolTip(QStringLiteral("Close search bar"));
    hide->setFixedWidth(28);

    findLayout->addWidget(m_findInput, 3);
    findLayout->addWidget(m_replaceInput, 3);
    findLayout->addWidget(m_matchCase);
    findLayout->addWidget(prev);
    findLayout->addWidget(next);
    findLayout->addWidget(replace);
    findLayout->addWidget(all);
    findLayout->addWidget(hide);
    m_findBar->hide();
    vertical->addWidget(m_findBar);

    auto* split = new QSplitter(Qt::Vertical, root);
    m_workspaces = new QTabWidget(split);
    m_workspaces->setObjectName(QStringLiteral("workspaceTabs"));
    m_deviceWorkspace = new DeviceWorkspace(m_workspaces);
    m_workspaces->addTab(m_deviceWorkspace, QStringLiteral("Device"));
    // The API Explorer belongs to the Script Editor page, not the QMainWindow.
    // Switching workspaces never moves, destroys, or re-docks the explorer.
    m_scriptWorkspace = new QSplitter(Qt::Horizontal, m_workspaces);
    m_scriptWorkspace->setObjectName(QStringLiteral("scriptWorkspaceSplitter"));
    m_apiPane = new QWidget(m_scriptWorkspace);
    m_apiPane->setObjectName(QStringLiteral("scriptApiExplorerPane"));
    auto* apiLayout = new QVBoxLayout(m_apiPane);
    apiLayout->setContentsMargins(3, 3, 3, 3);
    apiLayout->setSpacing(3);
    auto* apiHeader = new QHBoxLayout;
    apiHeader->addWidget(new QLabel(QStringLiteral("Instrument & Script API Explorer"),m_apiPane));
    auto* apiClose = new QPushButton(QStringLiteral("×"),m_apiPane);
    apiClose->setObjectName(QStringLiteral("closeScriptApiExplorer"));
    apiClose->setToolTip(QStringLiteral("Hide API Explorer (Ctrl+Shift+E to reopen)"));
    apiClose->setFixedWidth(25);
    apiHeader->addWidget(apiClose);
    apiLayout->addLayout(apiHeader);
    m_apiExplorer = new ApiExplorer(m_apiPane);
    m_apiExplorer->setObjectName(QStringLiteral("instrumentApiExplorer"));
    apiLayout->addWidget(m_apiExplorer,1);
    m_apiPane->setMinimumWidth(185);
    m_apiPane->setMaximumWidth(410);
    // Script commands belong to the Script Editor workspace, not the window-
    // level hardware toolbar. The editor view and its toolbar are one stable
    // splitter child, so switching to Panel/Scope doesn't re-parent actions.
    auto* scriptPage = new QWidget(m_scriptWorkspace);
    scriptPage->setObjectName(QStringLiteral("scriptEditorPage"));
    auto* scriptLayout = new QVBoxLayout(scriptPage);
    scriptLayout->setContentsMargins(0, 0, 0, 0);
    scriptLayout->setSpacing(0);
    auto* scriptActions = new QToolBar(QStringLiteral("Script Editor Actions"), scriptPage);
    scriptActions->setObjectName(QStringLiteral("scriptEditorToolbar"));
    scriptActions->setMovable(false);
    scriptActions->setFloatable(false);
    scriptActions->setIconSize(QSize(16, 16));
    scriptActions->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    scriptActions->setStyleSheet(QStringLiteral(
        "QToolBar { border: 0; border-bottom: 1px solid palette(mid); "
        "padding: 3px 5px; spacing: 4px; } "
        "QToolButton { padding: 4px 7px; }"));
    scriptLayout->addWidget(scriptActions);
    m_tabs = new QTabWidget(scriptPage);
    m_tabs->setObjectName(QStringLiteral("scriptEditorTabs"));
    scriptLayout->addWidget(m_tabs, 1);
    m_scriptWorkspace->addWidget(m_apiPane);
    m_scriptWorkspace->addWidget(scriptPage);
    m_scriptWorkspace->setChildrenCollapsible(false);
    m_scriptWorkspace->setStretchFactor(0,0);
    m_scriptWorkspace->setStretchFactor(1,1);
    m_scriptWorkspace->setSizes({255, 1100});
    m_workspaces->addTab(m_scriptWorkspace, QStringLiteral("Script Editor"));
    connect(apiClose,&QPushButton::clicked,this,[this]{
        if (m_apiAction) m_apiAction->setChecked(false);
        else m_apiPane->hide();
    });
    buildScopeWorkspace();
    buildPanelWorkspace();
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_console = new QPlainTextEdit(split);
    m_console->setObjectName(QStringLiteral("scriptOutputConsole"));
    m_console->setReadOnly(true);
    m_console->setPlaceholderText(QStringLiteral("Script output appears here"));
    m_console->document()->setMaximumBlockCount(5000);
    split->addWidget(m_workspaces);
    split->addWidget(m_console);
    split->setStretchFactor(0, 8);
    split->setStretchFactor(1, 1);
    split->setSizes({740, 90});
    vertical->addWidget(split);
    setCentralWidget(root);

    connect(m_apiExplorer,&ApiExplorer::insertRequested,this,[this](const QString& path){
        if(auto* editor=currentEditor()) {
            m_workspaces->setCurrentWidget(m_scriptWorkspace);
            editor->insertApiPath(path);
        }
    });

    m_problemsDock=new QDockWidget(QStringLiteral("Problems"),this);
    m_problemsDock->setObjectName(QStringLiteral("scriptProblemsDock"));
    m_problemsDock->setAllowedAreas(Qt::BottomDockWidgetArea|Qt::TopDockWidgetArea);
    m_problems=new QTreeWidget(m_problemsDock);
    m_problems->setObjectName(QStringLiteral("scriptProblemsTree"));
    m_problems->setHeaderLabels({QStringLiteral("Severity"),QStringLiteral("Source"),
                                  QStringLiteral("Line"),QStringLiteral("Message")});
    m_problems->setRootIsDecorated(false);
    m_problems->setAlternatingRowColors(true);
    m_problems->setColumnWidth(0,95);
    m_problems->setColumnWidth(1,210);
    m_problems->setColumnWidth(2,65);
    m_problemsDock->setWidget(m_problems);
    addDockWidget(Qt::BottomDockWidgetArea,m_problemsDock);
    m_problemsDock->hide();
    connect(m_problems,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){
        if(!item) return;
        navigateToProblem(item->data(0,Qt::UserRole).toString(),
                          item->data(2,Qt::UserRole).toInt(),
                          item->data(2,Qt::UserRole+1).toInt(),
                          item->data(0,Qt::UserRole+1).toBool());
    });

    connect(prev, &QPushButton::clicked, this, [this] { findText(true); });
    connect(next, &QPushButton::clicked, this, [this] { findText(false); });
    connect(m_findInput, &QLineEdit::returnPressed, this, [this] { findText(false); });
    connect(replace, &QPushButton::clicked, this, &MainWindow::replaceOnce);
    connect(all, &QPushButton::clicked, this, &MainWindow::replaceAll);
    connect(hide, &QPushButton::clicked, m_findBar, &QWidget::hide);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        updateStatus();
        rebuildProblems();
    });

    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    auto* newAction = addActionTo(file, QStringLiteral("&New"), QKeySequence::New,
                                  this, [this] { newTab(); });
    auto* openAction = addActionTo(file, QStringLiteral("&Open..."), QKeySequence::Open,
                                   this, [this] { openFile(); });
    auto* saveAction = addActionTo(file, QStringLiteral("&Save"), QKeySequence::Save,
                                   this, [this] {
        if (m_workspaces->currentWidget() == m_panelDesigner) savePanelProjectToFile(false);
        else saveEditor(currentEditor(), false);
    });
    addActionTo(file, QStringLiteral("Save &As..."), QKeySequence::SaveAs,
                this, [this] {
        if (m_workspaces->currentWidget() == m_panelDesigner) savePanelProjectToFile(true);
        else saveEditor(currentEditor(), true);
    });
    file->addSeparator();
    addActionTo(file, QStringLiteral("Export Readable Panel Sources..."), {}, this,
                [this] { exportReadablePanelSources(); });
    file->addSeparator();
    addActionTo(file, QStringLiteral("Close &Tab"), QKeySequence(QStringLiteral("Ctrl+W")),
                this, [this] { closeTab(m_tabs->currentIndex()); });
    addActionTo(file, QStringLiteral("E&xit"), QKeySequence::Quit,
                this, [this] { close(); });

    QMenu* edit = menuBar()->addMenu(QStringLiteral("&Edit"));
    addActionTo(edit, QStringLiteral("&Find..."), QKeySequence::Find,
                this, [this] { showFind(false); });
    addActionTo(edit, QStringLiteral("Find and &Replace..."),
                QKeySequence(QStringLiteral("Ctrl+H")), this,
                [this] { showFind(true); });
    addActionTo(edit, QStringLiteral("Find &Next"), QKeySequence::FindNext,
                this, [this] { findText(false); });
    addActionTo(edit, QStringLiteral("Find &Previous"), QKeySequence::FindPrevious,
                this, [this] { findText(true); });

    addActionTo(edit, QStringLiteral("API Help at Cursor (F1)"),QKeySequence(QStringLiteral("F1")),
                this,[this] { if(auto* editor=currentEditor()) editor->showApiHelpAtCursor(); });
    QMenu* script = menuBar()->addMenu(QStringLiteral("&Script"));
    m_runAction = addActionTo(script, QStringLiteral("&Run"), QKeySequence(QStringLiteral("F5")),
                              this, [this] { runScript(); });
    m_stopAction = addActionTo(script, QStringLiteral("S&top"), QKeySequence(QStringLiteral("Shift+F5")),
                               this, [this] { stopScript(); });
    script->addSeparator();
    addActionTo(script, QStringLiteral("Manage Script Libraries..."), {}, this,
                [this]{ manageLibraries(); });
    script->addSeparator();
    addActionTo(script, QStringLiteral("Visual Test Sequence Editor..."), {}, this,
                [this] { editTestSequence(); });
    addActionTo(script, QStringLiteral("Test Results Browser..."), {}, this,[this] {
        const QString folder=TestReportStorage::directory(m_panelProject.reportStorage);
        if(m_resultsBrowser) {
            m_resultsBrowser->setResultsDirectory(folder);
            m_resultsBrowser->show();
            m_resultsBrowser->raise();
            m_resultsBrowser->activateWindow();
            return;
        }
        auto* browser=new TestResultsBrowserDialog(folder,this);
        browser->setAttribute(Qt::WA_DeleteOnClose);
        m_resultsBrowser=browser;
        browser->show();
    });
    addActionTo(script, QStringLiteral("Open Test Results Folder"), {}, this,[this] {
        const QString folder=TestReportStorage::directory(m_panelProject.reportStorage);
        if(folder.isEmpty()){
            QMessageBox::warning(this,"Test Results","No Documents folder found. Configure an archive folder in the Visual Test Sequence Editor.");return;
        }
        if(!QDir(folder).exists()){
            QMessageBox::information(this,"Test Results",QStringLiteral(
                "The report folder does not exist yet. Enable automatic saving and complete a test to create it:\n%1").arg(folder));return;
        }
        if(!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
            QMessageBox::warning(this,"Test Results",QStringLiteral("Could not open folder: %1").arg(folder));
    });
    script->addSeparator();
    m_hintsAction = addActionTo(script, QStringLiteral("Autocomplete &Hints"), {},
                                this, [this] {
                                    for (auto* editor : m_paths.keys())
                                        editor->setHintsEnabled(m_hintsAction->isChecked());
                                });
    m_hintsAction->setCheckable(true);
    m_hintsAction->setChecked(true);
    setRunning(false);

    QMenu* view = menuBar()->addMenu(QStringLiteral("&View"));
    m_darkAction = addActionTo(view, QStringLiteral("&Dark Theme"), {},
                               this, [this] { toggleTheme(); });
    m_darkAction->setCheckable(true);
    m_darkAction->setChecked(true);
    m_apiAction = view->addAction(QStringLiteral("Instrument & Script API Explorer"));
    m_apiAction->setObjectName(QStringLiteral("toggleScriptApiExplorer"));
    m_apiAction->setToolTip(QStringLiteral("Instrument API Explorer — includes the TestRunner, modules, plots and Panel API"));
    m_apiAction->setCheckable(true);
    m_apiAction->setChecked(true);
    m_apiAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+E")));
    connect(m_apiAction,&QAction::toggled,this,[this](bool visible){
        m_apiPane->setVisible(visible);
        if (visible) m_workspaces->setCurrentWidget(m_scriptWorkspace);
    });
    m_problemsDock->toggleViewAction()->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    view->addAction(m_problemsDock->toggleViewAction());
    auto* outputAction=view->addAction(QStringLiteral("Script Output"));
    outputAction->setObjectName(QStringLiteral("toggleScriptOutput"));
    outputAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    outputAction->setCheckable(true);
    outputAction->setChecked(true);
    connect(outputAction,&QAction::toggled,m_console,&QWidget::setVisible);
    addActionTo(view, QStringLiteral("Clear &Output"), {}, this,
                [this] { m_console->clear(); });

    // The original QAction objects remain in File/Script menus and retain
    // Ctrl+N, Ctrl+O, Ctrl+S, F5 and Shift+F5 shortcuts. Only their visible
    // toolbar representation moves into the Script Editor tab.
    newAction->setIcon(UiIcons::icon("new"));
    openAction->setIcon(UiIcons::icon("open"));
    saveAction->setIcon(UiIcons::icon("save"));
    m_runAction->setIcon(UiIcons::icon("run"));
    m_stopAction->setIcon(UiIcons::icon("stop"));
    newAction->setToolTip(QStringLiteral("New standalone JavaScript tab (Ctrl+N)"));
    openAction->setToolTip(QStringLiteral("Open JavaScript file (Ctrl+O)"));
    saveAction->setToolTip(QStringLiteral("Save active script or panel project (Ctrl+S)"));
    m_runAction->setToolTip(QStringLiteral("Run standalone JavaScript (F5). For panels use Run Panel."));
    m_stopAction->setToolTip(QStringLiteral("Stop standalone JavaScript (Shift+F5)"));
    scriptActions->addAction(newAction);
    scriptActions->addAction(openAction);
    scriptActions->addAction(saveAction);
    scriptActions->addSeparator();
    scriptActions->addAction(m_runAction);
    scriptActions->addAction(m_stopAction);

    // Keep the established instrument selection pointers for all existing
    // script/Panel/Scope workflows; the controls now live in the Device tab.
    // No global toolbar consumes canvas height in other workspaces.
    m_runtimeMode = m_deviceWorkspace->runtimeMode();
    m_deviceIndex = m_deviceWorkspace->deviceIndex();
    m_configIndex = m_deviceWorkspace->configIndex();
    m_detectButton = m_deviceWorkspace->detectButton();
    connect(m_runtimeMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { refreshRunControls(); });


    qRegisterMetaType<ScopeFrame>("ScopeFrame");
    statusBar()->showMessage(QStringLiteral("Ready · Ctrl+Space: completion · F5: run"));
    newTab(QString(), QString::fromUtf8(kSample));
    makeExamplePanel();
    m_tabs->setCurrentIndex(0);
    m_workspaces->setCurrentWidget(m_deviceWorkspace);
    // Apply the console theme as well as the editor theme at startup.
    m_dark = false;
    toggleTheme();
    refreshRunControls();
    // Discover hardware after the main window has fully initialized.
    QTimer::singleShot(0,m_deviceWorkspace,[this]{ if(m_deviceWorkspace) m_deviceWorkspace->refreshDevices(); });
}

MainWindow::~MainWindow() {
    m_closing = true;
    m_pendingTriggerRestart = false;
    stopCapture();
    if (m_panelRuntime) stopPanel();
    if (m_panelThread) { m_panelThread->quit(); m_panelThread->wait(5000); }
    if (m_acquisitionThread) { m_acquisitionThread->quit(); m_acquisitionThread->wait(5000); }
    stopScript();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
}

ScriptEditor* MainWindow::currentEditor() const {
    return qobject_cast<ScriptEditor*>(m_tabs->currentWidget());
}

ScriptEditor* MainWindow::newTab(const QString& title, const QString& content) {
    auto* editor = new ScriptEditor(m_tabs);
    editor->setDarkTheme(m_dark);
    editor->setHintsEnabled(m_hintsAction ? m_hintsAction->isChecked() : true);
    editor->setModuleSourceResolver([this,editor](const QString& name) -> QString {
        if(!ScriptLibraryBridge::validLibraryName(name) ||
           !name.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive))return {};
        const bool panelOwned=(editor==m_panelEditor || m_embeddedEditors.contains(editor));
        if(panelOwned) {
            for(const auto& entry:m_panelProject.libraries) {
                const auto obj=entry.toObject();
                if(obj.value(QStringLiteral("name")).toString()!=name)continue;
                for(auto it=m_embeddedEditors.cbegin();it!=m_embeddedEditors.cend();++it)
                    if(it.value()==name)return it.key()->toPlainText();
                return obj.value(QStringLiteral("source")).toString();
            }
        }
        const QString project=panelOwned?m_panelProject.path:m_paths.value(editor);
        const QString base=QFileInfo(project).absolutePath();
        QStringList roots;
        if(!project.isEmpty())roots << QDir(base).filePath(QStringLiteral("libraries")) << base;
        roots << ScriptLibraryBridge::userLibraryDirectory()
              << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("libraries"));
        for(const auto& root:roots) {
            const QString canonicalRoot=QFileInfo(root).canonicalFilePath();
            const QFileInfo candidate(QDir(root).filePath(name));
            const QString canonical=candidate.canonicalFilePath();
            if(canonicalRoot.isEmpty() || !candidate.isFile() || candidate.size()>1024*1024 ||
               !canonical.startsWith(canonicalRoot+QLatin1Char('/'),
#ifdef Q_OS_WIN
                   Qt::CaseInsensitive
#else
                   Qt::CaseSensitive
#endif
               ))continue;
            QFile file(canonical);
            if(file.open(QIODevice::ReadOnly))return QString::fromUtf8(file.readAll());
        }
        return {};
    });
    editor->setPlainText(content);
    editor->document()->setModified(false);
    const QString id = title.isEmpty() ? QStringLiteral("Untitled %1").arg(++m_untitledCounter) : title;
    // Untitled tab names are kept in a QWidget property, independent of path.
    editor->setProperty("displayName", id);
    m_paths.insert(editor, QString());
    m_tabs->addTab(editor, id);
    m_tabs->setCurrentWidget(editor);
    connect(editor->document(), &QTextDocument::modificationChanged, this,
            [this, editor] { updateTabTitle(editor); });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this,
            [this, editor] { if (currentEditor() == editor) updateStatus(); });
    connect(editor, &ScriptEditor::diagnosticsChanged, this,
            [this, editor] { if (currentEditor()==editor) rebuildProblems(); });
    connect(editor->document(), &QTextDocument::contentsChanged, this,
            [this, editor] {
                if(m_runtimeErrors.remove(editor)>0) {
                    editor->highlightErrorLine(-1);
                    if(currentEditor()==editor) rebuildProblems();
                }
            });
    return editor;
}

void MainWindow::updateTabTitle(ScriptEditor* editor) {
    const int index = m_tabs->indexOf(editor);
    if (index < 0) return;
    const QString path = m_paths.value(editor);
    QString name = path.isEmpty() ? editor->property("displayName").toString()
                                  : QFileInfo(path).fileName();
    if (m_embeddedEditors.contains(editor)) {
        m_tabs->setTabText(index, QStringLiteral("Library: ") + m_embeddedEditors.value(editor)
                 + (m_panelDirty ? QStringLiteral("*") : QString()));
        m_tabs->setTabToolTip(index,QStringLiteral("Embedded panel library; Ctrl+S saves the complete .wfpanel."));
        return;
    }
    if (editor == m_panelEditor) {
        // The panel file, not an independent .js file, is this tab's owner.
        if (m_panelDirty) name += QLatin1Char('*');
        m_tabs->setTabText(index, QStringLiteral("Panel: ")+name);
        m_tabs->setTabToolTip(index,
            QStringLiteral("Embedded panel JavaScript — close this tab safely; "
                           "use Panel Designer > Edit JavaScript to reopen. "
                           "Ctrl+S saves the .wfpanel project."));
        return;
    }
    if (editor->document()->isModified()) name += QLatin1Char('*');
    m_tabs->setTabText(index, name);
    m_tabs->setTabToolTip(index, path.isEmpty() ? QStringLiteral("Unsaved script") : path);
}

void MainWindow::rebuildProblems() {
    if (!m_problems) return;
    m_problems->clear();
    auto add=[this](const QString& severity,const RuntimeProblem& issue,bool panelOwned){
        QString location=issue.line>0 ? QString::number(issue.line) : QStringLiteral("?");
        if(issue.column>0)location+=QStringLiteral(":%1").arg(issue.column);
        auto* row=new QTreeWidgetItem(m_problems,{severity,issue.source,location,issue.message});
        row->setData(0,Qt::UserRole,issue.source);
        row->setData(0,Qt::UserRole+1,panelOwned);
        row->setData(2,Qt::UserRole,issue.line);
        row->setData(2,Qt::UserRole+1,issue.column);
        row->setToolTip(3,issue.context+(issue.stack.isEmpty() ? QString()
            : QStringLiteral("\n")+issue.stack));
    };
    for(const auto& issue:m_panelProblems)add(QStringLiteral("Error"),issue,true);
    if (auto* editor=currentEditor()) {
        const auto runtime=m_runtimeErrors.constFind(editor);
        if(runtime!=m_runtimeErrors.cend()) add(QStringLiteral("Error"),runtime.value(),false);
        for(const auto& issue:editor->diagnostics()) {
            RuntimeProblem warning;
            warning.source=m_paths.value(editor);
            if(warning.source.isEmpty())warning.source=editor->property("displayName").toString();
            warning.line=issue.line;warning.column=issue.column;
            warning.message=issue.message;
            warning.context=QStringLiteral("Static API check");
            add(QStringLiteral("Warning"),warning,false);
        }
    }
    m_problemsDock->setWindowTitle(m_problems->topLevelItemCount()==0
        ? QStringLiteral("Problems — none")
        : QStringLiteral("Problems — %1").arg(m_problems->topLevelItemCount()));
}

void MainWindow::updateStatus() {
    if (ScriptEditor* editor = currentEditor()) {
        const QTextCursor cursor = editor->textCursor();
        statusBar()->showMessage(QStringLiteral("Line %1, Column %2   |   %3 lines   |   Ctrl+Space: completion")
            .arg(cursor.blockNumber()+1)
            .arg(cursor.positionInBlock()+1)
            .arg(editor->document()->blockCount()));
    }
}

bool MainWindow::saveEditor(ScriptEditor* editor, bool saveAs) {
    if (!editor) return false;
    if (editor == m_panelEditor || m_embeddedEditors.contains(editor))
        return savePanelProjectToFile(saveAs); // Never save an embedded panel script as an unrelated .js.
    QString fileName = saveAs ? QString() : m_paths.value(editor);
    if (fileName.isEmpty()) {
        fileName = QFileDialog::getSaveFileName(this, QStringLiteral("Save JavaScript"),
                      m_paths.value(editor), QStringLiteral("JavaScript (*.js *.mjs);;All files (*)"));
        if (fileName.isEmpty()) return false;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), file.errorString());
        return false;
    }
    const QByteArray content = editor->toPlainText().toUtf8();
    if (file.write(content) != content.size()) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), file.errorString());
        return false;
    }
    m_paths[editor] = fileName;
    editor->document()->setModified(false);
    updateTabTitle(editor);
    return true;
}

bool MainWindow::confirmDiscard(ScriptEditor* editor) {
    if (!editor || !editor->document()->isModified()) return true;
    const auto choice = QMessageBox::question(
        this, QStringLiteral("Unsaved changes"),
        QStringLiteral("Save changes to %1?").arg(
            m_paths.value(editor).isEmpty() ? editor->property("displayName").toString()
                                            : QFileInfo(m_paths.value(editor)).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) return saveEditor(editor, false);
    return true;
}

ScriptEditor* MainWindow::openScriptPath(const QString& path) {
    const QString normalized=QFileInfo(path).canonicalFilePath();
    if(normalized.isEmpty()) return nullptr;
    for(auto it=m_paths.cbegin();it!=m_paths.cend();++it) {
        if(!it.value().isEmpty() && QFileInfo(it.value()).canonicalFilePath()==normalized) {
            m_tabs->setCurrentWidget(it.key());
            m_workspaces->setCurrentWidget(m_scriptWorkspace);
            return it.key();
        }
    }
    QFile file(normalized);
    if(!file.open(QIODevice::ReadOnly)) return nullptr;
    ScriptEditor* editor=newTab(QFileInfo(normalized).fileName(),QString::fromUtf8(file.readAll()));
    m_paths[editor]=normalized;
    updateTabTitle(editor);
    m_workspaces->setCurrentWidget(m_scriptWorkspace);
    return editor;
}

void MainWindow::openFile() {
    const QString name=QFileDialog::getOpenFileName(this,QStringLiteral("Open JavaScript"),
        QString(),QStringLiteral("JavaScript (*.js *.mjs);;All files (*)"));
    if(name.isEmpty())return;
    if(!openScriptPath(name))QMessageBox::warning(this,QStringLiteral("Open failed"),
                                                 QStringLiteral("Cannot read %1").arg(name));
}

void MainWindow::navigateToProblem(const QString& source,int line,int column,bool panelOwned) {
    ScriptEditor* editor=nullptr;
    if(panelOwned) {
        if(source==m_panelProject.scriptName) {
            showPanelScriptEditor();
            editor=m_panelEditor.data();
        } else {
            for(const auto& v:m_panelProject.libraries) {
                if(v.toObject().value(QStringLiteral("name")).toString()==source) {
                    showPanelLibraryEditor(source);
                    for(auto it=m_embeddedEditors.cbegin();it!=m_embeddedEditors.cend();++it)
                        if(it.value()==source) { editor=it.key();break; }
                    break;
                }
            }
        }
    }
    if(!editor && QFileInfo(source).isFile())editor=openScriptPath(source);
    if(!editor && !panelOwned)editor=currentEditor();
    if(!editor) {
        statusBar()->showMessage(QStringLiteral("Source unavailable: %1").arg(source),7000);
        return;
    }
    m_workspaces->setCurrentWidget(m_scriptWorkspace);
    m_tabs->setCurrentWidget(editor);
    if(line>0) {
        editor->goToLine(line);
        if(column>1) {
            QTextCursor cursor=editor->textCursor();
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::Right,QTextCursor::MoveAnchor,
                                qMin(column-1,int(cursor.block().text().size())));
            editor->setTextCursor(cursor);
        }
        editor->highlightErrorLine(line);
    }
    editor->setFocus();
}

void MainWindow::closeTab(int index) {
    if (index < 0) return;
    auto* editor = qobject_cast<ScriptEditor*>(m_tabs->widget(index));
    if (!editor) return;
    if (m_embeddedEditors.contains(editor)) {
        setEmbeddedLibrary(m_embeddedEditors.value(editor),editor->toPlainText());
        m_embeddedEditors.remove(editor);
        m_paths.remove(editor); m_runtimeErrors.remove(editor);
        m_tabs->removeTab(index);editor->deleteLater();
        if (!m_tabs->count()) newTab();
        return;
    }
    if (editor == m_panelEditor) {
        // Closing a panel script is *not* discarding it: the project's copy is
        // updated on every edit and remains available for Edit JavaScript,
        // Run Panel, and Save/Export even with no editor tab open.
        m_panelProject.script = editor->toPlainText();
        m_panelEditor = nullptr;
        m_paths.remove(editor);
        m_runtimeErrors.remove(editor);
        m_tabs->removeTab(index);
        editor->deleteLater();
        if (m_tabs->count() == 0) newTab();
        statusBar()->showMessage(QStringLiteral("Panel JavaScript retained in project. Use Edit JavaScript to reopen."), 5000);
        return;
    }
    if (!confirmDiscard(editor)) return;
    m_paths.remove(editor);
    m_runtimeErrors.remove(editor);
    m_tabs->removeTab(index);
    editor->deleteLater();
    if (m_tabs->count() == 0) newTab();
}

void MainWindow::setRunning(bool running) {
    if (m_runAction) m_runAction->setEnabled(!running);
    if (m_stopAction) m_stopAction->setEnabled(running);
}

void MainWindow::runScript() {
    if (m_thread || m_acquisitionThread || m_panelThread || !currentEditor()) return;
    ScriptEditor* editor = currentEditor();
    if (editor == m_panelEditor) {
        QMessageBox::information(this,"Panel JavaScript",
            "This script belongs to the Panel project and uses its ui object and embedded libraries. "
            "Use Run Panel in the Panel Designer instead of Run Script.");
        return;
    }
    if (m_embeddedEditors.contains(editor)) {
        QMessageBox::information(this,"Embedded panel library",
            "Library editors are part of the panel project. Use Run Panel to execute the panel script that loads them.");
        return;
    }
    if (m_paths.value(editor).endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive)
        && editor->document()->isModified() && !saveEditor(editor,false)) return;
    editor->highlightErrorLine(-1);
    m_runtimeErrors.remove(editor);
    rebuildProblems();
    m_console->clear();
    m_console->appendPlainText(QStringLiteral("Running script..."));
    const bool hardware = m_runtimeMode->currentIndex() == 1;
    const int deviceIndex = m_deviceIndex->value();
    const int configIndex = m_configIndex->value()-1;
    if (hardware) m_console->appendPlainText(QStringLiteral("Hardware mode selected; device output may change. Use only on a safely connected circuit."));
    const QString source = editor->toPlainText();
    QString fileName = m_paths.value(editor);
    if (fileName.isEmpty()) fileName = editor->property("displayName").toString()+QStringLiteral(".js");

    auto* thread = new QThread(this);
    auto* worker = new ScriptWorker();
    m_thread = thread;
    m_worker = worker;
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker,
            [worker, source, fileName, hardware, deviceIndex, configIndex] { worker->execute(source, fileName, hardware, deviceIndex, configIndex); });
    connect(worker, &ScriptWorker::outputReady, this,
            [this](const QString& message) { m_console->appendPlainText(message); });
    connect(worker,&ScriptWorker::consoleClearRequested,this,[this]{m_console->clear();});
    const QPointer<ScriptEditor> trackedEditor(editor);
    connect(worker, &ScriptWorker::scopeFrameReady, this, &MainWindow::showFrame);
    connect(worker, &ScriptWorker::scriptDiagnostic, this,
            [this,trackedEditor](const QString& source,int line,int column,
                                 const QString& message,const QString& stack) {
                if(!trackedEditor)return;
                RuntimeProblem issue;
                issue.source=source;issue.line=line;issue.column=column;
                issue.context=QStringLiteral("Standalone script");
                issue.message=message;issue.stack=stack;
                m_runtimeErrors.insert(trackedEditor.data(),issue);
                if(currentEditor()==trackedEditor) trackedEditor->highlightErrorLine(line);
                rebuildProblems();
                m_problemsDock->show();
            });
    connect(worker, &ScriptWorker::finished, this,
            [this](bool success, const QString& message) {
                m_console->appendPlainText(message);
                statusBar()->showMessage(message, 5000);
                setRunning(false);
                m_runtimeMode->setEnabled(true);
                m_deviceIndex->setEnabled(m_runtimeMode->currentIndex()==1);
                m_configIndex->setEnabled(m_runtimeMode->currentIndex()==1);
                if (!success) m_console->setFocus();
                refreshRunControls();
            });
    connect(worker, &ScriptWorker::finished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this,
            [this, thread] {
                if (m_thread == thread) { m_thread = nullptr; m_worker = nullptr; }
                thread->deleteLater();
                refreshRunControls();
            });
    setRunning(true);
    m_runtimeMode->setEnabled(false);
    m_deviceIndex->setEnabled(false);
    m_configIndex->setEnabled(false);
    thread->start();
    refreshRunControls();
}

void MainWindow::stopScript() {
    if (m_worker) m_worker->requestStop();
}

void MainWindow::showFind(bool replace) {
    m_findBar->show();
    m_replaceInput->setVisible(replace);
    // Hide replace buttons if just searching; they are indices 5/6 in layout.
    if (auto* layout = m_findBar->layout()) {
        for (int i : {5, 6}) {
            if (QWidget* w = layout->itemAt(i)->widget()) w->setVisible(replace);
        }
    }
    if (ScriptEditor* editor = currentEditor()) {
        if (editor->textCursor().hasSelection())
            m_findInput->setText(editor->textCursor().selectedText());
    }
    m_findInput->setFocus();
    m_findInput->selectAll();
}

bool MainWindow::findText(bool backwards) {
    ScriptEditor* editor = currentEditor();
    if (!editor || m_findInput->text().isEmpty()) return false;
    QTextDocument::FindFlags flags;
    if (m_matchCase->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    if (backwards) flags |= QTextDocument::FindBackward;
    if (editor->find(m_findInput->text(), flags)) return true;
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
    editor->setTextCursor(cursor);
    if (editor->find(m_findInput->text(), flags)) return true;
    statusBar()->showMessage(QStringLiteral("No matching text."), 3000);
    return false;
}

void MainWindow::replaceOnce() {
    ScriptEditor* editor = currentEditor();
    if (!editor || m_findInput->text().isEmpty()) return;
    QTextCursor cursor = editor->textCursor();
    bool matches = cursor.hasSelection() && cursor.selectedText().compare(
        m_findInput->text(), m_matchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0;
    if (!matches && !findText(false)) return;
    cursor = editor->textCursor();
    cursor.insertText(m_replaceInput->text());
    editor->setTextCursor(cursor);
    findText(false);
}

void MainWindow::replaceAll() {
    ScriptEditor* editor = currentEditor();
    const QString needle = m_findInput->text();
    if (!editor || needle.isEmpty()) return;
    QTextDocument::FindFlags flags;
    if (m_matchCase->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    editor->setTextCursor(cursor);
    int count = 0;
    while (editor->find(needle, flags)) {
        QTextCursor replacement = editor->textCursor();
        replacement.insertText(m_replaceInput->text());
        editor->setTextCursor(replacement);
        ++count;
    }
    cursor = editor->textCursor();
    cursor.endEditBlock();
    statusBar()->showMessage(QStringLiteral("Replaced %1 occurrence(s).").arg(count), 5000);
}

void MainWindow::toggleTheme() {
    m_dark = !m_dark;
    if (m_darkAction) m_darkAction->setChecked(m_dark);
    for (auto* editor : m_paths.keys()) editor->setDarkTheme(m_dark);
    QPalette p = m_console->palette();
    p.setColor(QPalette::Base, QColor(m_dark ? "#121b26" : "#ffffff"));
    p.setColor(QPalette::Text, QColor(m_dark ? "#c7d7ea" : "#20252d"));
    m_console->setPalette(p);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    m_closing = true;
    m_pendingTriggerRestart = false;
    stopCapture();
    if (m_panelRuntime) stopPanel();
    if (m_panelThread && !m_panelThread->wait(5000)) {
        QMessageBox::warning(this,"Panel runtime busy","Panel JavaScript has not stopped yet; close cancelled.");
        m_closing=false;event->ignore();return;
    }
    if (m_acquisitionThread && !m_acquisitionThread->wait(5000)) {
        QMessageBox::warning(this,QStringLiteral("Acquisition still running"),
            QStringLiteral("Oscilloscope did not stop; close cancelled to avoid destroying an active thread."));
        m_closing = false;
        event->ignore();return;
    }
    if (!confirmPanelChanges(QStringLiteral("exiting"))) {
        m_closing=false;event->ignore();return;
    }
    for (int i = m_tabs->count()-1; i >= 0; --i) {
        auto* editor = qobject_cast<ScriptEditor*>(m_tabs->widget(i));
        // The panel script was already handled as part of the panel project.
        if (editor == m_panelEditor || m_embeddedEditors.contains(editor)) continue;
        if (!confirmDiscard(editor)) {
            m_closing = false;
            event->ignore(); return;
        }
    }
    stopScript();
    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            QMessageBox::warning(this, QStringLiteral("Script still running"),
                QStringLiteral("The script did not stop; close cancelled to avoid destroying an active thread."));
            m_closing = false;
            event->ignore(); return;
        }
    }
    event->accept();
}

void MainWindow::buildScopeWorkspace() {
    auto* page = new QWidget(m_workspaces);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);
    auto* first = new QHBoxLayout;
    m_startScope = new QPushButton(QStringLiteral("Start Live"),page);
    m_singleScope = new QPushButton(QStringLiteral("Single"),page);
    m_stopScope = new QPushButton(QStringLiteral("Stop"),page);
    UiIcons::decorate(m_startScope, "run", QStringLiteral("Start continuous oscilloscope acquisition"));
    UiIcons::decorate(m_singleScope, "single", QStringLiteral("Capture one oscilloscope frame"));
    UiIcons::decorate(m_stopScope, "stop", QStringLiteral("Stop oscilloscope acquisition"));
    m_stopScope->setEnabled(false);
    first->addWidget(m_startScope);
    first->addWidget(m_singleScope);
    first->addWidget(m_stopScope);
    first->addSpacing(15);
    first->addWidget(new QLabel(QStringLiteral("Rate:"),page));
    m_scopeRate = new QDoubleSpinBox(page);
    m_scopeRate->setDecimals(2);
    m_scopeRate->setRange(0.01,100.0);
    m_scopeRate->setValue(1.0);
    m_scopeRate->setSuffix(QStringLiteral(" MS/s"));
    m_scopeRate->setToolTip(QStringLiteral("Requested sample rate; actual device limits depend on selected configuration"));
    first->addWidget(m_scopeRate);
    first->addWidget(new QLabel(QStringLiteral("Samples:"),page));
    m_scopeSamples = new QSpinBox(page);
    m_scopeSamples->setRange(64,32768);
    m_scopeSamples->setSingleStep(256);
    m_scopeSamples->setValue(4096);
    first->addWidget(m_scopeSamples);
    first->addWidget(new QLabel(QStringLiteral("Range:"),page));
    m_scopeRange = new QDoubleSpinBox(page);
    m_scopeRange->setRange(0.5,25.0);
    m_scopeRange->setSingleStep(0.5);
    m_scopeRange->setValue(5.0);
    m_scopeRange->setSuffix(QStringLiteral(" V"));
    m_scopeRange->setToolTip(QStringLiteral("Requested analog input range; actual hardware may quantize it"));
    first->addWidget(m_scopeRange);
    first->addStretch(1);
    layout->addLayout(first);

    auto* second = new QHBoxLayout;
    m_showCh1 = new QCheckBox(QStringLiteral("CH1"),page);
    m_showCh2 = new QCheckBox(QStringLiteral("CH2"),page);
    m_showCh1->setChecked(true);
    m_showCh2->setChecked(true);
    second->addWidget(m_showCh1);
    second->addWidget(m_showCh2);
    m_softwareAlignment = new QCheckBox(QStringLiteral("Display-only edge alignment (periodic signals)"),page);
    m_softwareAlignment->setChecked(false);
    m_softwareAlignment->setToolTip(QStringLiteral("Opt-in visual alignment only. Does not fix or alter hardware triggering, raw samples, statistics, or CSV output. Inserts gaps where shifted data are unavailable."));
    second->addWidget(m_softwareAlignment);
    second->addSpacing(12);
    m_testWavegen = new QCheckBox(QStringLiteral("Enable W1 1 kHz / 1 V test sine during capture"),page);
    m_testWavegen->setToolTip(QStringLiteral("Optional physical output. Connect W1 to Scope CH1+ and instrument GND to CH1− only when safe."));
    m_testWavegen->setEnabled(false); // simulation is selected on startup
    second->addWidget(m_testWavegen);
    second->addStretch(1);
    auto* fit = new QPushButton(QStringLiteral("Fit"),page);
    auto* csv = new QPushButton(QStringLiteral("Export CSV"),page);
    UiIcons::decorate(fit, "fit", QStringLiteral("Fit waveform data to the display"));
    UiIcons::decorate(csv, "export", QStringLiteral("Export captured waveform samples to CSV"));
    second->addWidget(fit);
    second->addWidget(csv);
    layout->addLayout(second);

    // Hardware edge-trigger controls: edits during Live safely stop/reopen the worker.
    auto* triggerRow = new QHBoxLayout;
    triggerRow->addWidget(new QLabel(QStringLiteral("Trigger:"),page));
    m_triggerMode = new QComboBox(page);
    m_triggerMode->addItems({QStringLiteral("Auto"),QStringLiteral("Normal"),QStringLiteral("None")});
    m_triggerMode->setToolTip(QStringLiteral("Auto: force capture after timeout; Normal: wait for edge; None: untriggered"));
    triggerRow->addWidget(m_triggerMode);
    triggerRow->addWidget(new QLabel(QStringLiteral("Source:"),page));
    m_triggerSource = new QComboBox(page);
    m_triggerSource->addItems({QStringLiteral("CH1"),QStringLiteral("CH2")});
    triggerRow->addWidget(m_triggerSource);
    m_triggerSlope = new QComboBox(page);
    m_triggerSlope->addItems({QStringLiteral("Rising"),QStringLiteral("Falling")});
    triggerRow->addWidget(m_triggerSlope);
    triggerRow->addWidget(new QLabel(QStringLiteral("Level:"),page));
    m_triggerLevel = new QDoubleSpinBox(page);
    m_triggerLevel->setDecimals(3);
    m_triggerLevel->setRange(-25.0,25.0);
    m_triggerLevel->setSingleStep(0.1);
    m_triggerLevel->setSuffix(QStringLiteral(" V"));
    m_triggerLevel->setValue(0.0);
    triggerRow->addWidget(m_triggerLevel);
    triggerRow->addWidget(new QLabel(QStringLiteral("Hysteresis:"),page));
    m_triggerHysteresis = new QDoubleSpinBox(page);
    m_triggerHysteresis->setDecimals(3);
    m_triggerHysteresis->setRange(0.0, 2.0);
    m_triggerHysteresis->setSingleStep(0.025);
    m_triggerHysteresis->setValue(0.050);
    m_triggerHysteresis->setSuffix(QStringLiteral(" V"));
    m_triggerHysteresis->setToolTip(QStringLiteral("Analog trigger hysteresis. Try 0.05 V on a 1 V sine to reject noise and wrong-edge events. 0 disables additional hysteresis."));
    triggerRow->addWidget(m_triggerHysteresis);
    triggerRow->addWidget(new QLabel(QStringLiteral("Position:"),page));
    m_triggerPosition = new QDoubleSpinBox(page);
    m_triggerPosition->setRange(0.0,100.0);
    m_triggerPosition->setDecimals(0);
    m_triggerPosition->setSingleStep(5.0);
    m_triggerPosition->setSuffix(QStringLiteral(" %"));
    m_triggerPosition->setValue(50.0);
    m_triggerPosition->setToolTip(QStringLiteral("Trigger location across the capture: 0% left, 50% center, 100% right"));
    triggerRow->addWidget(m_triggerPosition);
    triggerRow->addWidget(new QLabel(QStringLiteral("Auto timeout:"),page));
    m_triggerAutoTimeout = new QDoubleSpinBox(page);
    m_triggerAutoTimeout->setDecimals(2);
    m_triggerAutoTimeout->setRange(0.01,10.0);
    m_triggerAutoTimeout->setSingleStep(0.05);
    m_triggerAutoTimeout->setValue(0.20);
    m_triggerAutoTimeout->setSuffix(QStringLiteral(" s"));
    triggerRow->addWidget(m_triggerAutoTimeout);
    triggerRow->addStretch(1);
    layout->addLayout(triggerRow);

    // Separate calibrated vertical scales for CH1 and CH2. These edit the display,
    // not the ADC input range specified on the acquisition row.
    auto* voltageRow = new QHBoxLayout;
    voltageRow->addWidget(new QLabel(QStringLiteral("Vertical:"),page));
    m_scaleMode = new QComboBox(page);
    m_scaleMode->addItems({QStringLiteral("Fixed"),QStringLiteral("Auto fit")});
    voltageRow->addWidget(m_scaleMode);
    const auto addVoltsSpin = [page,voltageRow](const QString& label, double min,
                                               double max, double value, double step,
                                               const QString& suffix) {
        voltageRow->addWidget(new QLabel(label,page));
        auto* control = new QDoubleSpinBox(page);
        control->setDecimals(3);
        control->setRange(min,max);
        control->setSingleStep(step);
        control->setValue(value);
        control->setSuffix(suffix);
        voltageRow->addWidget(control);
        return control;
    };
    m_ch1Div = addVoltsSpin(QStringLiteral("CH1:"),0.001,25.0,0.5,0.1,QStringLiteral(" V/div"));
    m_ch1Center = addVoltsSpin(QStringLiteral("Center:"),-50.0,50.0,0.0,0.1,QStringLiteral(" V"));
    m_ch2Div = addVoltsSpin(QStringLiteral("CH2:"),0.001,25.0,0.5,0.1,QStringLiteral(" V/div"));
    m_ch2Center = addVoltsSpin(QStringLiteral("Center:"),-50.0,50.0,0.0,0.1,QStringLiteral(" V"));
    voltageRow->addStretch(1);
    layout->addLayout(voltageRow);

    m_scopePlot = new WaveformPlot(page);
    layout->addWidget(m_scopePlot,1);
    scaleControlChanged();
    m_scopeMetrics = new QLabel(QStringLiteral("CH1 / CH2 measurements will appear here"),page);
    m_scopeMetrics->setWordWrap(true);
    m_scopeMetrics->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_cursorReadout = new QLabel(QStringLiteral("Left click sets A | Right click sets B | Drag pans | Wheel zooms | Double-click resets"),page);
    m_cursorReadout->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_scopeInfo = new QLabel(QStringLiteral("Scope idle. Simulation by default; choose Physical device in the top toolbar for AD3."),page);
    m_scopeInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_scopeMetrics);
    layout->addWidget(m_cursorReadout);
    layout->addWidget(m_scopeInfo);
    m_workspaces->addTab(page,QStringLiteral("Oscilloscope"));

    // Scale controls update paint immediately, even while acquisition is running.
    connect(m_scaleMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){scaleControlChanged();});
    for (auto* control : {m_ch1Div,m_ch1Center,m_ch2Div,m_ch2Center})
        connect(control, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double){scaleControlChanged();});
    scaleControlChanged();

    connect(m_triggerMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){triggerControlChanged();});
    connect(m_triggerSource, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){triggerControlChanged();});
    connect(m_triggerSlope, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){triggerControlChanged();});
    for (auto* control : {m_triggerLevel,m_triggerHysteresis,m_triggerPosition,m_triggerAutoTimeout})
        connect(control, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double){triggerControlChanged();});

    connect(m_startScope,&QPushButton::clicked,this,[this]{startCapture(true);});
    connect(m_singleScope,&QPushButton::clicked,this,[this]{startCapture(false);});
    connect(m_stopScope,&QPushButton::clicked,this,&MainWindow::stopCapture);
    connect(m_showCh1,&QCheckBox::toggled,this,[this]{m_scopePlot->setChannelsVisible(m_showCh1->isChecked(),m_showCh2->isChecked());});
    connect(m_showCh2,&QCheckBox::toggled,this,[this]{m_scopePlot->setChannelsVisible(m_showCh1->isChecked(),m_showCh2->isChecked());});
    connect(m_softwareAlignment,&QCheckBox::toggled,m_scopePlot,&WaveformPlot::setSoftwareAlignmentEnabled);
    connect(fit,&QPushButton::clicked,m_scopePlot,&WaveformPlot::resetView);
    connect(csv,&QPushButton::clicked,this,&MainWindow::saveScopeCsv);
    connect(m_scopePlot,&WaveformPlot::cursorChanged,m_cursorReadout,&QLabel::setText);
}

void MainWindow::scaleControlChanged() {
    if (!m_scopePlot) return;
    const bool autoScale = m_scaleMode->currentIndex() == 1;
    for (auto* control : {m_ch1Div,m_ch1Center,m_ch2Div,m_ch2Center})
        control->setEnabled(!autoScale);
    m_scopePlot->setVoltageScales(autoScale, m_ch1Div->value(), m_ch2Div->value(),
                                 m_ch1Center->value(), m_ch2Center->value());
}

void MainWindow::triggerControlChanged() {
    if (!m_triggerMode) return;
    const bool enabled = m_triggerMode->currentIndex() != 2;
    for (auto* control : {static_cast<QWidget*>(m_triggerSource),
                          static_cast<QWidget*>(m_triggerSlope),
                          static_cast<QWidget*>(m_triggerLevel),
                          static_cast<QWidget*>(m_triggerPosition)})
        control->setEnabled(enabled && !m_thread);
    m_triggerAutoTimeout->setEnabled(m_triggerMode->currentIndex() == 0 && !m_thread);
    if (m_acquisitionThread && m_scopeContinuous && !m_closing) {
        // Stop the current acquisition and restart with the most recent settings.
        // The old and new workers never own the device at the same time.
        m_pendingTriggerRestart = true;
        m_scopeInfo->setText(QStringLiteral("Applying trigger setting; restarting acquisition..."));
        if (m_acquisitionWorker) m_acquisitionWorker->requestStop();
    }
}

void MainWindow::startCapture(bool continuous) {
    if (m_acquisitionThread || m_thread || m_panelThread) return;
    ScopeCaptureConfig config;
    config.physicalDevice = m_runtimeMode->currentIndex()==1;
    config.deviceIndex = m_deviceIndex->value();
    config.configIndex = m_configIndex->value()-1;
    config.samples = m_scopeSamples->value();
    config.sampleRateHz = m_scopeRate->value()*1e6;
    config.inputRange = m_scopeRange->value();
    config.continuous = continuous;
    config.trigger.mode = m_triggerMode->currentIndex() == 0 ? ScopeTriggerMode::Auto :
        m_triggerMode->currentIndex() == 1 ? ScopeTriggerMode::Normal : ScopeTriggerMode::None;
    config.trigger.channel = m_triggerSource->currentIndex();
    config.trigger.rising = m_triggerSlope->currentIndex() == 0;
    config.trigger.levelVolts = m_triggerLevel->value();
    config.trigger.hysteresisVolts = m_triggerHysteresis->value();
    config.trigger.positionPercent = m_triggerPosition->value();
    config.trigger.autoTimeoutSeconds = m_triggerAutoTimeout->value();
    config.testWavegen = config.physicalDevice && m_testWavegen->isChecked();
    const bool alreadyApprovedInternalRestart = m_skipWavegenConfirmationOnce;
    m_skipWavegenConfirmationOnce = false;
    if (config.testWavegen && !alreadyApprovedInternalRestart) {
        const auto result = QMessageBox::question(this,QStringLiteral("Enable physical W1 output?"),
            QStringLiteral("The AD3 will generate a 1 kHz, 1 V-peak sine on W1 during capture. "
                           "Verify your wiring and voltage limits. Continue?"),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    }
    m_scopeContinuous = continuous;
    auto* thread=new QThread(this);
    auto* worker=new ScopeAcquisitionWorker();
    m_acquisitionThread=thread;
    m_acquisitionWorker=worker;
    worker->moveToThread(thread);
    connect(thread,&QThread::started,worker,[worker,config]{ worker->execute(config); });
    connect(worker,&ScopeAcquisitionWorker::frameReady,this,&MainWindow::showFrame);
    connect(worker,&ScopeAcquisitionWorker::status,this,[this](const QString& status){
        m_scopeInfo->setText(status);
        m_console->appendPlainText(QStringLiteral("[Scope] %1").arg(status));
    });
    connect(worker,&ScopeAcquisitionWorker::finished,this,[this](const QString& reason,bool failed){
        m_scopeInfo->setText(reason);
        m_console->appendPlainText(QStringLiteral("[Scope] %1").arg(reason));
        if (failed) statusBar()->showMessage(reason,10000);
    });
    connect(worker,&ScopeAcquisitionWorker::finished,thread,&QThread::quit,Qt::DirectConnection);
    connect(thread,&QThread::finished,worker,&QObject::deleteLater);
    connect(thread,&QThread::finished,this,[this,thread]{
        if(m_acquisitionThread==thread){m_acquisitionWorker=nullptr;m_acquisitionThread=nullptr;}
        thread->deleteLater();
        const bool shouldRestart = m_pendingTriggerRestart && !m_closing && !m_thread;
        m_pendingTriggerRestart = false;
        refreshRunControls();
        if (shouldRestart) {
            // W1 was explicitly approved for the just-ended capture. Don't repeatedly
            // prompt for the same source when the trigger controls auto-apply.
            m_skipWavegenConfirmationOnce = m_testWavegen && m_testWavegen->isChecked();
            startCapture(true);
        }
    });
    m_scopeInfo->setText(QStringLiteral("Starting acquisition..."));
    thread->start();
    refreshRunControls();
}

void MainWindow::stopCapture() {
    m_pendingTriggerRestart = false;
    if (m_acquisitionWorker) {
        m_scopeInfo->setText(QStringLiteral("Stopping acquisition..."));
        m_acquisitionWorker->requestStop();
    }
}

void MainWindow::showFrame(const ScopeFrame& frame) {
    m_lastFrame=frame;
    m_scopePlot->setFrame(frame);
    // Panel charts receive only explicit plots.* script commands.
    m_scopeMetrics->setText(formatScopeMetrics(frame));
}

void MainWindow::refreshRunControls() {
    const bool scriptBusy = m_thread != nullptr || m_panelThread != nullptr;
    const bool scopeBusy = m_acquisitionThread != nullptr;
    if(m_runAction) m_runAction->setEnabled(!scriptBusy && !scopeBusy);
    if(m_panelDesigner && !m_panelThread) m_panelDesigner->setRunning(false);
    if(m_startScope) m_startScope->setEnabled(!scriptBusy && !scopeBusy);
    if(m_singleScope) m_singleScope->setEnabled(!scriptBusy && !scopeBusy);
    if(m_stopScope) m_stopScope->setEnabled(scopeBusy);
    if(m_triggerMode) m_triggerMode->setEnabled(!scriptBusy && (!scopeBusy || m_scopeContinuous));
    if(m_triggerSource && m_triggerMode) {
        const bool enabled = !scriptBusy && (!scopeBusy || m_scopeContinuous)
            && m_triggerMode->currentIndex() != 2;
        m_triggerSource->setEnabled(enabled);
        m_triggerSlope->setEnabled(enabled);
        m_triggerLevel->setEnabled(enabled);
        m_triggerPosition->setEnabled(enabled);
        m_triggerAutoTimeout->setEnabled(enabled && m_triggerMode->currentIndex() == 0);
    }
    if(m_runtimeMode) m_runtimeMode->setEnabled(!scriptBusy && !scopeBusy);
    if(m_deviceIndex) m_deviceIndex->setEnabled(!scriptBusy && !scopeBusy && m_runtimeMode && m_runtimeMode->currentIndex()==1);
    if(m_configIndex) m_configIndex->setEnabled(!scriptBusy && !scopeBusy && m_runtimeMode && m_runtimeMode->currentIndex()==1);
    if(m_deviceWorkspace) m_deviceWorkspace->setBusy(scriptBusy || scopeBusy);
    if(m_testWavegen) m_testWavegen->setEnabled(!scriptBusy && !scopeBusy && m_runtimeMode && m_runtimeMode->currentIndex()==1);
    for(QWidget* w : {static_cast<QWidget*>(m_scopeRate),static_cast<QWidget*>(m_scopeSamples),static_cast<QWidget*>(m_scopeRange)})
        if(w) w->setEnabled(!scriptBusy && !scopeBusy);
}

void MainWindow::saveScopeCsv() {
    if(m_lastFrame.ch1.isEmpty()) {
        QMessageBox::information(this,QStringLiteral("Export CSV"),QStringLiteral("Capture a waveform first."));
        return;
    }
    const QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Export captured samples"),
                                                   QStringLiteral("scope.csv"),QStringLiteral("CSV files (*.csv)"));
    if(path.isEmpty())return;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QMessageBox::warning(this,QStringLiteral("Export CSV"),file.errorString());return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "time_s,ch1_v,ch2_v\n";
    const auto n=m_lastFrame.ch1.size();
    for(int i=0;i<n;++i){
        stream << QString::number(double(i)/m_lastFrame.sampleRateHz,'g',14) << ','
               << QString::number(m_lastFrame.ch1.at(i),'g',14) << ',';
        if(i<m_lastFrame.ch2.size())stream << QString::number(m_lastFrame.ch2.at(i),'g',14);
        stream << '\n';
    }
    stream.flush();
    if(stream.status()!=QTextStream::Ok || !file.commit())
        QMessageBox::warning(this,QStringLiteral("Export CSV"),QStringLiteral("Unable to write CSV completely."));
    else statusBar()->showMessage(QStringLiteral("CSV exported: %1 samples").arg(n),5000);
}


// ------- Native user-designed panels (inspired by QtOpenDynoScreen) -------
// Bundled Panel examples live in src/resources/examples.qrc.
// The original 18-Page showcase remains in panels/ as a regression fixture.

void MainWindow::buildPanelWorkspace() {
    m_panelDesigner = new PanelDesigner(m_workspaces);
    m_workspaces->addTab(m_panelDesigner,QStringLiteral("Panel Designer"));
    connect(m_panelDesigner,&PanelDesigner::newRequested,this,&MainWindow::newPanelProject);
    connect(m_panelDesigner,&PanelDesigner::loadRequested,this,&MainWindow::loadPanelProject);
    connect(m_panelDesigner,&PanelDesigner::exampleRequested,this,&MainWindow::loadBuiltInExample);
    connect(m_panelDesigner,&PanelDesigner::saveRequested,this,&MainWindow::savePanelProject);
    connect(m_panelDesigner,&PanelDesigner::runRequested,this,&MainWindow::runPanel);
    connect(m_panelDesigner,&PanelDesigner::stopRequested,this,&MainWindow::stopPanel);
    connect(m_panelDesigner,&PanelDesigner::editScriptRequested,this,&MainWindow::showPanelScriptEditor);
    connect(m_panelDesigner,&PanelDesigner::librariesRequested,this,&MainWindow::manageLibraries);
    connect(m_panelDesigner,&PanelDesigner::testSequenceEditorRequested,this,&MainWindow::editTestSequence);
    connect(m_panelDesigner,&PanelDesigner::panelLifecycleEdited,this,
        [this](bool initializeEnabled,bool shutdownEnabled) {
            if(m_panelThread || m_loadingPanelPage)return;
            PanelLifecycleSettings next=m_panelProject.lifecycle;
            next.initializeEnabled=initializeEnabled;
            next.shutdownEnabled=shutdownEnabled;
            if(next.initializeEnabled==m_panelProject.lifecycle.initializeEnabled &&
               next.shutdownEnabled==m_panelProject.lifecycle.shutdownEnabled)return;
            m_panelProject.lifecycle=next;
            m_panelDirty=true;
            if(m_panelEditor)updateTabTitle(m_panelEditor);
        });
    connect(m_panelDesigner,&PanelDesigner::panelLifecycleFunctionRequested,
            this,&MainWindow::openPanelLifecycleFunction);
    connect(m_panelDesigner,&PanelDesigner::eventHandlerRequested,this,&MainWindow::openPanelEventHandler);
    connect(m_panelDesigner,&PanelDesigner::pageSelected,this,[this](const QString& id){showPanelPage(id);});
    connect(m_panelDesigner,&PanelDesigner::pageAppearanceEdited,this,
        [this](const QString& themeMode,const QString& panelBg,const QString& pageBg,bool grid) {
            if(m_panelThread || m_loadingPanelPage)return;
            PanelPage* page=m_panelProject.findPage(m_panelProject.activePageId);
            if(!page)return;
            PanelAppearance next=m_panelProject.appearance;
            next.themeMode=themeMode;next.background=panelBg;next.showDesignGrid=grid;
            if(!next.valid() || !PanelAppearance::validColor(pageBg))return;
            if(next.themeMode==m_panelProject.appearance.themeMode &&
               next.background==m_panelProject.appearance.background &&
               next.showDesignGrid==m_panelProject.appearance.showDesignGrid &&
               pageBg==page->background)return;
            m_panelProject.appearance=next;
            page->background=pageBg;
            m_panelDesigner->setPageAppearance(next,pageBg,page->name);
            m_panelDirty=true;
            if(m_panelEditor)updateTabTitle(m_panelEditor);
        });
    connect(m_panelDesigner,&PanelDesigner::addPageRequested,this,&MainWindow::addPanelPage);
    connect(m_panelDesigner,&PanelDesigner::renamePageRequested,this,&MainWindow::renamePanelPage);
    connect(m_panelDesigner,&PanelDesigner::deletePageRequested,this,&MainWindow::deletePanelPage);
    connect(m_panelDesigner,&PanelDesigner::undoPageRequested,this,&MainWindow::undoPanelPage);
    connect(m_panelDesigner,&PanelDesigner::redoPageRequested,this,&MainWindow::redoPanelPage);
    connect(m_panelDesigner->canvas(),&PanelCanvas::documentChanged,this,[this]{
        if(m_loadingPanelPage || m_panelThread)return;
        // Only the active Page is represented by the canvas at any moment.
        PanelPage* current=m_panelProject.findPage(m_panelProject.activePageId);
        if(!current)return;
        current->controls=m_panelDesigner->canvas()->toJson();
        current->size=m_panelDesigner->panelSize();
        m_panelDirty=true;
        if (m_panelEditor) updateTabTitle(m_panelEditor);
    });
}

void MainWindow::syncVisiblePage() {
    if(!m_panelDesigner || m_loadingPanelPage || m_panelThread)return;
    if(PanelPage* page=m_panelProject.findPage(m_panelProject.activePageId)) {
        page->controls=m_panelDesigner->canvas()->toJson();
        page->size=m_panelDesigner->panelSize();
    }
}
bool MainWindow::preparePageWidgetsForRun() {
    // Every widget must exist BEFORE JavaScript onInitialize() can call
    // ui.set(), even on a Page the user has never visited. Build each Page
    // once, keep it hidden, then return to the selected Page. No value cache.
    if(!m_panelDesigner)return false;
    PanelCanvas* canvas=m_panelDesigner->canvas();
    const QString active=m_panelProject.activePageId;
    m_loadingPanelPage=true;
    canvas->setReservedIds({});
    QString error;
    bool ok=true;
    for(const auto& page:m_panelProject.pages) {
        if(!canvas->activatePage(page.id,page.controls,page.size,&error)) {
            ok=false;
            break;
        }
    }
    if(const PanelPage* selected=m_panelProject.findPage(active)) {
        QString restoreError;
        if(!canvas->activatePage(active,selected->controls,selected->size,&restoreError)) {
            ok=false;
            error=restoreError;
        }
    }
    m_loadingPanelPage=false;
    refreshPageTabs();
    if(!ok)QMessageBox::warning(this,QStringLiteral("Cannot start Panel"),
        QStringLiteral("Could not create all Page widgets: %1").arg(error));
    return ok;
}
MainWindow::PageSnapshot MainWindow::pageSnapshot() const {
    return {m_panelProject.pages,m_panelProject.activePageId};
}
void MainWindow::resetPageHistory() {
    m_pageHistory.clear();m_pageHistoryIndex=0;
    if(m_panelDesigner)m_panelDesigner->setPageHistoryEnabled(false,false);
}
void MainWindow::recordPageHistory(const PageSnapshot& before) {
    if(m_pageHistoryIndex<m_pageHistory.size())m_pageHistory.resize(m_pageHistoryIndex);
    m_pageHistory.append(PageHistoryEntry{before,pageSnapshot()});
    m_pageHistoryIndex=m_pageHistory.size();
    m_panelDesigner->setPageHistoryEnabled(true,false);
    m_panelDirty=true;
    if(m_panelEditor)updateTabTitle(m_panelEditor);
}
void MainWindow::refreshPageTabs() {
    if(!m_panelDesigner)return;
    QVector<QPair<QString,QString>> entries;
    for(const auto& page:m_panelProject.pages)entries.append({page.id,page.name});
    m_panelDesigner->setPageTabs(entries,m_panelProject.activePageId);
    if(const PanelPage* page=m_panelProject.findPage(m_panelProject.activePageId))
        m_panelDesigner->setPageAppearance(m_panelProject.appearance,page->background,page->name);
    m_panelDesigner->setPanelLifecycle(m_panelProject.lifecycle);
    m_panelDesigner->setPageHistoryEnabled(!m_panelThread && m_pageHistoryIndex>0,
                                      !m_panelThread && m_pageHistoryIndex<m_pageHistory.size());
    QSet<QString> otherIds;
    for(const auto& page:m_panelProject.pages) {
        if(page.id==m_panelProject.activePageId)continue;
        for(const auto& value:page.controls)
            otherIds.insert(value.toObject().value(QStringLiteral("id")).toString());
    }
    m_panelDesigner->canvas()->setReservedIds(otherIds);
}
bool MainWindow::showPanelPage(const QString& id) {
    if(!m_panelDesigner || m_loadingPanelPage)return false;
    const PanelPage* page=m_panelProject.findPage(id);
    if(!page)return false;
    PanelCanvas* canvas=m_panelDesigner->canvas();
    if(id!=m_panelProject.activePageId)syncVisiblePage();
    page=m_panelProject.findPage(id);
    m_loadingPanelPage=true;
    canvas->setReservedIds({}); // other-Page IDs installed after activation
    QString error;
    const bool ok=canvas->activatePage(id,page->controls,page->size,&error);
    if(ok) {
        m_panelProject.activePageId=id;
        canvas->setDesignMode(!m_panelThread);
    }
    m_loadingPanelPage=false;
    if(!ok)QMessageBox::warning(this,QStringLiteral("Cannot display Page"),error);
    refreshPageTabs();
    return ok;
}
void MainWindow::applyPageSnapshot(const PageSnapshot& snapshot) {
    m_panelProject.pages=snapshot.pages;
    m_panelProject.activePageId=snapshot.activeId;
    // clearControls() emits documentChanged(). Suppress the normal active-page
    // save while resetting or it would overwrite the restored snapshot.
    m_loadingPanelPage=true;
    m_panelDesigner->canvas()->resetPages();
    m_loadingPanelPage=false;
    showPanelPage(snapshot.activeId);
    m_panelDirty=true;
    if(m_panelEditor)updateTabTitle(m_panelEditor);
}
void MainWindow::undoPanelPage() {
    if(m_panelThread || m_pageHistoryIndex<=0)return;
    syncVisiblePage();
    // Retain widget edits made after the structural operation for Redo.
    m_pageHistory[m_pageHistoryIndex-1].after=pageSnapshot();
    --m_pageHistoryIndex;
    applyPageSnapshot(m_pageHistory.at(m_pageHistoryIndex).before);
}
void MainWindow::redoPanelPage() {
    if(m_panelThread || m_pageHistoryIndex>=m_pageHistory.size())return;
    syncVisiblePage();
    m_pageHistory[m_pageHistoryIndex].before=pageSnapshot();
    applyPageSnapshot(m_pageHistory.at(m_pageHistoryIndex++).after);
}
void MainWindow::addPanelPage() {
    if(m_panelThread || m_panelProject.pages.size()>=64)return;
    syncVisiblePage();
    bool accepted=false;
    const QString name=QInputDialog::getText(this,QStringLiteral("Add Page"),
        QStringLiteral("Page name:"),QLineEdit::Normal,QStringLiteral("Page %1").arg(m_panelProject.pages.size()+1),&accepted).trimmed();
    if(!accepted || name.isEmpty())return;
    const PageSnapshot before=pageSnapshot();
    int n=1;QString id;
    do {id=QStringLiteral("page_%1").arg(n++);}while(m_panelProject.findPage(id));
    QString error;
    const QSize size=m_panelDesigner->panelSize();
    if(!m_panelProject.addPage(id,name,size,&error)) {
        QMessageBox::warning(this,QStringLiteral("Cannot add Page"),error);return;
    }
    showPanelPage(id);
    recordPageHistory(before);
}
void MainWindow::renamePanelPage() {
    if(m_panelThread)return;
    syncVisiblePage();
    PanelPage* page=m_panelProject.findPage(m_panelProject.activePageId);
    if(!page)return;
    bool accepted=false;
    const QString name=QInputDialog::getText(this,QStringLiteral("Rename Page"),
        QStringLiteral("Page name:"),QLineEdit::Normal,page->name,&accepted).trimmed();
    if(!accepted || name.isEmpty() || name==page->name)return;
    if(name.size()>80){QMessageBox::warning(this,"Page name","Maximum 80 characters.");return;}
    for(const auto& other:m_panelProject.pages) {
        if(other.id!=page->id && other.name.compare(name,Qt::CaseInsensitive)==0) {
            QMessageBox::warning(this,"Page name","A Page with that name already exists.");return;
        }
    }
    const PageSnapshot before=pageSnapshot();
    page->name=name;
    refreshPageTabs();
    recordPageHistory(before);
}
void MainWindow::deletePanelPage() {
    if(m_panelThread || m_panelProject.pages.size()<=1)return;
    syncVisiblePage();
    const QString id=m_panelProject.activePageId;
    const PanelPage* page=m_panelProject.findPage(id);
    if(!page)return;
    if(QMessageBox::question(this,QStringLiteral("Delete Page"),
          QStringLiteral("Delete Page '%1' and all %2 widgets on it?\n"
                         "Use Undo Page to restore the Page and its widgets.")
            .arg(page->name).arg(page->controls.size()),
          QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    const PageSnapshot before=pageSnapshot();
    QString error;
    if(!m_panelProject.deletePage(id,&error)) {
        QMessageBox::warning(this,QStringLiteral("Cannot delete Page"),error);return;
    }
    showPanelPage(m_panelProject.activePageId);
    m_panelDesigner->canvas()->forgetPage(id);
    recordPageHistory(before);
}

void MainWindow::openPanelLifecycleFunction(const QString& name) {
    if(name!=QStringLiteral("onInitialize") && name!=QStringLiteral("onShutdown"))return;
    if(m_panelThread)return;
    showPanelScriptEditor();
    if(!m_panelEditor)return;
    const QRegularExpression matcher(
        QStringLiteral(R"(\bfunction\s+%1\s*\()") .arg(QRegularExpression::escape(name)));
    // Only insert boilerplate if there is no conventional function declaration.
    // The user can still implement hooks dynamically; no code is executed here.
    if(!matcher.match(m_panelEditor->toPlainText()).hasMatch()) {
        QString comment;
        if(name==QStringLiteral("onInitialize"))
            comment=QStringLiteral("/** Called once after libraries and the panel script load.\n"
                                   " * Initialize application state and configure controls here.\n"
                                   " * @returns {void}\n */\n"
                                   "function onInitialize() {\n    print(\"Panel initialized\");\n}\n");
        else
            comment=QStringLiteral("/** Called once on normal Stop, before native resources are released.\n"
                                   " * Not called when JavaScript is forcibly interrupted.\n"
                                   " * @returns {void}\n */\n"
                                   "function onShutdown() {\n    print(\"Panel shutdown\");\n}\n");
        QTextCursor cursor=m_panelEditor->textCursor();
        cursor.movePosition(QTextCursor::End);
        const QString existing=m_panelEditor->toPlainText();
        cursor.insertText((existing.isEmpty() || existing.endsWith(QLatin1Char('\n')))
                           ? QStringLiteral("\n")+comment
                           : QStringLiteral("\n\n")+comment);
        m_panelEditor->setTextCursor(cursor);
    }
    const auto match=matcher.match(m_panelEditor->toPlainText());
    if(match.hasMatch()) {
        QTextCursor cursor=m_panelEditor->textCursor();
        cursor.setPosition(match.capturedStart());
        m_panelEditor->setTextCursor(cursor);
        m_panelEditor->centerCursor();
    }
    m_panelEditor->setFocus();
}

void MainWindow::showPanelScriptEditor() {
    if (!m_panelDesigner) return;
    if (!m_panelEditor || m_tabs->indexOf(m_panelEditor) < 0) {
        m_panelEditor = newTab(m_panelProject.scriptName, m_panelProject.script);
        auto* editor = m_panelEditor.data();
        connect(editor->document(), &QTextDocument::contentsChanged, this,
            [this, view=QPointer<ScriptEditor>(editor)] {
                if (!view || m_panelEditor != view) return;
                const QString current = view->toPlainText();
                m_panelDesigner->setPanelScriptSource(current);
                if (m_panelProject.script != current) {
                    m_panelProject.script = current;
                    m_panelDirty = true;
                    updateTabTitle(view);
                }
            });
        updateTabTitle(editor);
    }
    m_tabs->setCurrentWidget(m_panelEditor);
    m_workspaces->setCurrentWidget(m_scriptWorkspace);
    m_panelEditor->setFocus();
}

void MainWindow::replacePanelScript(const QString& tabName, const QString& source) {
    // The caller must first resolve unsaved changes to the previous project.
    // Do not leave an orphaned 'panel' tab associated with a different project.
    if (m_panelEditor) {
        auto* old = m_panelEditor.data();
        m_panelEditor = nullptr;
        const int index = m_tabs->indexOf(old);
        m_paths.remove(old);
        m_runtimeErrors.remove(old);
        if (index >= 0) m_tabs->removeTab(index);
        old->deleteLater();
    }
    m_panelProject.scriptName = tabName;
    m_panelProject.script = source;
    m_panelDesigner->setPanelScriptSource(source);
    m_panelDirty = false;
    showPanelScriptEditor();
}

bool MainWindow::confirmPanelChanges(const QString& operation) {
    if (!m_panelDirty) return true;
    const auto response = QMessageBox::question(this, QStringLiteral("Unsaved panel project"),
        QStringLiteral("The panel layout or its JavaScript has unsaved changes. "
                       "Save the panel project before %1?").arg(operation),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (response == QMessageBox::Cancel) return false;
    if (response == QMessageBox::Save) return savePanelProjectToFile(false);
    return true;
}

// Activate validated .wfpanel content without tying the native widget layer
// to whether it originated in a read-only resource or on the filesystem.
bool MainWindow::activatePanelProject(PanelProject loaded, bool fromResource,
                                      const QString& sourceName) {
    if (!m_panelDesigner || m_panelThread) return false;
    QString error;
    for (const auto& page : loaded.pages) {
        if (!PanelCanvas::validateJson(page.controls, &error)) {
            QMessageBox::warning(this, QStringLiteral("Invalid panel"),
                QStringLiteral("Page '%1': %2").arg(page.name,error));
            return false;
        }
    }
    const PanelPage* visible=loaded.findPage(loaded.activePageId);
    if (!visible) {
        QMessageBox::warning(this,QStringLiteral("Invalid panel"),
                             QStringLiteral("No active Page found"));
        return false;
    }
    m_loadingPanelPage=true;
    m_panelDesigner->canvas()->setReservedIds({});
    m_panelDesigner->canvas()->resetPages();
    m_panelDesigner->canvas()->setRunInformationDefinition(loaded.runInformation);
    const bool activated=m_panelDesigner->canvas()->activatePage(
        visible->id,visible->controls,visible->size,&error);
    m_loadingPanelPage=false;
    if (!activated) {
        QMessageBox::warning(this,QStringLiteral("Invalid panel"),error);
        return false;
    }
    closeEmbeddedEditors();
    m_panelProject=std::move(loaded);
    if (PanelPage* page=m_panelProject.findPage(m_panelProject.activePageId))
        page->size=m_panelDesigner->panelSize();
    resetPageHistory();
    refreshPageTabs();
    m_panelProblems.clear();
    replacePanelScript(m_panelProject.scriptName,m_panelProject.script);
    // Built-in Qt resources are read-only: Save/Export must choose a real file.
    m_panelProject.path=fromResource ? QString() : sourceName;
    m_panelDirty=false;
    if (m_panelEditor) updateTabTitle(m_panelEditor);
    m_workspaces->setCurrentWidget(m_panelDesigner);
    statusBar()->showMessage((fromResource ? QStringLiteral("Example loaded (Save/Export to customize): ") :
                                             QStringLiteral("Panel loaded: "))+sourceName,5000);
    return true;
}

void MainWindow::makeExamplePanel() {
    // Small, safe Getting Started Panel replaces the former 18-Page startup.
    const QString resource=QStringLiteral(":/wfscript/examples/GettingStarted.wfpanel");
    PanelProject loaded;
    QString error;
    if (!PanelProject::loadFile(resource,&loaded,&error)) {
        QMessageBox::warning(this,QStringLiteral("Default panel error"),error);
        return;
    }
    activatePanelProject(std::move(loaded),true,resource);
}

void MainWindow::loadBuiltInExample(const QString& resource) {
    if (m_panelThread || m_thread || m_acquisitionThread) {
        QMessageBox::information(this,QStringLiteral("Instrument busy"),
            QStringLiteral("Stop the running Panel or instrument before loading an example."));
        return;
    }
    if (!resource.startsWith(QStringLiteral(":/wfscript/examples/")) ||
        !resource.endsWith(QStringLiteral(".wfpanel"),Qt::CaseInsensitive)) return;
    if (!confirmPanelChanges(QStringLiteral("opening another example"))) return;
    PanelProject loaded;
    QString error;
    if (!PanelProject::loadFile(resource,&loaded,&error)) {
        QMessageBox::warning(this,QStringLiteral("Example load failed"),error);
        return;
    }
    activatePanelProject(std::move(loaded),true,resource);
}

void MainWindow::newPanelProject() {
    if(m_panelThread) return;
    if(!confirmPanelChanges(QStringLiteral("creating a new panel"))) return;
    m_panelDesigner->canvas()->setReservedIds({});
    m_panelDesigner->canvas()->resetPages();
    m_panelDesigner->setPanelSize(QSize(900,550));
    m_panelDesigner->canvas()->resetDesignHistory();
    closeEmbeddedEditors();
    m_panelProject=PanelProject::newProject(QStringLiteral("New Panel.js"),
        QStringLiteral("// Define panel events with ui.on('button1', 'clicked', function() { ... });\n"));
    m_panelDesigner->canvas()->activatePage(m_panelProject.activePageId,
        m_panelProject.mainPage().controls,m_panelProject.mainPage().size);
    resetPageHistory();
    refreshPageTabs();
    m_panelProblems.clear();
    replacePanelScript(m_panelProject.scriptName,m_panelProject.script);
    m_panelProject.path.clear();
    m_panelDirty=false;
    if(m_panelEditor) updateTabTitle(m_panelEditor);
    m_workspaces->setCurrentWidget(m_panelDesigner);
}

bool MainWindow::savePanelProjectToFile(bool saveAs) {
    if (m_panelThread || !m_panelDesigner) return false;
    QString name = saveAs ? QString() : m_panelProject.path;
    if (name.isEmpty()) {
        name = QFileDialog::getSaveFileName(this, QStringLiteral("Save WFScript Panel"),
            m_panelProject.path.isEmpty() ? QStringLiteral("MyExperiment.wfpanel") : m_panelProject.path,
            QStringLiteral("WFScript panel (*.wfpanel)"));
        if (name.isEmpty()) return false;
    }
    if (!name.endsWith(QStringLiteral(".wfpanel"),Qt::CaseInsensitive))
        name += QStringLiteral(".wfpanel");
    const QString priorScriptName=m_panelProject.scriptName;
    if(m_panelProject.scriptName==QStringLiteral("New Panel.js") ||
       m_panelProject.scriptName==QStringLiteral("Panel Example.js"))
        m_panelProject.scriptName=QFileInfo(name).completeBaseName()+QStringLiteral(".js");
    if (m_panelEditor) m_panelProject.script = m_panelEditor->toPlainText();
    // Save the active canvas; the document already owns all other Pages.
    syncVisiblePage();
    QString error;
    if (!m_panelProject.saveFile(name,&error)) {
        m_panelProject.scriptName=priorScriptName;
        QMessageBox::warning(this,QStringLiteral("Panel save failed"),error);
        return false;
    }
    m_panelProject.path=name;
    // The selected script name was persisted atomically with the project.
    m_panelDirty=false;
    if (m_panelEditor) {
        m_panelEditor->setProperty("displayName",m_panelProject.scriptName);
        m_panelEditor->document()->setModified(false);
        updateTabTitle(m_panelEditor);
    }
    for (ScriptEditor* ed:m_embeddedEditors.keys()) {
        ed->document()->setModified(false);
        updateTabTitle(ed);
    }
    statusBar()->showMessage(QStringLiteral("Panel saved (layout + JavaScript): ")+name,5000);
    return true;
}

void MainWindow::savePanelProject() {
    // The Panel Designer's 'Save/Export…' button always offers a destination;
    // Ctrl+S in the embedded script tab saves to the established project path.
    savePanelProjectToFile(true);
}
void MainWindow::loadPanelProject() {
    if(m_panelThread) return;
    if(!confirmPanelChanges(QStringLiteral("opening another panel"))) return;
    const QString path=QFileDialog::getOpenFileName(this,"Open WFScript Panel",QString(),"WFScript panel (*.wfpanel)");
    if(path.isEmpty()) return;
    PanelProject loaded;
    QString error;
    if(!PanelProject::loadFile(path,&loaded,&error)) {
        QMessageBox::warning(this,QStringLiteral("Invalid panel"),error);
        return;
    }
    activatePanelProject(std::move(loaded),false,path);
}
void MainWindow::runPanel() {
    if(!m_panelDesigner || m_panelThread || m_thread || m_acquisitionThread) {
        QMessageBox::information(this,"Panel busy","Stop the oscilloscope/script first, or open a panel project.");return;
    }
    const bool physical=m_runtimeMode->currentIndex()==1;
    if(physical && QMessageBox::warning(this,"Physical AD3 control",
        "Panel JavaScript can enable W1 and change connected hardware outputs. Run only with a safe test circuit.",
        QMessageBox::Ok|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Ok)return;
    syncVisiblePage(); // capture the last design edits before entering Run mode
    if(!preparePageWidgetsForRun())return;
    auto* worker=new PanelRuntime;
    auto* thread=new QThread(this);
    worker->moveToThread(thread);
    m_panelRuntime=worker;m_panelThread=thread;
    m_panelStopPending=false;
    m_panelWorkerCleanupComplete=false;
    ++m_panelRunGeneration;
    // The current panel script remains available even if its editor tab was closed.
    if (m_panelEditor) m_panelProject.script = m_panelEditor->toPlainText();
    const QString source=m_panelProject.script;
    // Derive script values from each factory's typed default, for every Page.
    // A selector starts as a string; a switch as bool; analog controls as double.
    const QVariantMap values=WidgetInitialValues::fromPages(m_panelProject.pages);
    QJsonArray panelControls;
    for(const auto& page:m_panelProject.pages)
        for(const auto& entry:page.controls)
            panelControls.append(entry); // all Pages share the handler registry
    QVariantMap embedded;
    for (const auto& entry:m_panelProject.libraries) {
        const QJsonObject lib=entry.toObject();
        embedded.insert(lib.value("name").toString(),lib.value("source").toString());
    }
    const QString projectFile=m_panelProject.path;
    const QString scriptName=m_panelProject.scriptName;
    const PanelLifecycleSettings lifecycle=m_panelProject.lifecycle;
    const QJsonObject testDefinition=m_panelProject.testDefinition;
    const QJsonObject runInformation=m_panelProject.runInformation;
    const QJsonObject reportStorage=m_panelProject.reportStorage;
    const QVariantMap runInformationInput=m_panelDesigner->canvas()->runInformationInputValues();
    const QString testLibrary=m_panelProject.testLibrary;
    m_panelProblems.clear();
    rebuildProblems();
    const int device=m_deviceIndex->value(),config=m_configIndex->value()-1;
    // CRITICAL: use a queued startup. QThread::started is emitted before
    // QThread::exec() enters its event loop. With a direct callback, a fast
    // failure/stop could call QThread::quit() before the loop was running,
    // leaving a zombie worker thread even after the worker emitted stopped().
    // Queued delivery guarantees start() runs inside the active event loop.
    connect(thread,&QThread::started,worker,[worker,source,values,physical,device,config,projectFile,embedded,scriptName,lifecycle,panelControls,testDefinition,runInformation,runInformationInput,testLibrary,reportStorage]{
        worker->start(source,values,physical,device,config,projectFile,embedded,scriptName,lifecycle,panelControls,testDefinition,testLibrary,runInformation,runInformationInput,reportStorage);
    },Qt::QueuedConnection);
    connect(m_panelDesigner->canvas(),&PanelCanvas::controlEvent,worker,&PanelRuntime::eventFromPanel,Qt::QueuedConnection);
    const quint64 panelGeneration=m_panelRunGeneration;
    connect(worker,&PanelRuntime::controlUpdate,this,[this,panelGeneration](const QString& id,const QVariant& value){
        if(!m_panelThread || m_panelRunGeneration!=panelGeneration)return;
        // Updates go straight to the original QWidget, including hidden Pages.
        if(m_panelDesigner)m_panelDesigner->canvas()->applyValue(id,value);
    },Qt::QueuedConnection);
    connect(worker,&PanelRuntime::plotCommandReady,this,[this,panelGeneration](const PlotCommand& command) {
        if(!m_panelThread || m_panelRunGeneration!=panelGeneration)return;
        if(m_panelDesigner)m_panelDesigner->canvas()->applyPlotCommand(command);
    },Qt::QueuedConnection);
    connect(worker,&PanelRuntime::scopeFrameReady,this,&MainWindow::showFrame,Qt::QueuedConnection);
    connect(worker,&PanelRuntime::printed,this,[this](const QString& text){m_console->appendPlainText("[Panel] "+text);});
    connect(worker,&PanelRuntime::consoleClearRequested,this,[this]{m_console->clear();});
    connect(worker,&PanelRuntime::failed,this,[this](const QString& text){m_console->appendPlainText("[Panel ERROR] "+text);});
    connect(worker,&PanelRuntime::scriptDiagnostic,this,
        [this,scriptName](const QString& source,int line,int column,
                          const QString& context,const QString& message,const QString& stack){
            RuntimeProblem issue;
            issue.source=source;issue.line=line;issue.column=column;
            issue.message=message;issue.context=context;issue.stack=stack;
            m_panelProblems.append(issue);
            QString location=source+QStringLiteral(":%1").arg(line);
            if(column>0)location+=QStringLiteral(":%1").arg(column);
            m_console->appendPlainText(QStringLiteral("[Panel ERROR] %1 [%2] %3")
                                       .arg(location,context,message));
            if(!stack.isEmpty())m_console->appendPlainText(QStringLiteral("[Panel TRACE] ")+stack);
            if(source==scriptName && m_panelEditor) m_panelEditor->highlightErrorLine(line);
            for(auto it=m_embeddedEditors.cbegin();it!=m_embeddedEditors.cend();++it)
                if(it.value()==source)it.key()->highlightErrorLine(line);
            rebuildProblems();
            m_problemsDock->show();
            statusBar()->showMessage(QStringLiteral("JavaScript error in %1").arg(location),8000);
        });
    connect(worker,&PanelRuntime::lifecycleChanged,this,[this](const QString& state,const QString& detail){
        m_console->appendPlainText(QStringLiteral("[Panel] Runtime %1: %2").arg(state,detail));
        if (state==QStringLiteral("Failed"))
            statusBar()->showMessage(QStringLiteral("Panel startup failed: %1").arg(detail),10000);
        else if (state==QStringLiteral("Running"))
            statusBar()->showMessage(QStringLiteral("Panel running — controls are live"));
    });
    connect(worker,&PanelRuntime::ready,this,[this]{statusBar()->showMessage("Panel running — controls are live");});
    // Worker cleanup and QThread termination are distinct milestones.
    connect(worker,&PanelRuntime::stopped,this,[this,thread]{
        if(m_panelThread!=thread)return;
        m_panelWorkerCleanupComplete=true;
    });
    // quit() is thread-safe. Since startup is queued, the QThread event loop
    // is running before PanelRuntime can emit stopped().
    connect(worker,&PanelRuntime::stopped,thread,&QThread::quit,Qt::DirectConnection);
    connect(thread,&QThread::finished,worker,&QObject::deleteLater);
    connect(thread,&QThread::finished,this,[this,thread]{
        finalizePanelThread(thread);
    });
    m_console->appendPlainText("[Panel] Starting project runtime...");
    m_panelDesigner->setRunning(true);
    refreshPageTabs();
    thread->start();refreshRunControls();
}
void MainWindow::finalizePanelThread(QThread* thread) {
    if(!thread)return;
    // QThread::finished is emitted just before the native thread exits;
    // thread_local destructors can still be running at that moment. Qt's
    // documentation recommends wait() to verify full termination. Never
    // report Stopped or permit another panel engine until wait(0) succeeds.
    if(!thread->wait(0UL)) {
        const QPointer<QThread> pending=thread;
        QTimer::singleShot(25,this,[this,pending]{
            if(pending)finalizePanelThread(pending.data());
        });
        return;
    }
    if(m_panelThread==thread) {
        m_panelRuntime=nullptr;
        m_panelThread=nullptr;
        m_panelStopPending=false;
        m_panelWorkerCleanupComplete=false;
        m_console->appendPlainText(QStringLiteral(
            "[Panel] Runtime Stopped: Worker thread exited; Design mode"));
        if(m_panelDesigner) {
            m_panelDesigner->setRunning(false);
            // Restore design-time defaults without destroying any Page widgets.
            for(const auto& page:m_panelProject.pages)
                m_panelDesigner->canvas()->restoreDocumentValues(page.controls);
            refreshPageTabs();
        }
        refreshRunControls();
        statusBar()->showMessage("Panel stopped — Design mode",4000);
    }
    thread->deleteLater();
}

void MainWindow::stopPanel() {
    if(!m_panelRuntime || !m_panelThread)return;
    if(m_panelStopPending) {
        // A second Stop while onShutdown() is executing is an emergency
        // cancellation. The first Stop allows orderly cleanup; the next one
        // interrupts any active JavaScript, but never force-kills a QThread.
        m_panelRuntime->requestStop();
        m_console->appendPlainText(QStringLiteral(
            "[Panel] Additional Stop requested: interrupt active JavaScript if needed"));
        return;
    }
    m_panelStopPending=true;
    m_console->appendPlainText(QStringLiteral("[Panel] Runtime StopRequested: Panel stop requested"));
    m_panelRuntime->requestStop();
    statusBar()->showMessage(QStringLiteral("Stopping panel JavaScript..."));
    m_console->appendPlainText(QStringLiteral(
        "[Panel] Runtime Interrupting: Interrupt sent; waiting for worker cleanup"));
    // A queued stop cannot run until a JavaScript callback returns. The
    // thread-safe QJSEngine interruption above breaks CPU-bound JavaScript.
    QMetaObject::invokeMethod(m_panelRuntime,"stop",Qt::QueuedConnection);
    const QPointer<QThread> stoppingThread=m_panelThread;
    const quint64 runGeneration=m_panelRunGeneration;
    // A watchdog must be tied to this exact run AND to actual thread liveness.
    // A CleanupComplete signal alone must not imply a hung native SDK call.
    QTimer::singleShot(3000,this,[this,stoppingThread,runGeneration]{
        if(!stoppingThread || m_panelThread!=stoppingThread ||
           m_panelRunGeneration!=runGeneration || !m_panelStopPending)return;
        // Nonblocking wait is the only reliable proof that QThread has fully
        // exited, including thread-local destructor cleanup after finished().
        if(stoppingThread->wait(0UL))return;
        if(m_panelWorkerCleanupComplete){
            m_console->appendPlainText(QStringLiteral(
                "[Panel WARNING] Worker cleanup completed, but the Qt thread "
                "has not finished after 3 seconds. Waiting for complete native thread exit; "
                "no forced termination."));
            statusBar()->showMessage(QStringLiteral("Panel cleanup complete — thread exit pending"));
        }else{
            m_console->appendPlainText(QStringLiteral(
                "[Panel WARNING] Panel worker is still active 3 seconds after Stop. "
                "JavaScript interruption, native execution or cleanup may be pending. "
                "The thread has not been forcibly terminated."));
            statusBar()->showMessage(QStringLiteral("Panel worker still active — awaiting shutdown"));
        }
    });
}


bool MainWindow::setEmbeddedLibrary(const QString& name,const QString& source) {
    if (!ScriptLibraryBridge::validLibraryName(name) || source.toUtf8().size()>1024*1024) return false;
    // Closing an unchanged library tab must not mark the project dirty.
    for (const auto& v:m_panelProject.libraries) {
        const QJsonObject existing=v.toObject();
        if (existing.value("name").toString()==name && existing.value("source").toString()==source)
            return true;
        if (existing.value("name").toString().compare(name,Qt::CaseInsensitive)==0
            && existing.value("name").toString()!=name) return false;
    }
    qsizetype total=source.toUtf8().size();
    QJsonArray next;
    for (const auto& v:m_panelProject.libraries) {
        const auto obj=v.toObject();
        if (obj.value("name").toString()==name) continue;
        total+=obj.value("source").toString().toUtf8().size();
        next.append(obj);
    }
    if(total>2*1024*1024 || next.size()>=64)return false;
    next.append(QJsonObject{{"name",name},{"source",source}});
    m_panelProject.libraries=next;
    m_panelDirty=true;
    if(m_panelEditor)updateTabTitle(m_panelEditor);
    return true;
}

void MainWindow::closeEmbeddedEditors() {
    const auto editors=m_embeddedEditors.keys();
    for (ScriptEditor* editor:editors) {
        m_embeddedEditors.remove(editor);
        m_paths.remove(editor);m_runtimeErrors.remove(editor);
        const int index=m_tabs->indexOf(editor);
        if(index>=0)m_tabs->removeTab(index);
        editor->deleteLater();
    }
}

void MainWindow::showPanelLibraryEditor(const QString& name) {
    for (auto it=m_embeddedEditors.cbegin();it!=m_embeddedEditors.cend();++it)
        if (it.value()==name) {
            m_tabs->setCurrentWidget(it.key());m_workspaces->setCurrentWidget(m_scriptWorkspace);
            it.key()->setFocus();return;
        }
    QString source;
    bool found=false;
    for (const auto& v:m_panelProject.libraries) {
        const auto obj=v.toObject();
        if(obj.value("name").toString()==name) {source=obj.value("source").toString();found=true;break;}
    }
    if(!found)return;
    auto* editor=newTab(QStringLiteral("Library: ")+name,source);
    m_embeddedEditors.insert(editor,name);
    connect(editor->document(),&QTextDocument::contentsChanged,this,
        [this,view=QPointer<ScriptEditor>(editor),name]{
            if(!view || m_embeddedEditors.value(view)!=name)return;
            if (!setEmbeddedLibrary(name,view->toPlainText())) {
                statusBar()->showMessage(QStringLiteral("Embedded library exceeds project limit"),6000);
            }
            updateTabTitle(view);
        });
    updateTabTitle(editor);
    m_tabs->setCurrentWidget(editor);m_workspaces->setCurrentWidget(m_scriptWorkspace);
    editor->setFocus();
}


// v5.6.62: edit the v1 data-only sequence embedded in the active .wfpanel.
// This is a GUI-thread, offline operation: it never starts a QJSEngine,
// calls device code, or writes to disk unless Save/Export is explicitly used.
void MainWindow::editTestSequence() {
    if(m_panelThread) {
        QMessageBox::information(this,"Panel running","Stop the Panel before editing the test.");return;
    }
    if(!m_panelDesigner)return;
    // The definition belongs to PanelProject. Never scrape/replace JavaScript.
    QStringList libraries;
    for(const auto& entry:m_panelProject.libraries) {
        const QString name=entry.toObject().value(QStringLiteral("name")).toString();
        if(name.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive))libraries.append(name);
    }
    if(libraries.isEmpty()) {
        const QString name=QStringLiteral("TestSteps.mjs");
        const QString source=QStringLiteral(
            "// Test functions: C++ handles sequencing, limits, and timing.\n"
            "export function verifySignal() { return true; }\n"
            "export function restoreOutputs() { return true; }\n");
        if(!setEmbeddedLibrary(name,source)) {
            QMessageBox::warning(this,"Test library","Could not create an embedded test library.");return;
        }
        libraries.append(name);
    }
    QString libraryName=m_panelProject.testLibrary;
    if(!libraries.contains(libraryName)) {
        if(libraries.contains(QStringLiteral("TestStationSteps.mjs")))libraryName=QStringLiteral("TestStationSteps.mjs");
        else libraryName=libraries.first();
    }
    if(libraries.size()>1) {
        bool accepted=false;
        const QString selection=QInputDialog::getItem(this,"Test function library",
            "Choose the .mjs library implementing this test's named handlers:",
            libraries,libraries.indexOf(libraryName),false,&accepted);
        if(!accepted)return;
        libraryName=selection;
    }
    QStringList exports;
    for(const auto& entry:m_panelProject.libraries) {
        const QJsonObject lib=entry.toObject();
        if(lib.value(QStringLiteral("name")).toString()!=libraryName)continue;
        for(const auto& item:ModuleExportCatalog::namedExports(lib.value(QStringLiteral("source")).toString()))
            if(item.item.kind==CompletionCatalog::Kind::Function)exports.append(item.item.name);
    }
    exports.removeDuplicates();exports.sort(Qt::CaseInsensitive);
    QJsonObject original=m_panelProject.testDefinition;
    if(original.isEmpty()) {
        original=QJsonObject{
            {QStringLiteral("schemaVersion"),1},
            {QStringLiteral("id"),QStringLiteral("my-test")},
            {QStringLiteral("title"),QStringLiteral("My Functional Test")},
            {QStringLiteral("config"),QJsonObject{{QStringLiteral("stopOnFailure"),true},
               {QStringLiteral("totalTimeoutMs"),15000}}},
            {QStringLiteral("steps"),QJsonArray{QJsonObject{
               {QStringLiteral("id"),QStringLiteral("check")},
               {QStringLiteral("name"),QStringLiteral("Verify signal")},
               {QStringLiteral("group"),QStringLiteral("main")},
               {QStringLiteral("type"),QStringLiteral("test")},
               {QStringLiteral("mode"),QStringLiteral("once")},
               {QStringLiteral("handler"),exports.isEmpty()?QStringLiteral("verifySignal"):exports.first()}}}}
        };
    }
    TestSequenceEditor editor(original,m_panelProject.runInformation,exports,this,m_panelProject.reportStorage);
    if(editor.exec()!=QDialog::Accepted)return;
    m_panelProject.testDefinition=editor.definition();
    m_panelProject.runInformation=editor.runInformation();
    m_panelProject.reportStorage=editor.reportStorage();
    m_panelDesigner->canvas()->setRunInformationDefinition(m_panelProject.runInformation);
    m_panelProject.testLibrary=libraryName;
    m_panelDirty=true;
    if(m_panelEditor)updateTabTitle(m_panelEditor);
    statusBar()->showMessage(QStringLiteral("Test definition saved in Panel project; save the .wfpanel file to persist"),9000);
}

// v5.6.62: .wfpanel uses JSON strings with escaped newlines so it can keep
// all scripts and libraries in one portable file. Export the original source
// bytes on explicit request for readable inspection and source control.
void MainWindow::exportReadablePanelSources() {
    if(m_panelThread) {
        QMessageBox::information(this,"Panel running","Stop the Panel before exporting source files.");return;
    }
    if(!m_panelDesigner)return;
    const QString path=QFileDialog::getExistingDirectory(this,
        QStringLiteral("Export readable JavaScript and test definitions"),
        QFileInfo(m_panelProject.path).absolutePath());
    if(path.isEmpty())return;
    QMap<QString,QByteArray> files;
    const QString mainName=m_panelProject.scriptName;
    if(mainName.isEmpty() || mainName.contains(QLatin1Char('/')) || mainName.contains(QLatin1Char('\\')) ||
       mainName==QStringLiteral(".") || mainName==QStringLiteral("..")){
        QMessageBox::warning(this,"Invalid script name","Cannot export the Panel script name.");return;
    }
    files.insert(mainName,(m_panelEditor?m_panelEditor->toPlainText():m_panelProject.script).toUtf8());
    for(const auto& entry:m_panelProject.libraries) {
        const auto lib=entry.toObject();
        const QString name=lib.value(QStringLiteral("name")).toString();
        if(name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) ||
           name==QStringLiteral(".") || name==QStringLiteral("..") || files.contains(name)) {
            QMessageBox::warning(this,"Export rejected","Invalid or duplicate embedded script filename.");return;
        }
        QString source=lib.value(QStringLiteral("source")).toString();
        for(auto it=m_embeddedEditors.cbegin();it!=m_embeddedEditors.cend();++it)
            if(it.value()==name && it.key())source=it.key()->toPlainText();
        files.insert(name,source.toUtf8());

    }
    if(!m_panelProject.testDefinition.isEmpty())
        files.insert(m_panelProject.testDefinition.value(QStringLiteral("id")).toString()+QStringLiteral(".wftest.json"),
            QJsonDocument(m_panelProject.testDefinition).toJson(QJsonDocument::Indented));
    QStringList existing;
    for(auto it=files.cbegin();it!=files.cend();++it)
        if(QFileInfo::exists(QDir(path).filePath(it.key())))existing.append(it.key());
    if(!existing.isEmpty() && QMessageBox::question(this,"Overwrite existing files",
        QStringLiteral("The folder already contains %1 matching files. Replace them?").arg(existing.size()),
        QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    for(auto it=files.cbegin();it!=files.cend();++it) {
        QSaveFile file(QDir(path).filePath(it.key()));
        if(!file.open(QIODevice::WriteOnly) || file.write(it.value())!=it.value().size() || !file.commit()) {
            QMessageBox::warning(this,"Source export failed",
                QStringLiteral("Could not export %1: %2").arg(it.key(),file.errorString()));return;
        }
    }
    QMessageBox::information(this,"Readable sources exported",
        QStringLiteral("Exported %1 readable source/definition files to:\n%2\n\n"
                       "The original self-contained .wfpanel is unchanged.").arg(files.size()).arg(path));
}

void MainWindow::manageLibraries() {
    if (m_panelThread) {
        QMessageBox::information(this,"Panel is running","Stop the panel before editing its libraries.");return;
    }
    const QString userDir=ScriptLibraryBridge::userLibraryDirectory();
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Script Libraries — Panel and Shared"));
    dialog.resize(660,460);
    auto* layout=new QVBoxLayout(&dialog);
    auto* help=new QLabel(QStringLiteral(
        "Embedded libraries travel inside this .wfpanel. Shared libraries live in your user data folder "
        "and are available to standalone scripts and panels. Import .js with include(), or .mjs "
        "with modules.load(). New libraries default to ES modules (.mjs); classic include() is "
        "supported for legacy/global helpers. Embedded names take priority. "
        "Library code runs with instrument permissions."),&dialog);
    help->setWordWrap(true);layout->addWidget(help);
    auto* list=new QListWidget(&dialog);
    list->setObjectName(QStringLiteral("panelLibrariesList"));layout->addWidget(list,1);
    auto* controls=new QHBoxLayout;
    auto mk=[&](const QString& text){auto* b=new QPushButton(text,&dialog);controls->addWidget(b);return b;};
    auto* create=mk("New...");auto* add=mk("Import...");auto* edit=mk("Edit");
    auto* remove=mk("Remove");auto* exportFile=mk("Export...");
    auto* folder=mk("Open Shared Folder");
    layout->addLayout(controls);
    auto* footer=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    connect(footer,&QDialogButtonBox::rejected,&dialog,&QDialog::accept);
    layout->addWidget(footer);
    auto refresh=[&]{
        const QString current=list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString();
        list->clear();
        auto* embeddedTitle=new QListWidgetItem(QStringLiteral("— Embedded in panel —"),list);
        embeddedTitle->setFlags(Qt::NoItemFlags);
        for(const auto& v:m_panelProject.libraries){
            const QString name=v.toObject().value("name").toString();
            auto* row=new QListWidgetItem(QStringLiteral("Panel: ")+name,list);
            row->setData(Qt::UserRole,QStringLiteral("P:")+name);
            if(row->data(Qt::UserRole).toString()==current)list->setCurrentItem(row);
        }
        auto* sharedTitle=new QListWidgetItem(QStringLiteral("— Shared user libraries —"),list);
        sharedTitle->setFlags(Qt::NoItemFlags);
        QDir dir(userDir);
        const QStringList files=dir.entryList({"*.js","*.mjs"},QDir::Files,QDir::Name);
        for(const QString& name:files){
            auto* row=new QListWidgetItem(QStringLiteral("Shared: ")+name,list);
            row->setData(Qt::UserRole,QStringLiteral("S:")+name);
            if(row->data(Qt::UserRole).toString()==current)list->setCurrentItem(row);
        }
    };
    auto current=[&]{return list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString();};
    auto writeShared=[&](const QString& name,const QString& source)->bool {
        QSaveFile file(QDir(userDir).filePath(name));
        const auto bytes=source.toUtf8();
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()){
            QMessageBox::warning(&dialog,"Cannot save library",file.errorString());return false;
        }
        return true;
    };
    auto editorFor=[&](const QString& selection){
        if(selection.startsWith(QStringLiteral("P:"))) {
            const QString name=selection.mid(2);
            dialog.accept();showPanelLibraryEditor(name);
        } else if(selection.startsWith(QStringLiteral("S:"))) {
            const QString path=QDir(userDir).filePath(selection.mid(2));
            QFile file(path);
            if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(&dialog,"Open failed",file.errorString());return;}
            ScriptEditor* ed=newTab(QFileInfo(path).fileName(),QString::fromUtf8(file.readAll()));
            m_paths.insert(ed,path);updateTabTitle(ed);
            dialog.accept();m_workspaces->setCurrentWidget(m_scriptWorkspace);m_tabs->setCurrentWidget(ed);
        }
    };
    connect(create,&QPushButton::clicked,&dialog,[&]{
        QDialog wizard(&dialog);
        wizard.setWindowTitle(QStringLiteral("Create Script Library"));
        wizard.setMinimumWidth(510);
        auto* form=new QVBoxLayout(&wizard);
        auto* explanation=new QLabel(QStringLiteral(
            "ES modules (.mjs) are recommended for reusable code: exports are explicit "
            "and private variables cannot collide with other libraries. "
            "Classic .js scripts remain supported for simple helpers and older projects."),&wizard);
        explanation->setWordWrap(true);form->addWidget(explanation);
        auto* nameLabel=new QLabel(QStringLiteral("Library name (without extension):"),&wizard);
        form->addWidget(nameLabel);
        auto* nameEdit=new QLineEdit(QStringLiteral("MyLibrary"),&wizard);
        nameEdit->setObjectName(QStringLiteral("newLibraryName"));form->addWidget(nameEdit);
        auto* typeLabel=new QLabel(QStringLiteral("Script type:"),&wizard);form->addWidget(typeLabel);
        auto* type=new QComboBox(&wizard);type->setObjectName(QStringLiteral("newLibraryType"));
        type->addItem(QStringLiteral("ES Module (.mjs) — Recommended"),QStringLiteral(".mjs"));
        type->addItem(QStringLiteral("Classic Script (.js) — Legacy and simple helpers"),QStringLiteral(".js"));
        form->addWidget(type);
        form->addWidget(new QLabel(QStringLiteral("Store in:"),&wizard));
        auto* location=new QComboBox(&wizard);location->setObjectName(QStringLiteral("newLibraryLocation"));
        location->addItem(QStringLiteral("Embedded in this panel"));
        location->addItem(QStringLiteral("Shared user library"));
        form->addWidget(location);
        auto* preview=new QLabel(&wizard);preview->setWordWrap(true);
        preview->setObjectName(QStringLiteral("newLibraryPreview"));
        form->addWidget(preview);
        const auto updatePreview=[=]{
            const QString base=nameEdit->text().trimmed();
            const QString ext=type->currentData().toString();
            preview->setText(ext==QStringLiteral(".mjs")
                ? QStringLiteral("Creates %1%2. In your panel script: var lib = modules.load(\"%1%2\");")
                      .arg(base,ext)
                : QStringLiteral("Creates %1%2. In your panel script: include(\"%1%2\");")
                      .arg(base,ext));
        };
        connect(nameEdit,&QLineEdit::textChanged,&wizard,updatePreview);
        connect(type,QOverload<int>::of(&QComboBox::currentIndexChanged),&wizard,updatePreview);
        updatePreview();
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&wizard);
        connect(buttons,&QDialogButtonBox::accepted,&wizard,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&wizard,&QDialog::reject);
        form->addWidget(buttons);
        if(wizard.exec()!=QDialog::Accepted)return;
        QString base=nameEdit->text().trimmed();
        const QString ext=type->currentData().toString();
        if(base.endsWith(ext,Qt::CaseInsensitive))base.chop(ext.size());
        // A user who types a conflicting extension should choose the proper
        // type instead; never silently create misleading 'Library.js.mjs'.
        const QString name=base+ext;
        if(base.isEmpty() || base.endsWith(QStringLiteral(".js"),Qt::CaseInsensitive) ||
           base.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive) ||
           !ScriptLibraryBridge::validLibraryName(name) || name.contains(QLatin1Char('/'))){
            QMessageBox::warning(&dialog,"Invalid name",
                "Use a simple filename. Select .js or .mjs using Script type; don't enter both extensions.");return;
        }
        QString identifier=base;
        identifier.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_$]")),QStringLiteral("_"));
        if(identifier.at(0).isDigit())identifier.prepend(QLatin1Char('_'));
        // All new modules include immediately preceding JSDoc because the
        // export index uses it for completion tooltips and F1 help. Keep docs
        // on the export itself, not only in a file-level comment.
        const QString source=ext==QStringLiteral(".mjs")
            ? QStringLiteral(
                "/**\n"
                " * %1: reusable, self-contained ECMAScript module.\n"
                " * Usage: var lib = modules.load(\"%2\");\n"
                " * Then type lib. for autocomplete, or press F1 on lib.square.\n"
                " */\n\n"
                "/**\n"
                " * Calculates the square of a value.\n"
                " * @param {number} value - Number to square.\n"
                " * @returns {number} The squared value.\n"
                " * @example\n"
                " * var lib = modules.load(\"%2\");\n"
                " * print(lib.square(5)); // 25\n"
                " */\n"
                "export function square(value) {\n"
                "    return value * value;\n"
                "}\n\n"
                "// For completion/F1: use named exports and JSDoc immediately\n"
                "// above each export. Avoid exporting functions dynamically.\n")
                .arg(base,name)
            : QStringLiteral(
                "// Classic script: loaded by include(\"%2\").\n"
                "// This exposes a global namespace. New reusable libraries\n"
                "// should preferably use .mjs for export-aware completion/F1.\n"
                "// JSDoc remains useful for readers and future tooling.\n"
                "var %1 = {\n"
                "    /**\n"
                "     * Calculates the square of a value.\n"
                "     * @param {number} value - Number to square.\n"
                "     * @returns {number} The squared value.\n"
                "     */\n"
                "    square: function(value) { return value * value; }\n"
                "};\n")
                .arg(identifier,name);
        if(location->currentIndex()==0) {
            for(const auto& v:m_panelProject.libraries)
                if(v.toObject().value(QStringLiteral("name")).toString()==name){
                    QMessageBox::warning(&dialog,"Duplicate","That embedded library exists.");return;
                }
            if(!setEmbeddedLibrary(name,source)){
                QMessageBox::warning(&dialog,"Too large","Panel library limit exceeded.");return;
            }
            refresh();editorFor(QStringLiteral("P:")+name);
        } else {
            if(QFileInfo::exists(QDir(userDir).filePath(name))){
                QMessageBox::warning(&dialog,"Duplicate","That shared library exists.");return;
            }
            if(writeShared(name,source)){refresh();editorFor(QStringLiteral("S:")+name);}
        }
    });
    connect(add,&QPushButton::clicked,&dialog,[&]{
        const QString file=QFileDialog::getOpenFileName(&dialog,"Import script library",QString(),"Script libraries (*.js *.mjs)");
        if(file.isEmpty())return;
        const QString name=QFileInfo(file).fileName();
        if(!ScriptLibraryBridge::validLibraryName(name)) {QMessageBox::warning(&dialog,"Invalid name","Rename the file to a safe .js/.mjs name.");return;}
        QFile input(file);
        if(!input.open(QIODevice::ReadOnly)||input.size()>1024*1024){QMessageBox::warning(&dialog,"Import failed","Cannot read library or exceeds 1 MiB.");return;}
        const QString source=QString::fromUtf8(input.readAll());
        bool ok=false;
        const QStringList destinations={"Embed in panel", "Copy to shared user folder"};
        const QString dest=QInputDialog::getItem(&dialog,"Import destination","Import into:",destinations,0,false,&ok);
        if(!ok)return;
        if(dest==destinations[0]){
            for(const auto& v:m_panelProject.libraries)if(v.toObject().value("name").toString()==name){QMessageBox::warning(&dialog,"Duplicate","Embedded library exists. Remove it first.");return;}
            if(!setEmbeddedLibrary(name,source)){QMessageBox::warning(&dialog,"Import failed","Embedded library limit exceeded.");return;}
        } else {
            if (QFileInfo::exists(QDir(userDir).filePath(name)) &&
                QMessageBox::question(&dialog,"Replace shared library",
                    QStringLiteral("Overwrite existing shared library %1?").arg(name))!=QMessageBox::Yes)return;
            if (!writeShared(name,source))return;
        }
        refresh();
    });
    connect(edit,&QPushButton::clicked,&dialog,[&]{editorFor(current());});
    connect(list,&QListWidget::itemDoubleClicked,&dialog,[&](QListWidgetItem*){editorFor(current());});
    connect(remove,&QPushButton::clicked,&dialog,[&]{
        const QString selection=current();if(selection.size()<3)return;
        if(QMessageBox::question(&dialog,"Remove library",QStringLiteral("Remove %1?").arg(selection.mid(2)))!=QMessageBox::Yes)return;
        if(selection.startsWith("P:")){
            const QString name=selection.mid(2);QJsonArray retained;
            for(const auto& v:m_panelProject.libraries) if(v.toObject().value("name").toString()!=name)retained.append(v);
            for(auto* ed:m_embeddedEditors.keys())if(m_embeddedEditors.value(ed)==name){
                m_embeddedEditors.remove(ed);m_paths.remove(ed);m_runtimeErrors.remove(ed);
                int idx=m_tabs->indexOf(ed);if(idx>=0)m_tabs->removeTab(idx);ed->deleteLater();
            }
            m_panelProject.libraries=retained;m_panelDirty=true;
        } else if(selection.startsWith("S:")){
            if(!QFile::remove(QDir(userDir).filePath(selection.mid(2)))){
                QMessageBox::warning(&dialog,"Remove failed","Cannot remove shared file.");return;
            }
        }
        refresh();
    });
    connect(exportFile,&QPushButton::clicked,&dialog,[&]{
        const QString selection=current();if(!selection.startsWith("P:"))return;
        const QString name=selection.mid(2);
        QString content;
        for(const auto& v:m_panelProject.libraries)if(v.toObject().value("name").toString()==name)content=v.toObject().value("source").toString();
        const QString path=QFileDialog::getSaveFileName(&dialog,"Export embedded library",QDir(userDir).filePath(name),"Script library (*.js *.mjs)");
        if(path.isEmpty())return;
        QSaveFile f(path);const QByteArray b=content.toUtf8();
        if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.commit())QMessageBox::warning(&dialog,"Export failed",f.errorString());
    });
    connect(folder,&QPushButton::clicked,&dialog,[&]{QDesktopServices::openUrl(QUrl::fromLocalFile(userDir));});
    refresh();dialog.exec();
}

void MainWindow::openPanelEventHandler(const QString& controlId,const QString& eventName,
                                      const QString& handler,bool create) {
    if(m_panelThread) {
        QMessageBox::information(this,QStringLiteral("Panel running"),
            QStringLiteral("Stop the panel before modifying its event handlers."));return;
    }
    showPanelScriptEditor();
    if(!m_panelEditor)return;
    const QRegularExpression declaration(QStringLiteral("\\bfunction\\s+%1\\s*\\(")
        .arg(QRegularExpression::escape(handler)));
    QString source=m_panelEditor->toPlainText();
    QRegularExpressionMatch match=declaration.match(source);
    if(!match.hasMatch() && create) {
        const QString arg=eventName==QStringLiteral("valueChanged")?QStringLiteral("value"):
                          eventName==QStringLiteral("toggled")?QStringLiteral("checked"):QString();
        const QString comment=arg.isEmpty()?QString():
            QStringLiteral(" * @param {%1} %2 - Event value.\n")
                .arg(eventName==QStringLiteral("toggled")?QStringLiteral("boolean"):QStringLiteral("number"),arg);
        const QString templ=QStringLiteral("/**\n * Widget: %1\n * Event: %2\n%3 */\nfunction %4(%5) {\n    // TODO: Handle %2\n}\n")
            .arg(controlId,eventName,comment,handler,arg);
        QTextCursor cursor=m_panelEditor->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(source.endsWith(QLatin1Char('\n'))?QStringLiteral("\n")+templ:QStringLiteral("\n\n")+templ);
        m_panelEditor->setTextCursor(cursor);
        source=m_panelEditor->toPlainText();
        match=declaration.match(source);
    }
    // Classic include() functions remain in their original library source.
    // Navigate to an embedded .js definition rather than inventing a wrapper
    // or attaching an incorrect line number to the panel's main script.
    if(!match.hasMatch()) {
        for(const auto& entry:m_panelProject.libraries) {
            const QJsonObject lib=entry.toObject();
            const QString libraryName=lib.value(QStringLiteral("name")).toString();
            if(!libraryName.endsWith(QStringLiteral(".js"),Qt::CaseInsensitive) ||
               libraryName.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive))continue;
            const auto found=declaration.match(lib.value(QStringLiteral("source")).toString());
            if(!found.hasMatch())continue;
            showPanelLibraryEditor(libraryName);
            for(auto it=m_embeddedEditors.cbegin();it!=m_embeddedEditors.cend();++it) {
                if(it.value()!=libraryName)continue;
                QTextCursor position=it.key()->textCursor();
                position.setPosition(found.capturedStart());
                it.key()->setTextCursor(position);
                it.key()->centerCursor();
                it.key()->setFocus();
                return;
            }
        }
    }
    if(!match.hasMatch()) {
        QMessageBox::information(this,QStringLiteral("Handler not found"),
            QStringLiteral("No global function '%1' exists in %2. Use Create Handler to add it. "
                           "Handlers exported from modules should be wrapped in a named main-script function.")
                .arg(handler,m_panelProject.scriptName));
        return;
    }
    QTextCursor cursor=m_panelEditor->textCursor();
    cursor.setPosition(match.capturedStart());
    m_panelEditor->setTextCursor(cursor);
    m_panelEditor->centerCursor();
    m_panelEditor->setFocus();
}
