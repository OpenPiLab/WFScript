#pragma once
#include <QWidget>
#include <QString>

// High-contrast segmented meter with held peak. Peak is transient visual state
// owned by this QWidget; it is not serialized into a PanelControl or reloaded
// whenever the user changes Page.
class SegmentedBarWidget final : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedBarWidget(QWidget* parent=nullptr);
    void configure(const QString& title,double value,double minimum,double maximum,
                   const QString& unit,const QString& accent,int decimals,
                   int segments,int warningPct,int alarmPct,bool peakHold);
    void setValue(double value);
    double value() const {return m_value;}
    double peakValue() const {return m_peak;}
    bool alarmActive() const;
    QSize sizeHint() const override {return QSize(355,140);}
    QSize minimumSizeHint() const override {return QSize(150,65);}
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title=QStringLiteral("Load monitor");
    QString m_unit=QStringLiteral("%");
    QString m_accent=QStringLiteral("green");
    double m_value=0.0,m_peak=0.0,m_min=0,m_max=100;
    int m_decimals=1,m_segments=28,m_warningPct=70,m_alarmPct=90;
    bool m_peakHold=true;
};
