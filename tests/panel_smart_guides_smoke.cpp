// RC1.5.5: design-time smart alignment regression. Run under Qt offscreen.
#include "PanelCanvas.h"
#include "panel/PanelSmartGuides.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFrame>
#include <QMouseEvent>
#include <QUndoStack>
#include <QDebug>
#include <QJsonObject>

namespace {
int fail(const char* what) { qCritical() << "FAIL smart guides:" << what; return 1; }
PanelControl make(const QString& id, const QRect& rect) {
    PanelControl c;
    c.type=QStringLiteral("button");c.id=id;c.text=id;c.geometry=rect;
    return c;
}
QRect geometry(const PanelCanvas& c,const QString& id) {
    for (const auto& value:c.toJson()) {
        const auto item=value.toObject();
        if (item.value("id").toString()==id)
            return QRect(item.value("x").toInt(),item.value("y").toInt(),
                         item.value("width").toInt(),item.value("height").toInt());
    }
    return {};
}
void moveWidget(QWidget* widget, const QPoint& offset, Qt::KeyboardModifiers mods=Qt::NoModifier) {
    const QPoint origin=widget->mapToGlobal(QPoint(5,5));
    const auto send=[&](QEvent::Type type, const QPoint& global,Qt::MouseButtons buttons){
        QMouseEvent event(type,QPointF(5,5),QPointF(global),
            type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,buttons,mods);
        QCoreApplication::sendEvent(widget,&event);
    };
    send(QEvent::MouseButtonPress,origin,Qt::LeftButton);
    send(QEvent::MouseMove,origin+offset,Qt::LeftButton);
    send(QEvent::MouseButtonRelease,origin+offset,Qt::NoButton);
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    using namespace PanelSmartGuides;
    const QVector<QRect> targets{QRect(100,100,120,50)};
    const auto nextTo=match(QRect(225,151,70,35),targets);
    if (!nextTo.x.matched || nextTo.x.delta!=-5 || nextTo.x.guide!=220 ||
        !nextTo.y.matched || nextTo.y.delta!=-1 || nextTo.y.guide!=150)
        return fail("adjoining right/bottom edge snap");
    if (match(QRect(238,164,70,35),targets).x.matched)
        return fail("no false snap outside tolerance");
    const auto centers=match(QRect(147,123,40,20),{QRect(100,100,120,50)});
    // Moving centre at 167, stationary centre at 160 (7 px): should NOT snap.
    if (centers.x.matched) return fail("centre snap must obey tolerance");
    const auto centered=match(QRect(143,119,40,20),targets);
    if (!centered.x.matched || centered.x.guide!=160 ||
        !centered.y.matched || centered.y.guide!=125)
        return fail("centre alignment");
    const auto resize=match(QRect(100,100,115,46),targets,6,true);
    if (!resize.x.matched || resize.x.delta!=5 || !resize.y.matched || resize.y.delta!=4)
        return fail("trailing resize edge aligns to peer");

    PanelCanvas canvas;
    canvas.show();
    if (!canvas.alignmentGuidesEnabled()) return fail("guides enabled by default");
    if (!canvas.addControl(make("fixed",QRect(100,100,120,50))) ||
        !canvas.addControl(make("moving",QRect(265,165,70,35))))
        return fail("create test controls");
    canvas.resetDesignHistory();
    QApplication::processEvents();
    auto* w=canvas.findChild<QWidget*>(QStringLiteral("moving"));
    auto* overlay=canvas.findChild<QWidget*>(QStringLiteral("panelAlignmentOverlay"));
    if (!w || !overlay || !overlay->testAttribute(Qt::WA_TransparentForMouseEvents))
        return fail("interactive widgets and noninterfering overlay");
    moveWidget(w,QPoint(-40,-14));
    if(geometry(canvas,"moving").topLeft()!=QPoint(220,150))return fail("move snaps to peer edges");
    if(overlay->isVisible())return fail("overlay clears on release");
    if (!canvas.undoStack()->canUndo())return fail("snap is single undo operation");
    canvas.undoStack()->undo();
    if(geometry(canvas,"moving").topLeft()!=QPoint(265,165))return fail("undo restores unsnapped location");
    canvas.undoStack()->redo();
    if(geometry(canvas,"moving").topLeft()!=QPoint(220,150))return fail("redo restores snapped location");
    // Restore original fixture and bypass the magnet temporarily using Alt.
    canvas.selectControlById(QStringLiteral("moving"));
    auto def=canvas.selectedControl();
    def.geometry=QRect(265,165,70,35);
    if(!canvas.updateSelected(def)) return fail("restore geometry in inspector");
    w=canvas.findChild<QWidget*>(QStringLiteral("moving"));
    moveWidget(w,QPoint(-40,-14),Qt::AltModifier);
    if(geometry(canvas,"moving").topLeft()!=QPoint(225,151))return fail("Alt disables guides");
    canvas.setAlignmentGuidesEnabled(false);
    if(canvas.alignmentGuidesEnabled())return fail("guide toggle state");
    canvas.setDesignMode(false);
    if(overlay->isVisible())return fail("no overlay in Run");
    qInfo() << "PASS: geometry, threshold, centres, resize, drag, Alt bypass, Undo/Redo, Run mode";
    return 0;
}
