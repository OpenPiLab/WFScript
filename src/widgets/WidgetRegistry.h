#pragma once
#include "WidgetFactory.h"
#include <QHash>
#include <QStringList>
#include <memory>

// Single process-level catalog, populated before the Panel runtime starts.
// This first release uses linked factories, not dynamically loaded DLLs.
class WidgetRegistry final {
public:
    static WidgetRegistry& instance();
    bool registerFactory(std::unique_ptr<WidgetFactory> factory);
    const WidgetFactory* factory(const QString& typeId) const;
    QStringList typeIds() const { return m_order; }
    QVector<WidgetProperty> properties(const QString& typeId) const;
private:
    WidgetRegistry();
    QHash<QString,std::shared_ptr<WidgetFactory>> m_factories;
    QStringList m_order;
};
