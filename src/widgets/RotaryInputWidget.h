#pragma once
#include <QWidget>
#include <QString>
#include <QPointF>

class RotaryInputWidget final : public QWidget {
    Q_OBJECT
public:
    explicit RotaryInputWidget(QWidget* parent=nullptr);
    void configure(QString title, QString unit, QString accent,double value,
                   double min,double max,double step,int decimals);
    void setValue(double value); // UI-origin updates are emitted by handlers only
    double value() const {return m_value;}
    QSize sizeHint() const override {return QSize(255,285);}
    QSize minimumSizeHint() const override {return QSize(110,130);}
signals:
    void userValueChanged(double value);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    void changeBySteps(double steps);
    void userSetValue(double value);
    QPointF logicalPoint(const QPointF& pos) const;
    double clipped(double value) const;
    QString m_title="SETPOINT";
    QString m_unit="V";
    QString m_accent="cyan";
    double m_value=50,m_min=0,m_max=100,m_step=1;
    int m_decimals=1;
    enum class DragMode { None, Circular, Vertical };
    DragMode m_dragMode=DragMode::None;
    double m_anchorValue=0;
    double m_dragSteps=0;
    double m_lastAngle=0;
    bool m_haveAngle=false;
    QPointF m_lastPointer;
    double m_wheelRemainder=0;
};
