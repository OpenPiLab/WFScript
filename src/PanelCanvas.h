#pragma once

#include "plot/PlotCommand.h"
#include "widgets/WidgetDefinition.h"
#include <QWidget>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>
#include <QPoint>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QColor>

class QRubberBand;
class QUndoStack;
class QContextMenuEvent;
class QKeyEvent;
class PanelCanvasHistoryCommand;
class QFrame;
class QMouseEvent;
class QPaintEvent;
class QDropEvent;
class QDragEnterEvent;

// Native Qt Widgets are used in BOTH design and run mode.
// This intentionally uses no QGraphicsProxyWidget, web content, or Qt Designer code.
class PanelCanvas final : public QWidget {
    Q_OBJECT
public:
    explicit PanelCanvas(QWidget* parent = nullptr);
    // The persisted document bounds and this QWidget's actual pixel bounds
    // are identical. The surrounding scroll-area viewport is workspace only.
    QSize documentSize() const { return m_documentSize; }
    QSize minimumContentSize() const;
    bool setDocumentSize(const QSize& requested);
    QSize sizeHint() const override { return m_documentSize; }
    bool designMode() const { return m_designMode; }
    void setDesignMode(bool design);
    void setRunInformationDefinition(const QJsonObject& schema);
    void setPageAppearance(const QColor& background, bool light, bool showGrid);
    QColor pageBackground() const { return m_background; }
    bool addControl(const PanelControl& value);
    void clearControls();
    QJsonArray toJson() const;
    bool fromJson(const QJsonArray& json, QString* error);
    static bool validateJson(const QJsonArray& json, QString* error = nullptr);
    // Page switching keeps each QWidget alive: inactive Pages are hidden, not serialized/rebuilt.
    // The model supplies controls only the first time a Page is visited.
    bool activatePage(const QString& id, const QJsonArray& controls,
                      const QSize& size, QString* error = nullptr);
    void resetPages();              // new/open project or document-wide undo/redo
    void forgetPage(const QString& id); // remove the widgets of a deleted Page
    void restoreDocumentValues(const QJsonArray& controls); // run -> design
    QString activePageId() const { return m_activePageId; }
    PanelControl selectedControl() const;
    QVector<PanelControl> selectedControls() const;
    int selectionCount() const { return m_selection.size(); }
    void selectAllControls();
    // Select widgets intersecting the canvas rectangle; optionally preserve the existing set.
    void selectControlsInRect(const QRect& rectangle, bool additive = false);
    void selectControlById(const QString& id, bool additive = false);
    QUndoStack* undoStack() const { return m_undoStack; }
    void resetDesignHistory();
    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }
    void copySelected();
    void cutSelected();
    void pasteControls();
    void duplicateSelected();
    void nudgeSelected(int dx, int dy);
    void alignSelected(const QString& operation);
    void bringSelectedToFront();
    void sendSelectedToBack();
    bool updateSelected(const PanelControl& def);
    bool setSelectedEventBinding(const QString& eventName,const QString& handler,bool enabled);
    void deleteSelected();
    void applyValue(const QString& id, const QVariant& value);
    void applyPlotCommand(const PlotCommand& command);
    QVariantMap initialValues() const;
    QVariantMap runInformationInputValues() const;
    QString uniqueId(const QString& type) const;
    // IDs owned by other Pages are reserved even when those widgets are hidden.
    void setReservedIds(const QSet<QString>& ids) { m_reservedIds=ids; }
    static QStringList supportedTypes();
signals:
    void selectedChanged(bool hasSelection);
    void documentChanged();
    void controlEvent(const QString& id, const QString& eventName, const QVariant& value);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
private:
    struct Item { PanelControl def; QWidget* widget = nullptr; };
    struct PageWidgets {
        QVector<Item> items;
        QSize size;
    };
    QVector<Item> m_items; // active Page only, for existing designer/editor operations
    QHash<QString, PageWidgets> m_inactivePages; // hidden native widgets are still alive
    QString m_activePageId;
    Item* findAnyId(const QString& id);
    QSet<QString> m_reservedIds;
    QWidget* m_selected = nullptr;
    QSet<QWidget*> m_selection;
    QHash<QWidget*, QRubberBand*> m_secondaryOutlines;
    QUndoStack* m_undoStack = nullptr;
    bool m_restoringHistory = false;
    bool m_batchEditing = false;
    bool m_snapToGrid = false;
    int m_pasteOffset = 0;
    struct Snapshot {
        QJsonArray controls;
        QSize documentSize;
        QStringList selectedIds;
    };
    Snapshot m_lastSnapshot;
    friend class PanelCanvasHistoryCommand;
    Snapshot snapshot() const;
    void restoreSnapshot(const Snapshot& state);
    void recordChange(const QString& description);
    void updateHistorySelection();
    void cloneControls(const QVector<PanelControl>& source);
    static bool parseControls(const QJsonArray& json, QVector<PanelControl>& definitions, QString* error);
    int snap(int coordinate) const;
    void showDesignerContextMenu(const QPoint& globalPosition);
    QRubberBand* m_outline = nullptr;
    QRubberBand* m_marquee = nullptr;
    bool m_marqueeSelecting = false;
    QPoint m_marqueeOrigin;
    QSet<QWidget*> m_marqueeBase;
    QFrame* m_resize = nullptr;
    QSize m_documentSize{900, 550};
    bool m_designMode = true;
    QJsonObject m_runInformationDefinition;
    QVariantMap m_runInformationInput; // pending UUT values, one shared GUI model across Pages
    void synchronizeRunInformationWidgets(const QString& originId = {});
    QColor m_background{240,244,249};
    bool m_showDesignGrid=true;
    bool m_dragging = false;
    bool m_resizing = false;
    QPoint m_dragOrigin;
    QRect m_originalGeometry;
    QHash<QWidget*, QRect> m_dragOriginals;
    QWidget* createWidget(const PanelControl& def);
    Item* findItem(QWidget* widget);
    const Item* findItem(QWidget* widget) const;
    Item* findId(const QString& id);
    void selectWidget(QWidget* widget, bool additive = false);
    void syncSelection();
    void selectRectWithBase(const QRect& rectangle, const QSet<QWidget*>& base);
    void growDocumentToFit(const QRect& widgetGeometry);
    static PanelControl readItem(const Item& item);
};
