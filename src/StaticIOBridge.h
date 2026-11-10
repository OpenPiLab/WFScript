#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>
class DwfRuntime;
class StaticIOBridge;

// Cached value is the REQUESTED output: 0 low, 1 high, 2 high impedance.
// Input.value takes a new SDK snapshot and reports the actual logic level.
class StaticIOInput final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int value READ value)
public:
    StaticIOInput(StaticIOBridge* bridge,int index,QObject* parent);
    int value() const;
private:
    StaticIOBridge* m_bridge;
    int m_index;
};
class StaticIOGroupInput final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int value READ value)
public:
    StaticIOGroupInput(StaticIOBridge* bridge,int start,QObject* parent);
    int value() const;
private:
    StaticIOBridge* m_bridge;
    int m_start;
};
class StaticIOMode final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text CONSTANT)
public:
    explicit StaticIOMode(QObject* parent):QObject(parent){}
    QString text() const{return QStringLiteral("I/Os");}
};
class StaticIOPin final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY changed)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(QObject* Input READ input CONSTANT)
public:
    StaticIOPin(StaticIOBridge* bridge,int index,QObject* parent);
    int value() const {return m_state;}
    QString text() const;
    void setValue(int v);
    void setText(const QString& value);
    QObject* input(){return &m_input;}
signals:
    void changed();
private:
    int m_state=2; // Always default to Z. No writes occur on construction.
    StaticIOInput m_input;
};
class StaticIOGroup final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Input READ input CONSTANT)
    Q_PROPERTY(QObject* Mode READ mode CONSTANT)
    Q_PROPERTY(QObject* DIO0 READ dio0 CONSTANT)
    Q_PROPERTY(QObject* DIO1 READ dio1 CONSTANT)
    Q_PROPERTY(QObject* DIO2 READ dio2 CONSTANT)
    Q_PROPERTY(QObject* DIO3 READ dio3 CONSTANT)
    Q_PROPERTY(QObject* DIO4 READ dio4 CONSTANT)
    Q_PROPERTY(QObject* DIO5 READ dio5 CONSTANT)
    Q_PROPERTY(QObject* DIO6 READ dio6 CONSTANT)
    Q_PROPERTY(QObject* DIO7 READ dio7 CONSTANT)
    Q_PROPERTY(QObject* DIO8 READ dio8 CONSTANT)
    Q_PROPERTY(QObject* DIO9 READ dio9 CONSTANT)
    Q_PROPERTY(QObject* DIO10 READ dio10 CONSTANT)
    Q_PROPERTY(QObject* DIO11 READ dio11 CONSTANT)
    Q_PROPERTY(QObject* DIO12 READ dio12 CONSTANT)
    Q_PROPERTY(QObject* DIO13 READ dio13 CONSTANT)
    Q_PROPERTY(QObject* DIO14 READ dio14 CONSTANT)
    Q_PROPERTY(QObject* DIO15 READ dio15 CONSTANT)
public:
    StaticIOGroup(StaticIOBridge* bridge,int start,QObject* parent);
    QObject* pin(int globalIndex) const;
    QObject* input(){return &m_input;}
    QObject* mode(){return &m_mode;}
    QObject* dio0() const { return pin(0); }
    QObject* dio1() const { return pin(1); }
    QObject* dio2() const { return pin(2); }
    QObject* dio3() const { return pin(3); }
    QObject* dio4() const { return pin(4); }
    QObject* dio5() const { return pin(5); }
    QObject* dio6() const { return pin(6); }
    QObject* dio7() const { return pin(7); }
    QObject* dio8() const { return pin(8); }
    QObject* dio9() const { return pin(9); }
    QObject* dio10() const { return pin(10); }
    QObject* dio11() const { return pin(11); }
    QObject* dio12() const { return pin(12); }
    QObject* dio13() const { return pin(13); }
    QObject* dio14() const { return pin(14); }
    QObject* dio15() const { return pin(15); }
private:
    int m_start;
    QVector<StaticIOPin*> m_pins;
    StaticIOGroupInput m_input;
    StaticIOMode m_mode;
};

// WaveForms-like StaticIO.Channel0.DIO0.value and .Input.value.
// Unlike the WaveForms UI, WFScript stages assignments until config(), or
// offers setPin(index,state) for an explicitly committed single-pin change.
class StaticIOBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Channel0 READ channel0 CONSTANT)
    Q_PROPERTY(QObject* Channel1 READ channel1 CONSTANT)
    Q_PROPERTY(QString lastError READ lastError)
    Q_PROPERTY(bool simulation READ simulation CONSTANT)
public:
    explicit StaticIOBridge(DwfRuntime* runtime,QObject* parent=nullptr);
    QObject* channel0(){return &m_ch0;}
    QObject* channel1(){return &m_ch1;}
    QString lastError() const {return m_error;}
    bool simulation() const {return !m_runtime;}
    StaticIOPin* pin(int index) const;
    Q_INVOKABLE bool config();
    Q_INVOKABLE bool setPin(int index,int value);
    Q_INVOKABLE int readPin(int index);
    Q_INVOKABLE int readGroup(int start);
    Q_INVOKABLE QVariantMap status();
    Q_INVOKABLE QVariantMap capabilities();
    Q_INVOKABLE bool stop();
private:
    DwfRuntime* m_runtime=nullptr;
    StaticIOGroup m_ch0;
    StaticIOGroup m_ch1;
    bool m_touched=false;
    bool m_confirmedOff=true;
    quint32 m_outputEnabled=0;
    quint32 m_outputLevels=0;
    quint32 m_inputLevels=0;
    quint32 m_outputSupported=0xffffu;
    quint32 m_inputSupported=0xffffu;
    QString m_error;
};
