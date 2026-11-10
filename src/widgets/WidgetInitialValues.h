#pragma once
#include <QVariantMap>
#include <QVector>
struct PanelPage;
// Build initial JavaScript-visible values for every Page using each widget's
// factory contract. Never requires QWidget instances or snapshots live state.
namespace WidgetInitialValues {
QVariantMap fromPages(const QVector<PanelPage>& pages);
}
