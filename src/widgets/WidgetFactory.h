#pragma once
#include "WidgetDefinition.h"
#include "../PanelEventCatalog.h"
#include <QVariant>
#include <QVector>
#include <QStringList>
#include <functional>

class QWidget;
struct PlotCommand;

// Represents a designer-visible property. Property metadata is descriptive in
// this foundation release; future versions can generate inspector editors.
struct WidgetProperty {
    QString id;              // stable, nonlocalized property key
    QString label;           // UI name
    QString kind;            // string, double, boolean, rect
    bool writable = true;
    double minimum = -1.0e9;   // numeric editor constraint
    double maximum = 1.0e9;
    int decimals = 3;
    // Keep WidgetProperty an aggregate so existing ordered initializer lists
    // remain source-compatible. Explicit default member initializers preserve
    // their original empty/null values without -Wmissing-field-initializers.
    QStringList choices{};        // kind = choice; stored as a string
    QVariant defaultValue{};      // used when an older project omits this property
    QString category{};          // optional inspector category (e.g. X Axis); does not affect disk format
};

// Native-widget contract. The factory never owns QWidget instances; Qt parent
// ownership controls widget lifetimes. All methods run on the GUI thread.
class WidgetFactory {
public:
    using EventSink = std::function<void(const QString&, const QVariant&)>;
    virtual ~WidgetFactory() = default;
    virtual QString typeId() const = 0;  // stable serialization identifier
    virtual QString displayName() const = 0;
    virtual QVector<WidgetProperty> properties() const = 0;
    // Factory-controlled new-item defaults. Toolbox and drag/drop use this
    // instead of branching on widget type in their own UI code.
    virtual PanelControl defaultDefinition(const QString& id) const {
        PanelControl c;
        c.id=id; c.type=typeId(); c.text=typeId();
        c.geometry=QRect(0,0,150,38);
        return c;
    }
    virtual QVector<PanelEventDefinition> events() const { return {}; }
    // Initial script-visible ui.get() value. Usually a number, but an option
    // selector owns a string and switch-like widgets own booleans.
    virtual QVariant initialRuntimeValue(const PanelControl& definition) const {
        return definition.value;
    }
    virtual QWidget* create(const PanelControl& definition, QWidget* parent,
                            EventSink onUserEvent) const = 0;
    // Apply design-time properties/restore saved defaults; never replace widget.
    virtual void applyDefinition(QWidget* widget, const PanelControl& definition) const = 0;
    // Read from the live object into a copy; snapshots do not destroy objects.
    virtual void readDefinition(QWidget* widget, PanelControl& definition) const = 0;
    // ui.set(): runtime update, including on hidden Pages, never rebuild widget.
    virtual void applyRuntimeValue(QWidget* widget, const PanelControl& definition,
                                   const QVariant& value) const = 0;
    // Explicit plot updates are worker-to-GUI messages, never a Scope broadcast.
    virtual void deliverPlotCommand(QWidget*, const PlotCommand&) const {}
};
