#include "WidgetRegistry.h"
#include "BuiltinWidgets.h"
#include <utility>

WidgetRegistry& WidgetRegistry::instance() {
    static WidgetRegistry registry;
    return registry;
}
WidgetRegistry::WidgetRegistry() { registerBuiltinWidgets(*this); }

bool WidgetRegistry::registerFactory(std::unique_ptr<WidgetFactory> factory) {
    if(!factory || factory->typeId().isEmpty() || m_factories.contains(factory->typeId()))
        return false;
    const QString type=factory->typeId();
    const auto eventDefinitions=factory->events();
    // Keep the existing source-aware script event catalog authoritative for
    // designer validation and PanelRuntime, including future widget types.
    PanelEventCatalog::registerType(type,eventDefinitions);
    m_order.append(type);
    m_factories.insert(type,std::shared_ptr<WidgetFactory>(std::move(factory)));
    return true;
}
const WidgetFactory* WidgetRegistry::factory(const QString& typeId) const {
    const auto it=m_factories.constFind(typeId);
    return it==m_factories.cend() ? nullptr : it.value().get();
}
QVector<WidgetProperty> WidgetRegistry::properties(const QString& typeId) const {
    const auto* f=factory(typeId);
    return f ? f->properties() : QVector<WidgetProperty>{};
}
