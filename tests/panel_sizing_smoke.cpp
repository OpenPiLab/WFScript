// WFScript v5.6.11: isolated Qt Widgets viewport, document-size and scrolling
// regression. Does not create MainWindow, start scripts or touch AD3 hardware.
#include "PanelDesigner.h"
#include "PanelCanvas.h"
#include <QApplication>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QDebug>

namespace {
int fail(const char* message) { qCritical() << "FAIL:" << message; return 1; }
void updateUi() {
    for (int i = 0; i < 4; ++i) QCoreApplication::processEvents();
}
}

int main(int argc, char** argv) {
    QApplication app(argc,argv);
    PanelDesigner panel;
    panel.resize(1500,900);
    panel.show();
    updateUi();
    auto* scroll = panel.canvasScrollArea();
    auto* canvas = panel.canvas();
    if (!scroll || !canvas || scroll->widget()!=canvas || scroll->widgetResizable())
        return fail("The scroll area must NOT stretch the exact-size panel canvas");
    if (canvas->documentSize() != QSize(900,550))
        return fail("Default saved canvas size is 900x550");
    panel.setFocusMode(true);
    updateUi();
    if (scroll->viewport()->width() <= 900 || scroll->viewport()->height() <= 550)
        return fail("Test window must have a large viewport in focus mode");
    if (canvas->size() != QSize(900,550) || canvas->documentSize()!=canvas->size())
        return fail("The 900x550 grid must visibly END at 900x550, not stretch with viewport");
    if (scroll->horizontalScrollBar()->maximum()!=0 ||
        scroll->verticalScrollBar()->maximum()!=0)
        return fail("No scrollbars when panel is smaller than the workspace");
    const QSize oversized(qMax(1850,scroll->viewport()->width()+250),
                          qMax(1200,scroll->viewport()->height()+250));
    if (!panel.setPanelSize(oversized))
        return fail("Explicit large panel dimensions accepted");
    updateUi();
    if (scroll->horizontalScrollBar()->maximum() <= 0 ||
        scroll->verticalScrollBar()->maximum() <= 0) {
        qCritical()<<"Scroll layout:"<<"canvas"<<canvas->size()
                   <<"viewport"<<scroll->viewport()->size()
                   <<"hmax"<<scroll->horizontalScrollBar()->maximum()
                   <<"vmax"<<scroll->verticalScrollBar()->maximum()
                   <<"scroll size"<<scroll->size();
        return fail("Oversized panel must have both scrollbars");
    }
    if (canvas->documentSize()!=oversized || canvas->size()!=oversized)
        return fail("Large document dimensions must match actual visible canvas dimensions");
    // Existing schema v1 still round-trips explicit dimensions and controls.
    const QJsonObject serialized{{"format", "WFScriptPanel"}, {"version", 1},
        {"canvasWidth",panel.panelSize().width()},
        {"canvasHeight",panel.panelSize().height()},
        {"controls",canvas->toJson()}, {"script", QStringLiteral("// test")}};
    const auto parsed=QJsonDocument::fromJson(QJsonDocument(serialized).toJson()).object();
    if(parsed.value("canvasWidth").toInt()!=oversized.width() ||
       parsed.value("canvasHeight").toInt()!=oversized.height())
        return fail("Explicit panel size serializes in v1 project JSON");

    if (!panel.setPanelSize(QSize(900,550)))
        return fail("Small panels accepted");
    updateUi();
    if (canvas->size()!=QSize(900,550) ||
        scroll->horizontalScrollBar()->maximum()!=0 ||
        scroll->verticalScrollBar()->maximum()!=0)
        return fail("Shrinking panel must shrink real widget AND remove scrollbars");
    // Simulates a viewport-resize operation: the panel remains exact-sized.
    panel.resize(1700,1000);
    updateUi();
    if (canvas->size()!=QSize(900,550))
        return fail("Resizing the editor must not silently change panel dimensions");
    PanelControl control;
    control.type=QStringLiteral("button");
    control.id=QStringLiteral("beyondOriginalDocument");
    control.text=QStringLiteral("Test");
    control.geometry=QRect(1150,650,150,40);
    if (!canvas->addControl(control)) return fail("Can place control beyond original 900x550 area");
    if (canvas->documentSize().width() < 1300 || canvas->documentSize().height() < 690 ||
        canvas->size()!=canvas->documentSize())
        return fail("Control placement must extend the ACTUAL canvas and saved bounds together");
    if (panel.setPanelSize(QSize(900,550)))
        return fail("Shrinking below a control must be rejected without clipping it");
    canvas->clearControls();
    if (!panel.setPanelSize(QSize(900,550)) || canvas->size()!=QSize(900,550))
        return fail("Can shrink both actual canvas and saved size after clearing outside controls");
    panel.setFocusMode(false);
    updateUi();
    if (!panel.toolboxShown() || !panel.propertiesShown())
        return fail("Original stable panel side tools still restore");
    qInfo() << "PASS: displayed canvas equals documented dimensions; oversized scrolls, shrinking works; v1 JSON unchanged; focus restores";
    return 0;
}
