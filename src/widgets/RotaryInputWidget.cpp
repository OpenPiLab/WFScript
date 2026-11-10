#include "RotaryInputWidget.h"
#include "InstrumentVisuals.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QRadialGradient>
#include <cmath>
#include <utility>

RotaryInputWidget::RotaryInputWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Drag around the dial, drag vertically outside the ring, "
                              "or use the trackpad/wheel and arrow keys. "
                              "Hold Shift for fine adjustment."));
}
void RotaryInputWidget::configure(QString title,QString unit,QString accent,double value,
                                  double min,double max,double step,int decimals) {
    if(!std::isfinite(value)||!std::isfinite(min)||!std::isfinite(max)||
       !std::isfinite(step)||max<=min||step<=0)return;
    m_title=std::move(title);m_unit=std::move(unit);m_accent=std::move(accent);
    m_min=min;m_max=max;m_step=step;m_decimals=qBound(0,decimals,6);
    m_value=clipped(value);update();
}
double RotaryInputWidget::clipped(double v) const {return qBound(m_min,v,m_max);}
void RotaryInputWidget::setValue(double v) {
    if(!std::isfinite(v))return;
    v=clipped(v);if(qFuzzyCompare(1.+m_value,1.+v))return;
    m_value=v;update();
}
void RotaryInputWidget::userSetValue(double v) {
    if(!std::isfinite(v))return;
    double old=m_value;setValue(v);
    if(m_value!=old)emit userValueChanged(m_value);
}
void RotaryInputWidget::changeBySteps(double steps) {
    const double candidate=m_min+std::round((m_value-m_min)/m_step+steps)*m_step;
    userSetValue(clipped(candidate));
}
// Painting uses a centered 255 x 285 logical coordinate space. Pointer input
// must use the identical transform, including for resized/aspect-stretched widgets.
QPointF RotaryInputWidget::logicalPoint(const QPointF& pos) const {
    const double scale=qMin(width()/255.0,height()/285.0);
    if(scale<=0.0)return QPointF();
    return QPointF((pos.x()-(width()-255.0*scale)/2.0)/scale,
                   (pos.y()-(height()-285.0*scale)/2.0)/scale);
}
void RotaryInputWidget::mousePressEvent(QMouseEvent* e) {
    if(e->button()!=Qt::LeftButton){QWidget::mousePressEvent(e);return;}
    const QPointF logical=logicalPoint(e->position());
    const QPointF center(127.5,143.0);
    const double distance=std::hypot(logical.x()-center.x(),logical.y()-center.y());
    // The dial face and illuminated ring use true angular rotation. The label
    // and other free space retain a slower vertical-drag alternative.
    m_dragMode=(distance>=36.0 && distance<=114.0)
        ?DragMode::Circular:DragMode::Vertical;
    m_anchorValue=m_value;
    m_dragSteps=0.0;
    m_lastPointer=e->position();
    m_lastAngle=std::atan2(center.y()-logical.y(),logical.x()-center.x());
    m_haveAngle=true;
    setFocus(Qt::MouseFocusReason);
    e->accept();
}
void RotaryInputWidget::mouseMoveEvent(QMouseEvent* e) {
    if(m_dragMode==DragMode::None){QWidget::mouseMoveEvent(e);return;}
    const double sensitivity=(e->modifiers() & Qt::ShiftModifier)?0.25:1.0;
    if(m_dragMode==DragMode::Circular) {
        const QPointF logical=logicalPoint(e->position());
        const QPointF center(127.5,143.0);
        // The angle is undefined near the centre; skip movements there rather
        // than generating an enormous value jump as the pointer crosses it.
        if(std::hypot(logical.x()-center.x(),logical.y()-center.y())<30.0) {
            m_haveAngle=false;
        } else {
            const double angle=std::atan2(center.y()-logical.y(),logical.x()-center.x());
            if(m_haveAngle) {
                constexpr double pi=3.14159265358979323846;
                // Signed shortest angular distance crosses +/-pi smoothly.
                // Clockwise follows the painted min-to-max sweep.
                const double delta=std::remainder(m_lastAngle-angle,2.0*pi);
                m_dragSteps+=delta*(180.0/pi)*(m_max-m_min)/(270.0*m_step)*sensitivity;
            }
            m_lastAngle=angle;
            m_haveAngle=true;
        }
    } else {
        // Vertical drag is intentionally less sensitive than the old 7 px
        // per step scheme, and integrates small pointer movements continuously.
        m_dragSteps+=(m_lastPointer.y()-e->position().y())/16.0*sensitivity;
    }
    m_lastPointer=e->position();
    // Avoid accumulated overtravel at either stop: reverse motion should
    // immediately move the value away from the limit, not unwind dead travel.
    m_dragSteps=qBound((m_min-m_anchorValue)/m_step,m_dragSteps,
                       (m_max-m_anchorValue)/m_step);
    userSetValue(m_anchorValue+std::round(m_dragSteps)*m_step);
    e->accept();
}
void RotaryInputWidget::mouseReleaseEvent(QMouseEvent* e) {
    if(e->button()==Qt::LeftButton && m_dragMode!=DragMode::None) {
        m_dragMode=DragMode::None;
        m_haveAngle=false;
        e->accept();
    } else QWidget::mouseReleaseEvent(e);
}
void RotaryInputWidget::wheelEvent(QWheelEvent* e) {
    // Precision trackpads often deliver very small angleDelta values (or
    // pixelDelta with angleDelta zero). Do not round each event separately.
    // Accumulating fractional steps lets a two-finger gesture reliably move
    // through the configured increment without excessive sensitivity.
    // Do not allow kinetic scrolling to keep changing a potentially live
    // instrument setpoint after the user's fingers have left the trackpad.
    if(e->phase()==Qt::ScrollMomentum){e->accept();return;}
    const QPoint pixels=e->pixelDelta();
    const QPoint angles=e->angleDelta();
    double steps=pixels.y()!=0?pixels.y()/28.0:angles.y()/120.0;
    if(steps==0.0) {
        if(e->phase()==Qt::ScrollBegin || e->phase()==Qt::ScrollEnd)e->accept();
        else QWidget::wheelEvent(e);
        return;
    }
    if(e->phase()==Qt::ScrollBegin)m_wheelRemainder=0.0;
    if(e->modifiers() & Qt::ShiftModifier)steps*=0.25;
    m_wheelRemainder+=steps;
    const double wholeSteps=std::trunc(m_wheelRemainder);
    if(wholeSteps!=0.0) {
        m_wheelRemainder-=wholeSteps;
        changeBySteps(wholeSteps);
    }
    // Equally avoid a stale scroll remainder pushing against the end stops.
    if((m_value>=m_max && steps>0.0)||(m_value<=m_min && steps<0.0))
        m_wheelRemainder=0.0;
    e->accept();
}
void RotaryInputWidget::keyPressEvent(QKeyEvent* e) {
    if(e->key()==Qt::Key_Up||e->key()==Qt::Key_Right){changeBySteps(1);e->accept();}
    else if(e->key()==Qt::Key_Down||e->key()==Qt::Key_Left){changeBySteps(-1);e->accept();}
    else if(e->key()==Qt::Key_Home){userSetValue(m_min);e->accept();}
    else if(e->key()==Qt::Key_End){userSetValue(m_max);e->accept();}
    else QWidget::keyPressEvent(e);
}
void RotaryInputWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);InstrumentVisuals::startPaint(p,this,255,285);
    InstrumentVisuals::background(p,QRectF(0,0,255,285),13,this);
    const QColor active=InstrumentVisuals::accent(m_accent);
    QFont title=InstrumentVisuals::labelFont(font(),16,true);p.setFont(title);
    p.setPen(InstrumentVisuals::tone(this, QColor(192,211,225)));p.drawText(QRectF(14,11,227,27),Qt::AlignCenter,
        InstrumentVisuals::elide(title,m_title,224));
    const QPointF c(127.5,143.0);const QRectF arc(38.5,54,178,178);
    p.setBrush(Qt::NoBrush);p.setPen(QPen(InstrumentVisuals::tone(this, QColor(58,78,95)),12,Qt::SolidLine,Qt::RoundCap));
    p.drawArc(arc,225*16,-270*16);
    const double f=InstrumentVisuals::fraction(m_value,m_min,m_max);
    p.setPen(QPen(active,10,Qt::SolidLine,Qt::RoundCap));
    p.drawArc(arc,225*16,-int(270*f*16));
    for(int i=0;i<=10;i++) {
        const double angle=(225-270.0*i/10.0)*3.14159265358979323846/180.;
        p.setPen(QPen(InstrumentVisuals::tone(this, QColor(116,144,165)),1.5));
        p.drawLine(QPointF(c.x()+std::cos(angle)*107,c.y()-std::sin(angle)*107),
                   QPointF(c.x()+std::cos(angle)*112,c.y()-std::sin(angle)*112));
    }
    QRadialGradient face(QPointF(c.x()-25,c.y()-29),109);
    face.setColorAt(0,InstrumentVisuals::tone(this, QColor(70,90,111)));face.setColorAt(.6,InstrumentVisuals::tone(this, QColor(34,48,64)));
    face.setColorAt(1,InstrumentVisuals::tone(this, QColor(14,23,35)));
    p.setBrush(face);p.setPen(QPen(InstrumentVisuals::tone(this, QColor(97,125,146)),2));p.drawEllipse(c,76,76);
    const double angle=(225-270*f)*3.14159265358979323846/180.;
    p.setPen(QPen(active,5,Qt::SolidLine,Qt::RoundCap));
    p.drawLine(QPointF(c.x()+std::cos(angle)*48,c.y()-std::sin(angle)*48),
               QPointF(c.x()+std::cos(angle)*66,c.y()-std::sin(angle)*66));
    const QString reading=InstrumentVisuals::number(m_value,m_decimals);
    p.setPen(InstrumentVisuals::tone(this, QColor(224,237,247)));p.setFont(InstrumentVisuals::numericFont(font(),30,reading,142));
    p.drawText(QRectF(57,128,141,40),Qt::AlignCenter,reading);
    p.setPen(active);p.setFont(InstrumentVisuals::labelFont(font(),12,true));
    p.drawText(QRectF(65,167,125,19),Qt::AlignCenter,m_unit);
    p.setPen(InstrumentVisuals::tone(this, QColor(139,164,185)));p.setFont(InstrumentVisuals::labelFont(font(),11));
    p.drawText(QRectF(15,242,94,22),Qt::AlignLeft,InstrumentVisuals::number(m_min,m_decimals));
    p.drawText(QRectF(146,242,94,22),Qt::AlignRight,InstrumentVisuals::number(m_max,m_decimals));
    if(hasFocus()) {p.setPen(QPen(active,1,Qt::DotLine));p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(5,5,245,275),10,10);}
}
