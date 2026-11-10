#pragma once
#include "ScriptValueConversion.h"
#include <QObject>
#include <QJSValue>
#include <QJSEngine>
#include <QVariantMap>
#include <QSet>
#include <utility>

// Owned and invoked entirely on the Panel QJSEngine worker thread. GUI/native
// updates cross thread boundaries as QVariant, never as QJSValue.
class PanelUiBridge final : public QObject {
    Q_OBJECT
public:
    explicit PanelUiBridge(QJSEngine* engine,QVariantMap values,
                           QSet<QString> readOnlyIds={},QObject* parent=nullptr)
        : QObject(parent),m_engine(engine),m_values(std::move(values)),
          m_readOnlyIds(std::move(readOnlyIds)) {}
    // Return native JS Arrays/Objects, not a generic QVariant wrapper.
    Q_INVOKABLE QJSValue get(const QString& id) const {
        return ScriptValueConversion::toJavaScript(m_engine,m_values.value(id));
    }
    Q_INVOKABLE bool set(const QString& id,const QJSValue& scriptValue) {
        if(!m_values.contains(id) || m_readOnlyIds.contains(id))return false;
        // Convert on worker thread before queued GUI update.
        const QVariant value=scriptValue.toVariant(QJSValue::ConvertJSObjects);
        m_values.insert(id,value);
        emit updateRequested(id,value);
        return true;
    }
    bool setNative(const QString& id,const QVariant& value) {
        if(!m_values.contains(id))return false;
        m_values.insert(id,value);
        emit updateRequested(id,value);
        return true;
    }
    void receiveEvent(const QString& id,const QVariant& value) { m_values.insert(id,value); }
signals:
    void updateRequested(const QString& id,const QVariant& value);
private:
    QJSEngine* m_engine=nullptr; // borrowed; must outlive this bridge
    QVariantMap m_values;
    QSet<QString> m_readOnlyIds;
};
