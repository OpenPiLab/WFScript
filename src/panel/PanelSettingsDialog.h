#pragma once
#include "../PanelProject.h"
#include <QString>
class QWidget;

struct PanelSettingsResult {
    bool accepted=false;
    PanelLifecycleSettings lifecycle;
    QString navigateTo;
};

// Dialog stays independent of MainWindow, editor tabs and runtime state.
PanelSettingsResult editPanelSettings(QWidget* parent,
                                      const PanelLifecycleSettings& current);
