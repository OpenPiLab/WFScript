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
    if(!f)return {};
    // Truly universal properties go here, not in 29 widget-specific factories.
    // ID and geometry are always rendered by the Inspector's fixed header.
    WidgetProperty hint{QStringLiteral("toolTip"),QStringLiteral("Tooltip / Help"),
                        QStringLiteral("string"),true};
    hint.defaultValue=QString();
    hint.category=QStringLiteral("Common");
    WidgetProperty enabled{QStringLiteral("runEnabled"),QStringLiteral("Enabled during Run"),
                           QStringLiteral("boolean"),true};
    enabled.defaultValue=true;
    enabled.category=QStringLiteral("Common");
    WidgetProperty visible{QStringLiteral("runVisible"),QStringLiteral("Visible during Run"),
                           QStringLiteral("boolean"),true};
    visible.defaultValue=true;
    visible.category=QStringLiteral("Common");
    QVector<WidgetProperty> result{hint,enabled,visible};
    for(const WidgetProperty& own:f->properties()) {
        // Do not create two fields with the same persistent JSON key.
        bool duplicate=false;
        for(const WidgetProperty& common:result)
            if(common.id==own.id) {duplicate=true;break;}
        if(!duplicate)result.append(own);
    }
    return result;
}
