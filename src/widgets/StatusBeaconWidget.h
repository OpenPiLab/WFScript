#pragma once
#include <QWidget>
#include <QString>

// Scalable status lamp for boolean/number readings. Illuminated state and
// design-time defaults are separate; hidden Pages retain their live state.
class StatusBeaconWidget final : public QWidget {
    Q_OBJECT
public:
    explicit StatusBeaconWidget(QWidget* parent=nullptr);
    void configure(const QString& title,double value,const QString& activeColor,
                   const QString& onText,const QString& offText,
                   bool invert,bool showGlow);
    void setValue(double value);
    bool active() const {return m_invert ? !m_rawActive : m_rawActive;}
    QSize sizeHint() const override {return QSize(290,120);}
    QSize minimumSizeHint() const override {return QSize(120,58);}
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title=QStringLiteral("Equipment ready");
    QString m_onText=QStringLiteral("ONLINE");
    QString m_offText=QStringLiteral("OFFLINE");
    QString m_color=QStringLiteral("green");
    bool m_rawActive=false;
    bool m_invert=false;
    bool m_showGlow=true;
};
