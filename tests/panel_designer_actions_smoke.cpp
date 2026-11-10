// WFScript v5.6.12: native Qt Widgets designer commands; no DWF or scripts.
// Run via CTest with QT_QPA_PLATFORM=offscreen, or directly under Qt Creator.
#include "PanelCanvas.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDebug>
#include <QUndoStack>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace {
int fail(const char* message) { qCritical() << "FAIL:" << message; return 1; }
PanelControl make(const QString& id, int x, int y, int w=150, int h=40) {
    PanelControl c; c.type="button"; c.id=id; c.text=id;
    c.geometry=QRect(x,y,w,h);return c;
}
QRect geometry(const PanelCanvas& canvas,const QString& id) {
    for(const auto& c:canvas.toJson()) {
        const auto o=c.toObject();
        if(o.value("id").toString()==id)
            return QRect(o.value("x").toInt(),o.value("y").toInt(),
                         o.value("width").toInt(),o.value("height").toInt());
    }
    return {};
}
}
int main(int argc, char** argv) {
    QApplication app(argc,argv);
    PanelCanvas canvas;
    if(canvas.size()!=QSize(900,550) || canvas.documentSize()!=canvas.size())
        return fail("Exact canvas bounds must survive this designer revision");
    if(!canvas.addControl(make("first",20,20,110)) ||
       !canvas.addControl(make("second",240,100,160)))
        return fail("Add two ordinary controls");
    auto* history=canvas.undoStack();
    if(!history || !history->canUndo() || canvas.toJson().size()!=2)
        return fail("Adding controls must create an undo history");
    canvas.selectControlById("first");
    canvas.selectControlById("second",true);
    if(canvas.selectionCount()!=2) return fail("Ctrl-click-style multiselection");
    canvas.alignSelected("left");
    if(geometry(canvas,"first").x()!=geometry(canvas,"second").x())
        return fail("Align left follows the primary selection");
    history->undo();
    if(geometry(canvas,"first").x()!=20 || geometry(canvas,"second").x()!=240)
        return fail("Undo restores both original positions");
    history->redo();
    if(geometry(canvas,"first").x()!=240 || geometry(canvas,"second").x()!=240)
        return fail("Redo alignment");
    canvas.duplicateSelected();
    if(canvas.toJson().size()!=4 || canvas.selectionCount()!=2)
        return fail("Duplicate must create two uniquely named controls, selected as a group");
    const QJsonArray duplicated=canvas.toJson();
    history->undo();
    if(canvas.toJson().size()!=2) return fail("Undo duplicate is ONE step");
    history->redo();
    if(canvas.toJson()!=duplicated) return fail("Redo duplicate restores all controls");
    canvas.copySelected();
    if(!QApplication::clipboard()->mimeData()->hasFormat("application/x-wfscript-panel-controls+json"))
        return fail("Designer clipboard uses its own safe MIME type");
    canvas.pasteControls();
    if(canvas.toJson().size()!=6) return fail("Paste duplicated group");
    history->undo();
    if(canvas.toJson().size()!=4) return fail("Undo paste is ONE step");
    canvas.deleteSelected();
    if(canvas.toJson().size()!=2) return fail("Delete entire selected group");
    history->undo();
    if(canvas.toJson().size()!=4 || canvas.selectionCount()!=2)
        return fail("Undo group deletion restores all controls and their selection");
    canvas.selectControlById("first");
    canvas.nudgeSelected(10,0);
    if(geometry(canvas,"first").x()!=250) return fail("Keyboard nudge moves selected widget");
    history->undo();
    if(geometry(canvas,"first").x()!=240) return fail("Undo nudge");
    canvas.setSnapToGrid(true);
    if(!canvas.snapToGrid())return fail("Grid snapping can be enabled");
    // Save and reopen the same v1 panel control schema.
    const QJsonArray state=canvas.toJson();
    const QSize bounds=canvas.documentSize();
    QString error;
    if(!canvas.fromJson(state,&error) || canvas.toJson()!=state ||
       canvas.documentSize()!=bounds || history->canUndo())
        return fail("Load resets design history while preserving v1 controls and canvas bounds");
    canvas.setDesignMode(false);
    const QJsonArray runningState=canvas.toJson();
    canvas.selectAllControls();canvas.duplicateSelected();canvas.nudgeSelected(20,0);canvas.deleteSelected();
    if(canvas.toJson()!=runningState)return fail("Designer operations must be disabled in Run mode");
    qInfo()<<"PASS: multiselect, alignment, undo/redo, duplication, copy/paste, grouped delete, nudge, load, run protection";
    return 0;
}
