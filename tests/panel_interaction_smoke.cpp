// WFScript v5.6.13: Qt Widgets marquee selection + immediate properties regression.
// Compile with -DWFSCRIPT_BUILD_TESTS=ON and run in Qt Creator or ctest.
#include "PanelDesigner.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSpinBox>
#include <QTimer>
#include <QUndoStack>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

namespace {
int fail(const char* message) { qCritical() << "FAIL:" << message;return 1; }
PanelControl control(const QString& id,int x,int y) {
    PanelControl c; c.id=id;c.type="button";c.text=id;c.geometry=QRect(x,y,90,34);return c;
}
void waitForProperties() {
    QEventLoop loop;
    QTimer::singleShot(350,&loop,&QEventLoop::quit);
    loop.exec();
}
int xFor(const PanelCanvas* canvas,const QString& name) {
    for (const auto& v:canvas->toJson()) {
        const auto obj=v.toObject();
        if (obj.value("id").toString()==name)return obj.value("x").toInt();
    }
    return -100000;
}
void dragMarquee(PanelCanvas* canvas,const QPoint& a,const QPoint& b,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
    const auto send=[canvas](QEvent::Type type,QPoint pos,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods){
        QMouseEvent e(type,QPointF(pos),QPointF(canvas->mapToGlobal(pos)),
                      type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,buttons,mods);
        QCoreApplication::sendEvent(canvas,&e);
    };
    send(QEvent::MouseButtonPress,a,Qt::LeftButton,modifiers);
    send(QEvent::MouseMove,b,Qt::LeftButton,modifiers);
    send(QEvent::MouseButtonRelease,b,Qt::NoButton,modifiers);
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    qInfo()<<"INTERACTION STAGE 1: construct PanelDesigner";
    PanelDesigner designer;
    auto* canvas=designer.canvas();
    if (!designer.setPanelSize(QSize(900,550)) || canvas->size()!=QSize(900,550))
        return fail("Preserve exact document bounds");
    if(!canvas->addControl(control("A",40,45)) ||
       !canvas->addControl(control("B",195,90)) ||
       !canvas->addControl(control("C",420,270)))
        return fail("Fixture setup");
    canvas->resetDesignHistory();
    qInfo()<<"INTERACTION STAGE 2: show designer";
    designer.show();
    QCoreApplication::processEvents();
    // The rectangle overlaps A/B but not C. Select via actual mouse events.
    qInfo()<<"INTERACTION STAGE 3: initial marquee";
    dragMarquee(canvas,QPoint(12,15),QPoint(310,160));
    if(canvas->selectionCount()!=2)return fail("Marquee drag selects two widgets");
    if(canvas->undoStack()->canUndo())return fail("Selection alone does not create undo history");
    // Ctrl+marquee keeps previous selection and adds C.
    qInfo()<<"INTERACTION STAGE 4: additive marquee";
    dragMarquee(canvas,QPoint(380,220),QPoint(550,350),Qt::ControlModifier);
    if(canvas->selectionCount()!=3)return fail("Ctrl+marquee adds to selection");
    qInfo()<<"INTERACTION STAGE 5: select and inspect";
    canvas->selectControlById("A");
    auto* x=designer.findChild<QSpinBox*>("panelPropertyX");
    auto* id=designer.findChild<QLineEdit*>("panelPropertyId");
    if(!x || !id || !designer.findChild<QLineEdit*>("panelPropertyText"))
        return fail("Find property controls");
    qInfo()<<"INTERACTION STAGE 6: property X edit";
    x->setValue(65);
    waitForProperties();
    if(xFor(canvas,"A")!=65)return fail("Spin edits apply without Apply button");
    if(!canvas->undoStack()->canUndo())return fail("Property edit creates Undo");
    qInfo()<<"INTERACTION STAGE 7: undo and redo";
    canvas->undoStack()->undo();
    if(xFor(canvas,"A")!=40)return fail("Undo restores property edit");
    canvas->undoStack()->redo();
    if(xFor(canvas,"A")!=65)return fail("Redo restores property edit");
    // Text edits commit on editingFinished and are reflected in serialized JSON.
    qInfo()<<"INTERACTION STAGE 8: caption and id edits";
    // Undo/Redo restores complete widget snapshots. The selected-property
    // controls may be recreated, so never dereference a cached child editor.
    auto* text=designer.findChild<QLineEdit*>("panelPropertyText");
    id=designer.findChild<QLineEdit*>("panelPropertyId");
    if(!text || !id)return fail("Inspector editors restored after Undo/Redo");
    qInfo()<<"INTERACTION STAGE 8a: commit caption";
    text->setText("New caption");
    QMetaObject::invokeMethod(text,"editingFinished",Qt::DirectConnection);
    qInfo()<<"INTERACTION STAGE 8b: verify caption";
    if(canvas->selectedControl().text!="New caption")return fail("Caption auto-commits on editingFinished");
    // Caption commit may refresh the inspector; always query the active ID field.
    id=designer.findChild<QLineEdit*>("panelPropertyId");
    if(!id)return fail("ID editor remains available after caption commit");
    qInfo()<<"INTERACTION STAGE 8c: commit ID";
    id->setText("RenamedA");
    QMetaObject::invokeMethod(id,"editingFinished",Qt::DirectConnection);
    qInfo()<<"INTERACTION STAGE 8d: verify renamed ID";
    if(canvas->selectedControl().id!="RenamedA")return fail("ID rename auto-commits");
    qInfo()<<"INTERACTION STAGE 9: serialize and reload";
    const QJsonArray serialized=canvas->toJson();
    QString error;
    if(!canvas->fromJson(serialized,&error) || canvas->toJson()!=serialized)
        return fail("Serialization unchanged and valid");
    qInfo()<<"INTERACTION STAGE 10: run mode isolation";
    designer.setRunning(true);
    const auto runningState=canvas->toJson();
    dragMarquee(canvas,QPoint(12,15),QPoint(550,350));
    if(canvas->toJson()!=runningState)return fail("Marquee cannot alter controls in Run mode");
    designer.setRunning(false);
    qInfo()<<"PASS: marquee, additive marquee, live properties, Undo/Redo, rename, project schema, Run isolation";
    return 0;
}
