#pragma once
#include "PanelCanvas.h"
#include "PanelProject.h" // PanelLifecycleSettings is used in this public interface.
#include "panel/PanelAppearance.h"
#include <QWidget>
#include <QVariant>

class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTabWidget;
class QTabBar;
class QPoint;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QToolButton;
class QLabel;
class QSplitter;
class QScrollArea;
class QUndoStack;
class WidgetPropertyInspector;
class PageAppearanceEditor;
class QDialog;

class PanelDesigner final : public QWidget {
    Q_OBJECT
public:
    explicit PanelDesigner(QWidget* parent=nullptr);
    PanelCanvas* canvas() const { return m_canvas; }
    QSize panelSize() const { return m_canvas->documentSize(); }
    bool setPanelSize(const QSize& size);
    QScrollArea* canvasScrollArea() const { return m_scrollArea; }
    void setRunning(bool running);
    bool running() const { return m_running; }
    bool focusMode() const { return m_focusMode; }
    bool toolboxShown() const;
    bool propertiesShown() const;
    void setFocusMode(bool focused);
    void setToolboxShown(bool shown);
    void setPropertiesShown(bool shown);
    void resetUiLayout();
    void setPageTabs(const QVector<QPair<QString,QString>>& pages,const QString& activeId);
    QString selectedPageId() const;
    void setPageHistoryEnabled(bool canUndo,bool canRedo);
    void setPageAppearance(const PanelAppearance& appearance, const QString& pageBackground,
                           const QString& pageName);
    void setPanelLifecycle(const PanelLifecycleSettings& lifecycle);
signals:
    void pageAppearanceEdited(const QString& themeMode,const QString& panelBackground,
                              const QString& pageBackground,bool showDesignGrid);
    void panelLifecycleEdited(bool initializeEnabled,bool shutdownEnabled);
    void panelLifecycleFunctionRequested(const QString& name);
    void newRequested();
    void loadRequested();
    void exampleRequested(const QString& resourcePath);
    void saveRequested();
    void runRequested();
    void stopRequested();
    void editScriptRequested();
    void librariesRequested();
    void testSequenceEditorRequested();
    void pageSelected(const QString& id);
    void addPageRequested();
    void renamePageRequested();
    void deletePageRequested();
    void undoPageRequested();
    void redoPageRequested();
    void eventHandlerRequested(const QString& controlId,const QString& eventName,
                               const QString& handler,bool create);
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    void applyPageAppearance();
    PanelCanvas* m_canvas=nullptr;
    QTabBar* m_pageTabs=nullptr;
    // The last QTabBar tab is a non-document "+" action.
    int m_lastRealPageTab=0;
    bool m_canUndoPage=false;
    bool m_canRedoPage=false;
    void showPageContextMenu(const QPoint& localPosition);
    QSplitter* m_splitter=nullptr;
    QScrollArea* m_scrollArea=nullptr;
    QWidget* m_toolboxPane=nullptr;
    QWidget* m_propertiesPane=nullptr;
    QPushButton* m_toolboxToggle=nullptr;
    QPushButton* m_propertiesToggle=nullptr;
    QPushButton* m_focusToggle=nullptr;
    QPushButton* m_designToolsToggle=nullptr;
    QDialog* m_designToolsPalette=nullptr;
    bool m_focusMode=false;
    bool m_beforeFocusToolbox=true;
    bool m_beforeFocusProperties=true;
    QTreeWidget* m_toolbox=nullptr;
    QLineEdit* m_toolboxSearch=nullptr;
    WidgetPropertyInspector* m_propertyEditor=nullptr;
    PageAppearanceEditor* m_pageAppearanceEditor=nullptr;
    QLabel* m_propertyTitle=nullptr;
    QLabel* m_widgetInfo=nullptr;
    PanelAppearance m_appearance;
    QString m_pageBackground;
    QString m_pageName=QStringLiteral("Main");
    QPushButton *m_new=nullptr,*m_open=nullptr,*m_save=nullptr,*m_sequenceEditor=nullptr;
    QToolButton* m_examples=nullptr;
    QPushButton *m_run=nullptr,*m_stop=nullptr,*m_add=nullptr,*m_delete=nullptr;
    QPushButton *m_undo=nullptr,*m_redo=nullptr,*m_copy=nullptr,*m_paste=nullptr,*m_duplicate=nullptr;
    QPushButton *m_align=nullptr,*m_arrange=nullptr,*m_snap=nullptr;
    QLabel* m_propertiesStatus=nullptr;
    QTabWidget* m_inspectorTabs=nullptr;
    QListWidget* m_eventList=nullptr;
    QLineEdit* m_eventHandler=nullptr;
    QCheckBox* m_eventEnabled=nullptr;
    QLabel* m_eventHelp=nullptr;
    QLabel* m_eventNotice=nullptr;
    QPushButton* m_eventCreate=nullptr;
    QPushButton* m_eventAssign=nullptr;
    QPushButton* m_eventChoose=nullptr;
    QPushButton* m_eventGo=nullptr;
    QPushButton* m_eventRemove=nullptr;
    QString m_panelScriptSource;
    bool m_refreshingEvents=false;
    bool m_loadingProperties=false;
    bool m_running=false;
    void loadProperties();
    void applyProperties();
    void scheduleProperties();
    void addCurrent();
    void refreshPageDimensions();
    void applyRequestedPageSize(const QSize& size);
    void updateSelectionActions();
    void refreshEvents();
    void selectedEventChanged();
    void assignEvent(bool remove=false);
    void createOrGoEvent(bool create);
    void chooseEventHandler();
public:
    void setPanelScriptSource(const QString& source);
};
