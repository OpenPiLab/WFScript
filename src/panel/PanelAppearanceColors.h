#pragma once
// GUI-only conversion of model appearance data to Qt palette colours.
// PanelProject / .wfpanel serialization remains independent of QtGui.
#include "PanelAppearance.h"
#include <QColor>
#include <QPalette>
inline bool panelThemeIsLight(const PanelAppearance& a,const QPalette& applicationPalette) {
    if(a.themeMode==QLatin1String("light"))return true;
    if(a.themeMode==QLatin1String("dark"))return false;
    return applicationPalette.color(QPalette::Window).lightness() >= 128;
}
inline QColor panelCanvasColor(const PanelAppearance& a,const QPalette& applicationPalette,
                              const QString& pageOverride={}) {
    if(!pageOverride.isEmpty())return QColor(pageOverride);
    if(!a.background.isEmpty())return QColor(a.background);
    return panelThemeIsLight(a,applicationPalette) ? QColor(240,244,249) : QColor(15,24,36);
}
