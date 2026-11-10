#pragma once
#include <QWidget>
class PanelGauge final : public QWidget {
public:
    explicit PanelGauge(QWidget* parent=nullptr);
    void configure(double lo,double hi,double value,QString title);
    void setValue(double value);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    double m_low=0,m_high=100,m_value=0;
    QString m_title;
};
