#include "WidgetInitialValues.h"
#include "WidgetDocumentCodec.h"
#include "WidgetRegistry.h"
#include "../PanelProject.h"

namespace WidgetInitialValues {
QVariantMap fromPages(const QVector<PanelPage>& pages) {
    QVariantMap initial;
    for(const PanelPage& page:pages) {
        for(const auto& value:page.controls) {
            if(!value.isObject())continue;
            PanelControl d;
            if(!WidgetDocumentCodec::decode(value.toObject(),&d,nullptr))continue;
            if(const auto* factory=WidgetRegistry::instance().factory(d.type))
                initial.insert(d.id,factory->initialRuntimeValue(d));
        }
    }
    return initial;
}
}
