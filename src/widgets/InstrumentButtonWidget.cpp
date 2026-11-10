#include "InstrumentButtonWidget.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QLinearGradient>
#include <utility>

InstrumentButtonWidget::InstrumentButtonWidget(QWidget* parent):QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
}
void InstrumentButtonWidget::configure(QString title,QString onText,QString offText,
    QString accent,bool latching,bool initialState) {
    m_title=std::move(title);m_onText=std::move(onText);m_offText=std::move(offText);
    m_accent=std::move(accent);m_latching=latching;m_down=false;
    m_state=initialState;update();
}
void InstrumentButtonWidget::setState(bool value) {if(m_state==value)return;m_state=value;update();}
void InstrumentButtonWidget::beginPress() {
    if(m_down || !isEnabled())return;
    m_down=true;
    emit userPressed();
    if(!m_latching) {setState(true);emit userValueChanged(true);}
    update();
}
void InstrumentButtonWidget::endPress(bool activate) {
    if(!m_down)return;
    m_down=false;
    if(!m_latching) {
        if(m_state) {setState(false);emit userValueChanged(false);}
        emit userReleased();
    } else if(activate) {
        setState(!m_state);
        emit userToggled(m_state);
        emit userValueChanged(m_state);
    }
    if(activate)emit userClicked();
    update();
}
void InstrumentButtonWidget::mousePressEvent(QMouseEvent* e) {
    if(e->button()==Qt::LeftButton && isEnabled()) {setFocus(Qt::MouseFocusReason);beginPress();e->accept();}
    else QWidget::mousePressEvent(e);
}
void InstrumentButtonWidget::mouseReleaseEvent(QMouseEvent* e) {
    if(e->button()==Qt::LeftButton && m_down) {endPress(rect().contains(e->pos()));e->accept();}
    else QWidget::mouseReleaseEvent(e);
}
void InstrumentButtonWidget::keyPressEvent(QKeyEvent* e) {
    if((e->key()==Qt::Key_Space || e->key()==Qt::Key_Return || e->key()==Qt::Key_Enter)
        && !e->isAutoRepeat()) {beginPress();e->accept();return;}
    QWidget::keyPressEvent(e);
}
void InstrumentButtonWidget::keyReleaseEvent(QKeyEvent* e) {
    if((e->key()==Qt::Key_Space || e->key()==Qt::Key_Return || e->key()==Qt::Key_Enter)
        && !e->isAutoRepeat() && m_down) {endPress(true);e->accept();return;}
    QWidget::keyReleaseEvent(e);
}
void InstrumentButtonWidget::focusOutEvent(QFocusEvent* e) {endPress(false);QWidget::focusOutEvent(e);}
void InstrumentButtonWidget::changeEvent(QEvent* e) {
    if(e->type()==QEvent::EnabledChange && !isEnabled())endPress(false);
    QWidget::changeEvent(e);
}
void InstrumentButtonWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);InstrumentVisuals::startPaint(p,this,235,105);
    InstrumentVisuals::background(p,QRectF(0,0,235,105),13,this);
    const QColor accent=InstrumentVisuals::accent(m_accent);
    p.setPen(InstrumentVisuals::tone(this, QColor(175,197,214)));
    QFont header=InstrumentVisuals::labelFont(font(),13,true);
    p.setFont(header);
    p.drawText(QRectF(12,7,210,20),Qt::AlignCenter,
               InstrumentVisuals::elide(header,m_title,208));
    const QRectF button(12,35,211,59);
    QLinearGradient gradient(button.topLeft(),button.bottomLeft());
    const bool active=m_state || m_down;
    gradient.setColorAt(0,active?accent.lighter(130):InstrumentVisuals::tone(this, QColor(60,79,97)));
    gradient.setColorAt(1,active?accent.darker(165):InstrumentVisuals::tone(this, QColor(30,45,62)));
    p.setPen(QPen(active?accent:InstrumentVisuals::tone(this, QColor(94,117,137)),1.6));p.setBrush(gradient);
    p.drawRoundedRect(button,12,12);
    p.setPen(active?InstrumentVisuals::tone(this, QColor(10,24,32)):InstrumentVisuals::tone(this, QColor(222,234,245)));
    const QString label=active?m_onText:m_offText;
    QFont value=InstrumentVisuals::numericFont(font(),24,label,185);
    p.setFont(value);p.drawText(button,Qt::AlignCenter,label);
    if(hasFocus()) {p.setBrush(Qt::NoBrush);p.setPen(QPen(InstrumentVisuals::tone(this, QColor(223,238,251)),1,Qt::DashLine));
        p.drawRoundedRect(button.adjusted(3,3,-3,-3),9,9);}
    if(!isEnabled()){p.fillRect(QRectF(0,0,235,105),InstrumentVisuals::tone(this, QColor(8,13,20,90)));}
}
