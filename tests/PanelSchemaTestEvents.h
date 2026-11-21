#pragma once
// Schema-only native tests intentionally link no QWidget factories. The GUI
// registers built-in metadata through WidgetRegistry before loading Panels;
// reproduce just the factory events used by the embedded laboratory examples.
#include "PanelEventCatalog.h"
inline void registerPanelSchemaTestEvents() {
    PanelEventCatalog::registerType(QStringLiteral("numeric_stepper"),{
        {QStringLiteral("valueChanged"),QStringLiteral("valueChanged(value: number)"),
         QStringLiteral("User edited number or used step buttons."),QStringLiteral("value")}});
    PanelEventCatalog::registerType(QStringLiteral("option_selector"),{
        {QStringLiteral("selectionChanged"),QStringLiteral("selectionChanged(text: string)"),
         QStringLiteral("User selected an option."),QStringLiteral("text")},
        {QStringLiteral("indexChanged"),QStringLiteral("indexChanged(index: number)"),
         QStringLiteral("User selected an option."),QStringLiteral("index")}});
    PanelEventCatalog::registerType(QStringLiteral("switch_button"),{
        {QStringLiteral("valueChanged"),QStringLiteral("valueChanged(state: boolean)"),
         QStringLiteral("Switch state changed."),QStringLiteral("state")},
        {QStringLiteral("toggled"),QStringLiteral("toggled(checked: boolean)"),
         QStringLiteral("Switch toggled by user."),QStringLiteral("checked")}});
}
