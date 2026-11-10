// v5.6.33.2: precision-trackpad/rotary input and stable native editor typography.
// Runs with QT_QPA_PLATFORM=offscreen, without an Analog Discovery device.
#include "widgets/RotaryInputWidget.h"
#include "widgets/NumericStepperWidget.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QEvent>
#include <QEnterEvent>
#include <QDebug>
#include <cmath>

#define CHECK(expr) do { if (!(expr)) { qCritical() << "FAIL:" << #expr << __LINE__; return 1; } } while (false)
static void mouse(QWidget& w,QEvent::Type type,QPointF local,Qt::MouseButtons buttons,
                  Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
    const QPointF global=w.mapToGlobal(local.toPoint());
    const Qt::MouseButton button=(type==QEvent::MouseMove)?Qt::NoButton:Qt::LeftButton;
    QMouseEvent e(type,local,global,button,buttons,modifiers);
    QApplication::sendEvent(&w,&e);
}
static void wheel(QWidget& w,QPoint pixels,QPoint angles,Qt::ScrollPhase phase=Qt::ScrollUpdate) {
    const QPointF local(w.width()/2.0,w.height()/2.0);
    QWheelEvent e(local,w.mapToGlobal(local.toPoint()),pixels,angles,
                  Qt::NoButton,Qt::NoModifier,phase,false);
    QApplication::sendEvent(&w,&e);
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    RotaryInputWidget dial;
    dial.resize(255,285);
    dial.configure("FREQUENCY","Hz","cyan",50,0,100,1,1);
    dial.show();
    int changed=0;
    QObject::connect(&dial,&RotaryInputWidget::userValueChanged,[&](double){ ++changed; });
    dial.setValue(50);CHECK(changed==0);
    // A genuine 90-degree clockwise gesture, from the top of the face
    // towards its right side, must increase the setpoint.
    mouse(dial,QEvent::MouseButtonPress,QPointF(127.5,73),Qt::LeftButton);
    mouse(dial,QEvent::MouseMove,QPointF(197.5,143),Qt::LeftButton);
    mouse(dial,QEvent::MouseButtonRelease,QPointF(197.5,143),Qt::NoButton);
    CHECK(dial.value()>75 && dial.value()<90 && changed>0);
    // The pointer hit-test must match isotropic painting on resized dials.
    dial.resize(510,285);dial.setValue(50);
    mouse(dial,QEvent::MouseButtonPress,QPointF(255,73),Qt::LeftButton);
    mouse(dial,QEvent::MouseMove,QPointF(325,143),Qt::LeftButton);
    mouse(dial,QEvent::MouseButtonRelease,QPointF(325,143),Qt::NoButton);
    CHECK(dial.value()>75 && dial.value()<90);
    dial.resize(255,285);
    // Trackpad events typically deliver fractional wheel notches. Eight 15/120
    // angle deltas should produce exactly one configured 1-Hz increment.
    dial.setValue(50);changed=0;
    for(int i=0;i<7;++i)wheel(dial,{},QPoint(0,15));
    CHECK(dial.value()==50 && changed==0);
    wheel(dial,{},QPoint(0,15));
    CHECK(dial.value()==51 && changed==1);
    changed=0;
    for(int i=0;i<8;++i)wheel(dial,{},QPoint(0,-15));
    CHECK(dial.value()==50 && changed==1);
    // Pixel-delta mode is preferred where provided by touchpads.
    dial.setValue(50);changed=0;
    for(int i=0;i<4;++i)wheel(dial,QPoint(0,7),{});
    CHECK(dial.value()==51 && changed==1);
    wheel(dial,QPoint(0,280),{},Qt::ScrollMomentum);
    CHECK(dial.value()==51 && changed==1); // no inertial overrun
    // The fallback vertical drag uses a reasonable 16px per increment.
    dial.setValue(50);changed=0;
    mouse(dial,QEvent::MouseButtonPress,QPointF(20,20),Qt::LeftButton);
    mouse(dial,QEvent::MouseMove,QPointF(20,4),Qt::LeftButton);
    mouse(dial,QEvent::MouseButtonRelease,QPointF(20,4),Qt::NoButton);
    CHECK(dial.value()==51 && changed==1);

    NumericStepperWidget numeric;
    numeric.resize(320,118);
    numeric.configure("SETPOINT","V","cyan",12.5,-100,100,0.1,2);
    numeric.show();
    QApplication::processEvents();
    QDoubleSpinBox* spin=numeric.findChild<QDoubleSpinBox*>();
    CHECK(spin!=nullptr);
    QLineEdit* editor=spin->findChild<QLineEdit*>();
    CHECK(editor!=nullptr);
    const int initial=editor->font().pixelSize();
    CHECK(initial>=20);
    CHECK(spin->font().pixelSize()==initial);
    editor->setFocus(Qt::MouseFocusReason);
    QEnterEvent enter(QPointF(5,5),QPointF(5,5),editor->mapToGlobal(QPoint(5,5)));
    QApplication::sendEvent(editor,&enter);
    QApplication::processEvents();
    CHECK(editor->font().pixelSize()==initial);
    editor->clearFocus();
    QEvent leave(QEvent::Leave);QApplication::sendEvent(editor,&leave);
    QApplication::processEvents();
    CHECK(editor->font().pixelSize()==initial);
    numeric.setValue(88.88);
    CHECK(editor->font().pixelSize()==initial);
    numeric.resize(480,177);
    QApplication::processEvents();
    const int larger=editor->font().pixelSize();
    CHECK(larger>initial);
    numeric.resize(320,118);
    QApplication::processEvents();
    CHECK(editor->font().pixelSize()==initial);
    qInfo()<<"PASS: smooth trackpad, circular drag, vertical input, stable/responsive numeric font";
    return 0;
}
