#pragma once
#include <QWidget>
#include <QString>

// Multi-state, display-only alarm/status visualization (0=OK,1=WARNING,2=ALARM).
class StatusBannerWidget final : public QWidget {
    Q_OBJECT
public:
    explicit StatusBannerWidget(QWidget* parent=nullptr);
    void configure(QString title,QString okText,QString warningText,QString alarmText,int level);
    void setLevel(int level);
    int level() const {return m_level;}
    QSize sizeHint() const override {return QSize(330,100);}
    QSize minimumSizeHint() const override {return QSize(125,50);}
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title="SYSTEM HEALTH";
    QString m_ok="NORMAL",m_warning="WARNING",m_alarm="ALARM";
    int m_level=0;
};
