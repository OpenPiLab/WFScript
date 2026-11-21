#pragma once
#include "ScriptValueConversion.h"
#include "PanelDataSchema.h"
#include "widgets/TextInputValidation.h"
#include "widgets/ExperimentTableData.h"
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <cmath>
#include <QObject>
#include <QJSValue>
#include <QJSEngine>
#include <QVariantMap>
#include <QSet>
#include <QMetaType>
#include <QMultiHash>
#include <utility>

// Owned and invoked entirely on the Panel QJSEngine worker thread. GUI/native
// updates cross thread boundaries as QVariant, never as QJSValue.
class PanelUiBridge final : public QObject {
    Q_OBJECT
public:
    explicit PanelUiBridge(QJSEngine* engine,QVariantMap values,
                           QSet<QString> readOnlyIds={},
                           const QJsonArray& definitions={}, const QJsonArray& controls={},
                           QObject* parent=nullptr)
        : QObject(parent),m_engine(engine),m_values(std::move(values)),
          m_readOnlyIds(std::move(readOnlyIds)) {
        for(const QJsonValue& entry:definitions) {
            const auto d=entry.toObject();
            const QString name=d.value(QStringLiteral("name")).toString();
            m_dataTypes.insert(name,d.value(QStringLiteral("type")).toString());
            m_dataValues.insert(name,PanelDataSchema::initialValue(d));
        }
        for(const QJsonValue& entry:controls) {
            const auto obj=entry.toObject();
            const QString id=obj.value(QStringLiteral("id")).toString();
            const QString widgetType=obj.value(QStringLiteral("type")).toString();
            if(widgetType==QStringLiteral("text_input") ||
               widgetType==QStringLiteral("ip_address_input")) {
                const auto props=obj.value(QStringLiteral("properties")).toObject();
                m_textInputRules.insert(id,TextInputRule{
                    widgetType==QStringLiteral("ip_address_input"),
                    props.value(QStringLiteral("maxLength")).toInt(
                        widgetType==QStringLiteral("ip_address_input") ? 15 : 256)});
            }
            if(widgetType==QStringLiteral("experiment_table")) {
                const QJsonObject props=obj.value(QStringLiteral("properties")).toObject();
                ExperimentTableState table;
                table.columns=props.value(QStringLiteral("columns")).toString().split(QLatin1Char('|')).size();
                table.maxRows=qBound(1,props.value(QStringLiteral("maxRows")).toInt(ExperimentTableLimits::DefaultRows),
                                     ExperimentTableLimits::MaxRows);
                const QVariantList initial=m_values.value(id).toList();
                ExperimentTableData::Mutation load;
                if(ExperimentTableData::decode(QVariant(initial),table.columns,table.maxRows,&load))
                    ExperimentTableData::apply(&table.rows,table.maxRows,table.columns,load);
                m_experimentTables.insert(id,std::move(table));
                // Store data only in the dedicated deque. Do not keep a second
                // large copy in m_values on each appended packet.
                m_values.insert(id,QVariantList{});
            }
            const QString name=obj.value(QStringLiteral("bindings")).toObject()
                    .value(QStringLiteral("value")).toString();
            if(!name.isEmpty() && m_dataValues.contains(name)) {
                m_widgetBinding.insert(id,name);
                m_widgetTypes.insert(id,obj.value(QStringLiteral("type")).toString());
                m_dataTargets.insert(name,id);
                m_values.insert(id,m_dataValues.value(name));
            }
        }
    }
    // Return native JS Arrays/Objects, not a generic QVariant wrapper.
    Q_INVOKABLE QJSValue get(const QString& id) const {
        auto table=m_experimentTables.constFind(id);
        if(table!=m_experimentTables.cend())
            return ScriptValueConversion::toJavaScript(m_engine,ExperimentTableData::snapshot(table->rows));
        return ScriptValueConversion::toJavaScript(m_engine,m_values.value(id));
    }
    Q_INVOKABLE int tableRowCount(const QString& id) const {
        const auto table=m_experimentTables.constFind(id);
        return table==m_experimentTables.cend()?-1:int(table->rows.size());
    }
    Q_INVOKABLE QJSValue tableSlice(const QString& id,int start,int count) const {
        const auto table=m_experimentTables.constFind(id);
        if(table==m_experimentTables.cend() || start<0 || count<0 || count>2048)
            return QJSValue(QJSValue::NullValue);
        return ScriptValueConversion::toJavaScript(m_engine,
            ExperimentTableData::snapshot(table->rows,start,count));
    }
    Q_INVOKABLE QJSValue tableSelection(const QString& id) const {
        if(!m_experimentTables.contains(id))return QJSValue(QJSValue::NullValue);
        return ScriptValueConversion::toJavaScript(m_engine,m_tableSelection.value(id,
            QVariantMap{{QStringLiteral("row"),-1},{QStringLiteral("column"),-1},
                        {QStringLiteral("rows"),QVariantList{}},{QStringLiteral("selectedCells"),0}}));
    }
    void receiveTableSelection(const QString& id,const QVariant& selection) {
        if(m_experimentTables.contains(id))m_tableSelection.insert(id,selection.toMap());
    }
    Q_INVOKABLE bool set(const QString& id,const QJSValue& scriptValue) {
        if(!m_values.contains(id) || m_readOnlyIds.contains(id) || m_widgetBinding.contains(id))return false;
        // A native text field must never display a truncated or invalid
        // programmatic value while ui.get() reports something different.
        if(m_textInputRules.contains(id) &&
           (!scriptValue.isString() || !acceptsTextValue(id,scriptValue.toString())))return false;
        // Convert on worker thread before queued GUI update.
        const QVariant value=scriptValue.toVariant(QJSValue::ConvertJSObjects);
        auto table=m_experimentTables.find(id);
        if(table!=m_experimentTables.end()) {
            ExperimentTableData::Mutation operation;
            if(!ExperimentTableData::decode(value,table->columns,table->maxRows,&operation) ||
               !ExperimentTableData::apply(&table->rows,table->maxRows,table->columns,operation))return false;
            emit updateRequested(id,value);
            return true;
        }
        m_values.insert(id,value);
        emit updateRequested(id,value);
        return true;
    }
    bool setNative(const QString& id,const QVariant& value) {
        if(!m_values.contains(id) ||
           (m_textInputRules.contains(id) &&
             (value.metaType().id()!=QMetaType::QString ||
              !acceptsTextValue(id,value.toString()))))return false;
        auto table=m_experimentTables.find(id);
        if(table!=m_experimentTables.end()) {
            ExperimentTableData::Mutation operation;
            if(!ExperimentTableData::decode(value,table->columns,table->maxRows,&operation) ||
               !ExperimentTableData::apply(&table->rows,table->maxRows,table->columns,operation))return false;
            emit updateRequested(id,value);return true;
        }
        m_values.insert(id,value);
        emit updateRequested(id,value);
        return true;
    }
    void receiveEvent(const QString& id,const QVariant& value) {
        auto table=m_experimentTables.find(id);
        if(table!=m_experimentTables.end()) {
            ExperimentTableData::Mutation operation;
            if(ExperimentTableData::decode(value,table->columns,table->maxRows,&operation))
                ExperimentTableData::apply(&table->rows,table->maxRows,table->columns,operation);
            return;
        }
        m_values.insert(id,value);
    }
    Q_INVOKABLE QStringList dataNames() const { return m_dataValues.keys(); }
    Q_INVOKABLE QJSValue dataNamesJs() const {
        // QJSEngine must receive a real Array, not a wrapped QStringList.
        return ScriptValueConversion::toJavaScript(m_engine, QVariant(m_dataValues.keys()));
    }
    Q_INVOKABLE bool hasData(const QString& name) const { return m_dataValues.contains(name); }
    Q_INVOKABLE QString boundDataForWidget(const QString& id) const {return m_widgetBinding.value(id);}
    Q_INVOKABLE QString dataType(const QString& name) const { return m_dataTypes.value(name); }
    Q_INVOKABLE QString lastDataError() const { return m_lastDataError; }
    // Worker-thread-only value accessor for native input event old/new comparison.
    QVariant dataValue(const QString& name) const { return m_dataValues.value(name); }
    Q_INVOKABLE void reportDataCallbackError(const QString& name,const QJSValue& error) {
        emit dataCallbackFailed(name,error);
    }
    Q_INVOKABLE QJSValue getData(const QString& name) const {
        return ScriptValueConversion::toJavaScript(m_engine,m_dataValues.value(name));
    }
    // A computed result is a typed, run-local read-only Panel Data value.
    // Registration rejects editable controls so native operator edits cannot
    // silently desynchronize a calculated output.
    Q_INVOKABLE bool registerComputedData(const QString& name) {
        if(!m_dataValues.contains(name) || m_computedNames.contains(name)) {
            m_lastDataError=QStringLiteral("Unknown or already computed Panel Data variable: %1").arg(name);
            return false;
        }
        for(auto it=m_widgetBinding.cbegin();it!=m_widgetBinding.cend();++it) {
            if(it.value()==name && !PanelDataSchema::inputEvent(m_widgetTypes.value(it.key())).isEmpty()) {
                m_lastDataError=QStringLiteral("Panel Data %1 is bound to an editable input; computed targets must use displays").arg(name);
                return false;
            }
        }
        m_computedNames.insert(name);
        m_lastDataError.clear();
        return true;
    }
    Q_INVOKABLE bool validateComputedValue(const QString& name,const QJSValue& value) {
        return validateDataValue(name,value);
    }
    Q_INVOKABLE bool setComputedData(const QString& name,const QJSValue& value) {
        if(!m_computedNames.contains(name)) {
            m_lastDataError=QStringLiteral("Panel Data %1 is not a computed variable").arg(name);
            return false;
        }
        if(!validateDataValue(name,value))return false;
        return changeData(name,value.toVariant());
    }
    Q_INVOKABLE bool setData(const QString& name,const QJSValue& value) {
        if(m_computedNames.contains(name)) {
            m_lastDataError=QStringLiteral("Panel Data %1 is computed and read-only").arg(name);
            return false;
        }
        if(!validateDataValue(name,value))return false;
        return changeData(name,value.toVariant());
    }
    // Called only by the native event handler before user JS callbacks.
    // Programmatic widget updates never produce a simulated operator action.
    bool setDataFromInput(const QString& id,const QString& event,const QVariant& value) {
        const QString name=m_widgetBinding.value(id);
        if(name.isEmpty() || m_computedNames.contains(name))return false;
        const QString inputEvent=PanelDataSchema::inputEvent(m_widgetTypes.value(id));
        if(inputEvent.isEmpty() || inputEvent!=event)return false;
        const QString type=m_dataTypes.value(name);
        if((type==QStringLiteral("number") && value.metaType().id()!=QMetaType::Double &&
            value.metaType().id()!=QMetaType::Int) ||
           (type==QStringLiteral("boolean") && value.metaType().id()!=QMetaType::Bool) ||
           (type==QStringLiteral("text") && value.metaType().id()!=QMetaType::QString))return false;
        return changeData(name,value);
    }
    void publishInitialData() {
        for(auto it=m_dataValues.cbegin();it!=m_dataValues.cend();++it)
            publish(it.key(),it.value());
    }
signals:
    void dataValueChanged(const QString& name,const QVariant& value);
    void dataCallbackFailed(const QString& name,const QJSValue& error);

    void updateRequested(const QString& id,const QVariant& value);
private:
    struct ExperimentTableState {
        int columns=0;
        int maxRows=ExperimentTableLimits::DefaultRows;
        ExperimentTableData::Store rows;
    };
    QHash<QString,ExperimentTableState> m_experimentTables;
    QHash<QString,QVariantMap> m_tableSelection;
    struct TextInputRule { bool ipv4=false; int maxLength=256; };
    bool acceptsTextValue(const QString& id,const QString& text) const {
        const auto it=m_textInputRules.constFind(id);
        return it==m_textInputRules.cend() ||
               TextInputValidation::allowedValue(it->ipv4,text,it->maxLength);
    }
    bool validateDataValue(const QString& name,const QJSValue& value) {
        const QString type=m_dataTypes.value(name);
        if(type.isEmpty()) {
            m_lastDataError=QStringLiteral("Unknown Panel Data variable: %1").arg(name);
            return false;
        }
        if((type==QStringLiteral("number") && (!value.isNumber() || !std::isfinite(value.toNumber()))) ||
           (type==QStringLiteral("boolean") && !value.isBool()) ||
           (type==QStringLiteral("text") && (!value.isString() || value.toString().size()>4096))) {
            m_lastDataError=QStringLiteral("Panel Data %1 requires a %2 value").arg(name,type);
            return false;
        }
        m_lastDataError.clear();
        return true;
    }
    bool changeData(const QString& name,const QVariant& value) {
        // One Panel Data text value can drive multiple inputs. Validate ALL
        // receivers before mutating any of them, preserving atomic fan-out.
        for(const QString& id:m_dataTargets.values(name)) {
            if(!acceptsTextValue(id,value.toString())) {
                m_lastDataError=QStringLiteral("Invalid value for %1 (%2): input length or IPv4 format")
                    .arg(name,id);
                return false;
            }
        }
        if(m_dataValues.value(name)==value)return true;
        m_dataValues.insert(name,value);
        m_lastDataError.clear();
        publish(name,value);
        return true;
    }
    void publish(const QString& name,const QVariant& value) {
        for(const QString& id:m_dataTargets.values(name)) {
            m_values.insert(id,value);
            emit updateRequested(id,value);
        }
        emit dataValueChanged(name,value);
    }
    QHash<QString,QString> m_dataTypes;
    QSet<QString> m_computedNames;
    QVariantMap m_dataValues;
    QHash<QString,QString> m_widgetBinding; // widget ID -> variable
    QMultiHash<QString,QString> m_dataTargets; // variable -> widget IDs
    QHash<QString,QString> m_widgetTypes;
    QHash<QString,TextInputRule> m_textInputRules;
    QString m_lastDataError;
    QJSEngine* m_engine=nullptr; // borrowed; must outlive this bridge
    QVariantMap m_values;
    QSet<QString> m_readOnlyIds;
};
