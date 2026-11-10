#pragma once
#include <QWidget>
#include <QString>

// A native, accessible input control. No Page-specific state cache.
// Momentary: down while held, returns false on release/cancel.
// Switch: toggles on click, retains its own live state.
class InstrumentButtonWidget final : public QWidget {
    Q_OBJECT
public:
    explicit InstrumentButtonWidget(QWidget* parent=nullptr);
    void configure(QString title,QString onText,QString offText,QString accent,
                   bool latching,bool initialState);
    void setState(bool state); // programmatic; never generates an input event
    bool state() const {return m_state;}
    bool isLatching() const {return m_latching;}
    QSize sizeHint() const override {return QSize(235,105);}
    QSize minimumSizeHint() const override {return QSize(100,52);}
signals:
    void userPressed();
    void userReleased();
    void userToggled(bool state);
    void userValueChanged(bool state);
    void userClicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void changeEvent(QEvent*) override;
private:
    void beginPress();
    void endPress(bool activate);
    QString m_title="CONTROL";
    QString m_onText="ON";
    QString m_offText="OFF";
    QString m_accent="cyan";
    bool m_latching=false;
    bool m_state=false;
    bool m_down=false;
};
