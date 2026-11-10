#pragma once
#include <QWidget>
#include <QString>
#include <QVariant>

// Read-only instrument result. Never drives hardware or emits a widget event.
// Numeric ui.set() evaluates inclusive bounds; {reset:true} restores NOT TESTED.
class MeasurementResultWidget final : public QWidget {
public:
    explicit MeasurementResultWidget(QWidget* parent=nullptr);
    void configure(const QString& title,const QString& unit,double low,double high,
                   int decimals);
    bool applyMeasurement(const QVariant& value);
    void reset();
    QString resultState() const { return m_state; }
    double measurement() const { return m_value; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title=QStringLiteral("Voltage Check");
    QString m_unit=QStringLiteral("V");
    QString m_state=QStringLiteral("NOT TESTED");
    double m_low=3.25, m_high=3.35, m_value=0.0;
    int m_decimals=3;
    bool m_hasValue=false;
};
