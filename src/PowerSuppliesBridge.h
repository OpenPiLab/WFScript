#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>

class DwfRuntime;

class SupplyMasterSetting final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY changed)
public:
    bool checked() const { return m_checked; }
    void setChecked(bool v) { if(m_checked!=v){m_checked=v;emit changed();} }
signals: void changed();
private: bool m_checked=false;
};
class SupplyEnableSetting final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY changed)
public:
    int value() const { return m_value; }
    void setValue(int v) { if(m_value!=v){m_value=v;emit changed();} }
signals: void changed();
private: int m_value=0;
};
class SupplyVoltageSetting final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY changed)
    Q_PROPERTY(double real READ real NOTIFY measuredChanged)
public:
    explicit SupplyVoltageSetting(double initial, QObject* parent=nullptr):QObject(parent),m_value(initial){}
    double value() const { return m_value; }
    double real() const { return m_real; }
    void setValue(double v) { if(m_value!=v){m_value=v;emit changed();} }
    void setMeasured(double v) { if(m_real!=v){m_real=v;emit measuredChanged();} }
signals: void changed(); void measuredChanged();
private: double m_value=0.0, m_real=0.0;
};
class SupplyOutputChannel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Enable READ enable CONSTANT)
    Q_PROPERTY(QObject* Voltage READ voltage CONSTANT)
public:
    explicit SupplyOutputChannel(double volts, QObject* parent=nullptr):QObject(parent),m_voltage(volts){}
    QObject* enable() {return &m_enable;}
    QObject* voltage() {return &m_voltage;}
    int enabled() const {return m_enable.value();}
    double volts() const {return m_voltage.value();}
    void setMeasured(double v){m_voltage.setMeasured(v);}
private:
    SupplyEnableSetting m_enable;
    SupplyVoltageSetting m_voltage;
};
class SupplyOutputs final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* PositiveSupply READ positive CONSTANT)
    Q_PROPERTY(QObject* NegativeSupply READ negative CONSTANT)
public:
    QObject* positive(){return &m_positive;}
    QObject* negative(){return &m_negative;}
    SupplyOutputChannel& positiveChannel(){return m_positive;}
    SupplyOutputChannel& negativeChannel(){return m_negative;}
private:
    SupplyOutputChannel m_positive{1.0};
    SupplyOutputChannel m_negative{-1.0};
};
// WaveForms: Supplies.MasterEnable.checked, Supplies.Output.PositiveSupply.Voltage.value,
// Supplies.Output.PositiveSupply.Enable.value (and NegativeSupply equivalents).
// WFScript explicitly commits edits with config()/run(); stop() is fail-closed.
class PowerSuppliesBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* MasterEnable READ masterEnable CONSTANT)
    Q_PROPERTY(QObject* Output READ output CONSTANT)
    Q_PROPERTY(QString lastError READ lastError)
    Q_PROPERTY(bool running READ running)
public:
    explicit PowerSuppliesBridge(DwfRuntime* runtime, QObject* parent=nullptr);
    QObject* masterEnable(){return &m_master;}
    QObject* output(){return &m_outputs;}
    SupplyOutputs& outputs(){return m_outputs;}
    QString lastError() const{return m_error;}
    bool running() const{return m_running;}
    Q_INVOKABLE bool config();
    Q_INVOKABLE bool run();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE QVariantMap status();
private:
    DwfRuntime* m_runtime=nullptr; // null = simulation, with no DWF SDK calls
    SupplyMasterSetting m_master;
    SupplyOutputs m_outputs;
    bool m_touched=false;
    bool m_running=false;
    QString m_error;
};
