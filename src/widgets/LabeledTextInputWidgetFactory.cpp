#include "LabeledTextInputWidgetFactory.h"
#include "LabeledTextInputWidget.h"
#include "WidgetRegistry.h"
#include <QJsonValue>
#include <QSignalBlocker>
#include <memory>

namespace {
WidgetProperty property(const QString& id, const QString& label,
                        const QString& kind, const QVariant& initial,
                        const QString& category = QStringLiteral("Input")) {
    WidgetProperty p;
    p.id = id;
    p.label = label;
    p.kind = kind;
    p.defaultValue = initial;
    p.category = category;
    return p;
}
class TextInputFactory final : public WidgetFactory {
public:
    explicit TextInputFactory(bool ipv4) : m_ipv4(ipv4) {}
    QString typeId() const override {
        return m_ipv4 ? QStringLiteral("ip_address_input")
                      : QStringLiteral("text_input");
    }
    QString displayName() const override {
        return m_ipv4 ? QStringLiteral("IP Address Input (IPv4)")
                      : QStringLiteral("Text Input");
    }
    PanelControl defaultDefinition(const QString& id) const override {
        PanelControl d = WidgetFactory::defaultDefinition(id);
        d.geometry.setSize(QSize(315, 88));
        d.text = m_ipv4 ? QStringLiteral("IP ADDRESS") : QStringLiteral("TEXT INPUT");
        d.properties.insert(QStringLiteral("defaultText"),
                            m_ipv4 ? QStringLiteral("192.168.1.100") : QString());
        d.properties.insert(QStringLiteral("placeholder"),
                            m_ipv4 ? QStringLiteral("192.168.1.100")
                                   : QStringLiteral("Enter text..."));
        d.properties.insert(QStringLiteral("labelPosition"), QStringLiteral("Top"));
        d.properties.insert(QStringLiteral("accent"), QStringLiteral("cyan"));
        d.properties.insert(QStringLiteral("maxLength"), m_ipv4 ? 15 : 256);
        d.properties.insert(QStringLiteral("required"), false);
        d.properties.insert(QStringLiteral("readOnly"), false);
        return d;
    }
    QVariant initialRuntimeValue(const PanelControl& d) const override {
        return d.properties.value(QStringLiteral("defaultText")).toString();
    }
    QVector<WidgetProperty> properties() const override {
        auto position = property(QStringLiteral("labelPosition"),
                                 QStringLiteral("Label position"),
                                 QStringLiteral("choice"), QStringLiteral("Top"),
                                 QStringLiteral("Appearance"));
        position.choices = {QStringLiteral("Top"), QStringLiteral("Bottom"),
                            QStringLiteral("Left"), QStringLiteral("Right")};
        auto accent = property(QStringLiteral("accent"),
                               QStringLiteral("Accent colour"),
                               QStringLiteral("choice"), QStringLiteral("cyan"),
                               QStringLiteral("Appearance"));
        accent.choices = {QStringLiteral("cyan"), QStringLiteral("green"),
                          QStringLiteral("amber"), QStringLiteral("blue"),
                          QStringLiteral("magenta"), QStringLiteral("red"),
                          QStringLiteral("white")};
        auto maxLength = property(QStringLiteral("maxLength"),
                                  QStringLiteral("Maximum characters"),
                                  QStringLiteral("integer"), m_ipv4 ? 15 : 256);
        maxLength.minimum = 1;
        maxLength.maximum = m_ipv4 ? 15 : 4096;
        return {
            property(QStringLiteral("text"), QStringLiteral("Label"),
                     QStringLiteral("string"), QString()),
            property(QStringLiteral("defaultText"), QStringLiteral("Initial text"),
                     QStringLiteral("string"), QString()),
            property(QStringLiteral("placeholder"), QStringLiteral("Placeholder text"),
                     QStringLiteral("string"), QString()),
            maxLength,
            property(QStringLiteral("required"), QStringLiteral("Required (not blank)"),
                     QStringLiteral("boolean"), false),
            property(QStringLiteral("readOnly"), QStringLiteral("Read only during Run"),
                     QStringLiteral("boolean"), false),
            position, accent
        };
    }
    QVector<PanelEventDefinition> events() const override {
        return {
            {QStringLiteral("valueChanged"),
             QStringLiteral("valueChanged(text: string)"),
             QStringLiteral("User edited an accepted text value. For IP input, only complete valid IPv4 addresses are emitted."),
             QStringLiteral("text")},
            {QStringLiteral("editingFinished"),
             QStringLiteral("editingFinished(text: string)"),
             QStringLiteral("Input accepted after Enter or loss of focus."),
             QStringLiteral("text")},
            {QStringLiteral("validationFailed"),
             QStringLiteral("validationFailed(text: string)"),
             QStringLiteral("Input rejected after Enter or loss of focus; ui.get() still holds the last accepted value."),
             QStringLiteral("text")}
        };
    }
    QWidget* create(const PanelControl& d, QWidget* parent, EventSink sink) const override {
        auto* input = new LabeledTextInputWidget(m_ipv4, parent);
        applyDefinition(input, d);
        QObject::connect(input, &LabeledTextInputWidget::userValueChanged,
                         input, [sink](const QString& text) { sink(QStringLiteral("valueChanged"), text); });
        QObject::connect(input, &LabeledTextInputWidget::userEditingFinished,
                         input, [sink](const QString& text) { sink(QStringLiteral("editingFinished"), text); });
        QObject::connect(input, &LabeledTextInputWidget::userValidationFailed,
                         input, [sink](const QString& text) { sink(QStringLiteral("validationFailed"), text); });
        return input;
    }
    void applyDefinition(QWidget* widget, const PanelControl& d) const override {
        auto* input = qobject_cast<LabeledTextInputWidget*>(widget);
        if (!input) return;
        const auto& p = d.properties;
        input->configure(d.text,
                         p.value(QStringLiteral("defaultText")).toString(),
                         p.value(QStringLiteral("placeholder")).toString(),
                         p.value(QStringLiteral("labelPosition")).toString(QStringLiteral("Top")),
                         p.value(QStringLiteral("accent")).toString(QStringLiteral("cyan")),
                         p.value(QStringLiteral("maxLength")).toInt(m_ipv4 ? 15 : 256),
                         p.value(QStringLiteral("required")).toBool(false),
                         p.value(QStringLiteral("readOnly")).toBool(false));
    }
    void readDefinition(QWidget*, PanelControl&) const override {
        // Operator input is transient, like all other ui.set() changes. Never
        // modify a saved initialText just because an operator typed something.
    }
    void applyRuntimeValue(QWidget* widget, const PanelControl&, const QVariant& value) const override {
        if (auto* input = qobject_cast<LabeledTextInputWidget*>(widget))
            if (value.metaType().id() == QMetaType::QString)
                input->setRuntimeText(value.toString());
    }
private:
    bool m_ipv4;
};
}
void registerLabeledTextInputWidgets(WidgetRegistry& registry) {
    registry.registerFactory(std::make_unique<TextInputFactory>(false));
    registry.registerFactory(std::make_unique<TextInputFactory>(true));
}
