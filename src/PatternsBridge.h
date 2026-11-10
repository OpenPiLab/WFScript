#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

class DwfRuntime;

// Limited, WaveForms-style Patterns1.Channels.DIO0 clock/PWM object. DIO0 is
// the *logical pattern channel name*, while Pin.value selects the real pin.
// All property changes are staged until an explicit config() or run().
class PatternNumber final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY changed)
    Q_PROPERTY(double real READ real NOTIFY changed)
public:
    explicit PatternNumber(double init, QObject* parent=nullptr):QObject(parent),m_value(init),m_real(init){}
    double value() const {return m_value;}
    double real() const {return m_real;}
    void setValue(double v){m_value=v;emit changed();}
    void setReal(double v){m_real=v;emit changed();}
signals: void changed();
private: double m_value=0,m_real=0;
};
class PatternText final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
public:
    explicit PatternText(const QString& init,QObject* parent=nullptr):QObject(parent),m_text(init){}
    QString text() const {return m_text;}
    void setText(const QString& s){m_text=s;emit changed();}
signals: void changed();
private: QString m_text;
};
class PatternChannel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Pin READ pin CONSTANT)
    Q_PROPERTY(QObject* DIO READ pin CONSTANT) // WaveForms-compatible physical pin selector alias
    Q_PROPERTY(QObject* Type READ type CONSTANT)
    Q_PROPERTY(QObject* Output READ output CONSTANT)
    Q_PROPERTY(QObject* Idle READ idle CONSTANT)
    Q_PROPERTY(QObject* Frequency READ frequency CONSTANT)
    Q_PROPERTY(QObject* Duty READ duty CONSTANT)
public:
    explicit PatternChannel(int pin=0):m_pin(pin),m_type(QStringLiteral("Clock")),
        m_output(QStringLiteral("PP")),m_idle(QStringLiteral("Z")),m_frequency(1000),m_duty(50){}
    QObject* pin(){return &m_pin;}
    QObject* type(){return &m_type;}
    QObject* output(){return &m_output;}
    QObject* idle(){return &m_idle;}
    QObject* frequency(){return &m_frequency;}
    QObject* duty(){return &m_duty;}
    // Mutable access for script configuration, const access for read-only status.
    PatternNumber& pinValue(){return m_pin;}
    const PatternNumber& pinValue() const{return m_pin;}
    PatternText& typeText(){return m_type;}
    const PatternText& typeText() const{return m_type;}
    PatternText& outputText(){return m_output;}
    const PatternText& outputText() const{return m_output;}
    PatternText& idleText(){return m_idle;}
    const PatternText& idleText() const{return m_idle;}
    PatternNumber& frequencyValue(){return m_frequency;}
    const PatternNumber& frequencyValue() const{return m_frequency;}
    PatternNumber& dutyValue(){return m_duty;}
    const PatternNumber& dutyValue() const{return m_duty;}
private:
    PatternNumber m_pin;
    PatternText m_type,m_output,m_idle;
    PatternNumber m_frequency,m_duty;
};
class PatternChannels final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* DIO0 READ dio0 CONSTANT)
    Q_PROPERTY(QObject* DIO1 READ dio1 CONSTANT)
    Q_PROPERTY(QObject* DIO2 READ dio2 CONSTANT)
public:
    QObject* dio0(){return &m_dio0;}
    QObject* dio1(){return &m_dio1;}
    QObject* dio2(){return &m_dio2;}
    PatternChannel& second(){return m_dio1;}
    PatternChannel& third(){return m_dio2;}
    PatternChannel& primary(){return m_dio0;}
    const PatternChannel& primary() const{return m_dio0;}
private: PatternChannel m_dio0{0},m_dio1{1},m_dio2{2};
};

class PatternsBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* Channels READ channels CONSTANT)
    Q_PROPERTY(QString lastError READ lastError)
    Q_PROPERTY(bool running READ running)
    Q_PROPERTY(bool simulation READ simulation CONSTANT)
    Q_PROPERTY(double clockHz READ clockHz)
public:
    explicit PatternsBridge(DwfRuntime* runtime,QObject* parent=nullptr);
    QObject* channels(){return &m_channels;}
    PatternChannels& channelGroup(){return m_channels;}
    const PatternChannels& channelGroup() const{return m_channels;}
    QString lastError() const{return m_error;}
    bool running() const{return m_running;}
    bool simulation() const{return !m_runtime;}
    double clockHz() const{return m_clockHz;}
    Q_INVOKABLE bool config(); // commit without energizing
    Q_INVOKABLE bool run();
    // WFScript convenience extension: synchronized custom A/B/index patterns.
    // The channel property objects remain WaveForms-style. Only runEncoder()
    // applies three outputs together; run() still runs one ordinary Clock.
    Q_INVOKABLE bool runEncoder(int aPin,int bPin,int indexPin,int ppr,double rpm,bool reverse);
    Q_INVOKABLE bool configEncoder(int aPin,int bPin,int indexPin,int ppr,double rpm,bool reverse);
    // WFScript extension, NOT native WaveForms scripting syntax. Each string
    // is a complete repeating period in chronological 0/1 bit order.
    Q_INVOKABLE bool runSequence(const QVariantList& pins, const QVariantList& bitStrings, double bitRateHz);
    Q_INVOKABLE bool configSequence(const QVariantList& pins, const QVariantList& bitStrings, double bitRateHz);
    Q_INVOKABLE bool stop();
    Q_INVOKABLE QVariantMap status() const;
private:
    bool commit(bool start);
    bool commitEncoder(int aPin,int bPin,int indexPin,int ppr,double rpm,bool reverse,bool start);
    bool commitSequence(const QVariantList& pins,const QVariantList& bitStrings,double bitRateHz,bool start);
    DwfRuntime* m_runtime=nullptr;
    PatternChannels m_channels;
    bool m_touched=false;
    bool m_running=false;
    double m_clockHz=0;
    double m_actualHz=0;
    double m_actualDuty=0;
    bool m_encoderMode=false;
    bool m_sequenceMode=false;
    QVariantList m_sequencePins;
    QVariantList m_sequenceBits;
    int m_sequenceLength=0,m_sequenceRepeatFactor=1;
    double m_sequenceRequestedBitRateHz=0,m_sequenceActualBitRateHz=0;
    int m_aPin=0,m_bPin=1,m_zPin=2,m_ppr=16;
    bool m_reverse=false;
    double m_rpm=600,m_actualRpm=0;
    QString m_error;
};
