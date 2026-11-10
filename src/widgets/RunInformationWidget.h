#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QVariantMap>
#include <QHash>

class QFormLayout;
class QScrollArea;
class QLabel;
class QEvent;

// This form is automatically linked to the same project-owned Test Executive
// used by the Panel's RUN buttons. It displays the project's UUT schema and
// only owns temporary operator input, never a second persisted configuration.
class RunInformationWidget final : public QWidget {
    Q_OBJECT
public:
    explicit RunInformationWidget(QWidget* parent=nullptr);
    void setSchema(const QJsonObject& schema);
    QVariantMap values() const;
    void setValues(const QVariantMap& values);
signals:
    void valuesChanged(const QVariantMap& values);
protected:
    bool event(QEvent* e) override;
private:
    QJsonObject m_schema;
    bool m_schemaApplied=false;
    QLabel* m_heading=nullptr;
    QLabel* m_linkLabel=nullptr;
    QLabel* m_hint=nullptr;
    QScrollArea* m_scroll=nullptr;
    QWidget* m_content=nullptr;
    QFormLayout* m_form=nullptr;
    QHash<QString,QWidget*> m_inputs;
    void emitValues();
    void applyTheme();
};
