#pragma once
#include <QWidget>
#include <QVector>
#include "../DwfRuntime.h"
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTableWidget;

// The Device workspace owns the controls formerly in the global toolbar.
// SDK discovery is non-invasive; capability inspection is a separate action.
class DeviceWorkspace final : public QWidget {
public:
    explicit DeviceWorkspace(QWidget* parent = nullptr);
    QComboBox* runtimeMode() const { return m_mode; }
    QSpinBox* deviceIndex() const { return m_device; }
    QSpinBox* configIndex() const { return m_config; }
    QPushButton* detectButton() const { return m_detect; }
    void setBusy(bool busy);
    void refreshDevices();
private:
    void updateSelection();
    void inspectCapabilities();
    QComboBox* m_mode = nullptr;
    QSpinBox* m_device = nullptr;
    QSpinBox* m_config = nullptr;
    QPushButton* m_detect = nullptr;
    QPushButton* m_inspect = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_model = nullptr;
    QLabel* m_serial = nullptr;
    QLabel* m_configs = nullptr;
    QLabel* m_capabilities = nullptr;
    QTableWidget* m_list = nullptr;
    QVector<DwfRuntime::DeviceInfo> m_devices;
    bool m_busy = false;
};
