#pragma once

#include <QWidget>
#include <QString>

// Display-only instrument. Value and display configuration live in this QWidget;
// Pages never recreate it or store/replay a parallel GUI-value cache.
class DigitalDisplayWidget final : public QWidget {
    Q_OBJECT
public:
    explicit DigitalDisplayWidget(QWidget* parent = nullptr);
    void configure(const QString& title, double value, int decimals,
                   const QString& unit, const QString& accent,
                   bool showPlus, bool groupThousands,
                   bool alarmEnabled, double alarmHigh);
    void setValue(double value);
    double value() const { return m_value; }
    QString formattedValue() const;
    bool alarmActive() const;
    QSize sizeHint() const override { return QSize(265, 120); }
    QSize minimumSizeHint() const override { return QSize(100, 60); }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QString m_title = QStringLiteral("Measurement");
    QString m_unit = QStringLiteral("V");
    QString m_accent = QStringLiteral("cyan");
    double m_value = 0.0;
    double m_alarmHigh = 90.0;
    int m_decimals = 3;
    bool m_showPlus = false;
    bool m_groupThousands = false;
    bool m_alarmEnabled = false;
};
