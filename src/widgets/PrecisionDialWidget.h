#pragma once
#include <QWidget>
#include <QString>

// Display-only precision dial. No animation timers, state caches, or signal
// coupling: the instance owns its value for its entire Page lifetime.
class PrecisionDialWidget final : public QWidget {
    Q_OBJECT
public:
    explicit PrecisionDialWidget(QWidget* parent=nullptr);
    void configure(const QString& title,double value,double minimum,double maximum,
                   const QString& unit,const QString& accent,int decimals,
                   bool alarmEnabled,double alarmHigh,bool showTicks);
    void setValue(double value);
    double value() const {return m_value;}
    bool alarmActive() const;
    QSize sizeHint() const override {return QSize(320,260);}
    QSize minimumSizeHint() const override {return QSize(135,110);}
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title=QStringLiteral("Precision voltage");
    QString m_unit=QStringLiteral("V");
    QString m_accent=QStringLiteral("cyan");
    double m_value=0.0;
    double m_min=0.0;
    double m_max=100.0;
    double m_alarmHigh=85.0;
    int m_decimals=2;
    bool m_alarmEnabled=false;
    bool m_showTicks=true;
};
