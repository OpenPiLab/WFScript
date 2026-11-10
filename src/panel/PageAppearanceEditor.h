#pragma once
#include "../PanelProject.h"
#include <QWidget>

class QComboBox;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;
class QSpinBox;

// Context-sensitive project/Page inspector, shown when no widget is selected.
// Project-wide appearance and lifecycle are distinct from per-Page dimensions
// and background. All edits flow through PanelDesigner / MainWindow owners.
class PageAppearanceEditor final : public QWidget {
    Q_OBJECT
public:
    explicit PageAppearanceEditor(QWidget* parent=nullptr);
    void setAppearance(const PanelAppearance& appearance, const QString& pageBackground,
                       const QString& pageName);
    void setPageSize(const QSize& size, const QSize& minimumContent);
    void setLifecycle(const PanelLifecycleSettings& lifecycle);
    void setEditable(bool editable);
    void showSizeError(const QString& message);
signals:
    void edited(const QString& mode, const QString& panelBackground,
                const QString& pageBackground, bool showGrid);
    void sizeRequested(const QSize& size);
    void fillWorkspaceRequested();
    void lifecycleEdited(bool initializeEnabled, bool shutdownEnabled);
    void lifecycleFunctionRequested(const QString& name);
private:
    QComboBox* m_theme=nullptr;
    QLineEdit* m_panelColor=nullptr;
    QLineEdit* m_pageColor=nullptr;
    QCheckBox* m_grid=nullptr;
    QLabel* m_name=nullptr;
    QSpinBox* m_width=nullptr;
    QSpinBox* m_height=nullptr;
    QPushButton* m_fillWorkspace=nullptr;
    QLabel* m_sizeHint=nullptr;
    QLabel* m_sizeError=nullptr;
    QCheckBox* m_initialize=nullptr;
    QCheckBox* m_shutdown=nullptr;
    QPushButton* m_editInitialize=nullptr;
    QPushButton* m_editShutdown=nullptr;
    bool m_loading=false;
    void commit();
};
