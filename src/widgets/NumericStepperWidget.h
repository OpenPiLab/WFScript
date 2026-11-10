#pragma once
#include <QWidget>
#include <QString>

class QDoubleSpinBox;
class QPushButton;

// High-precision input with full-size increment/decrement buttons and
// dynamically scaled child fonts. Uses native QDoubleSpinBox editing rules.
class NumericStepperWidget final : public QWidget {
    Q_OBJECT
public:
    explicit NumericStepperWidget(QWidget* parent=nullptr);
    void configure(QString title,QString unit,QString accent,double value,
                   double minimum,double maximum,double step,int decimals);
    void setValue(double value); // no userValueChanged on programmatic updates
    double value() const;
    double singleStep() const;
    QSize sizeHint() const override {return QSize(320,118);}
    QSize minimumSizeHint() const override {return QSize(145,65);}
signals:
    void userValueChanged(double value);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void changeEvent(QEvent*) override;
private:
    void layoutChildren();
    void applyPalette();
    void applyEditorFont();
    QDoubleSpinBox* m_spin=nullptr;
    QPushButton* m_minus=nullptr;
    QPushButton* m_plus=nullptr;
    QString m_title="PRECISION SETPOINT";
    QString m_unit="V";
    QString m_accent="cyan";
    bool m_programmatic=false;
    int m_editorFontPx=0;
    int m_styledFontPx=-1;
    QString m_styledAccent;
};
