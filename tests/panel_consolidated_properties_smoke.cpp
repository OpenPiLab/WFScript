// v5.6.41: GUI-independent-instrument QWidget smoke for inspector consolidation.
// Run with QT_QPA_PLATFORM=offscreen; no AD3 is opened.
#include "PanelDesigner.h"
#include "PanelCanvas.h"
#include "PanelProject.h"
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>

namespace {
int failure(const char* what) { qCritical() << "FAIL" << what; return 1; }
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PanelDesigner designer;
    designer.resize(1450,850);
    designer.show();
    QCoreApplication::processEvents();
    if(designer.findChild<QPushButton*>(QStringLiteral("panelSettingsButton")))
        return failure("legacy Settings toolbar button still exists");
    auto* width=designer.findChild<QSpinBox*>(QStringLiteral("panelPageWidth"));
    auto* height=designer.findChild<QSpinBox*>(QStringLiteral("panelPageHeight"));
    auto* fill=designer.findChild<QPushButton*>(QStringLiteral("panelFillWorkspace"));
    auto* initialize=designer.findChild<QCheckBox*>(QStringLiteral("enablePanelInitialize"));
    auto* shutdown=designer.findChild<QCheckBox*>(QStringLiteral("enablePanelShutdown"));
    auto* gotoInit=designer.findChild<QPushButton*>(QStringLiteral("editPanelInitialize"));
    if(!width || !height || !fill || !initialize || !shutdown || !gotoInit)
        return failure("consolidated inspector controls not found");
    if(width->value()!=900 || height->value()!=550)
        return failure("initial Page dimensions not reflected in inspector");

    width->setValue(1120);
    height->setValue(700);
    if(!QMetaObject::invokeMethod(height,"editingFinished",Qt::DirectConnection))
        return failure("Page size commit signal unavailable");
    if(designer.panelSize()!=QSize(1120,700))
        return failure("Page dimensions failed to update canvas");
    if(!designer.setPanelSize(QSize(1500,850)) || width->value()!=1500 || height->value()!=850)
        return failure("programmatic canvas size failed to refresh inspector");

    bool changed=false;
    bool receivedInit=false,receivedShutdown=false;
    QObject::connect(&designer,&PanelDesigner::panelLifecycleEdited,&designer,
                     [&](bool a,bool b){changed=true;receivedInit=a;receivedShutdown=b;});
    PanelLifecycleSettings saved;
    saved.initializeEnabled=true;
    designer.setPanelLifecycle(saved);
    if(!initialize->isChecked() || changed)
        return failure("loading lifecycle wrongly emitted an edit");
    shutdown->setChecked(true);
    if(!changed || !receivedInit || !receivedShutdown)
        return failure("lifecycle checkboxes failed to report edits");

    QString selectedHook;
    QObject::connect(&designer,&PanelDesigner::panelLifecycleFunctionRequested,
                     &designer,[&](const QString& name){selectedHook=name;});
    gotoInit->click();
    if(selectedHook!=QStringLiteral("onInitialize"))
        return failure("navigation to lifecycle function not forwarded");

    // Document persistence is the same, independent of where the user edits it.
    PanelProject project;
    project.mainPage().size=designer.panelSize();
    project.lifecycle.initializeEnabled=receivedInit;
    project.lifecycle.shutdownEnabled=receivedShutdown;
    QJsonObject root;
    QString error;
    if(!project.toJson(&root,&error))return failure("serialization failed");
    PanelProject restored;
    if(!PanelProject::fromJson(root,QString(),&restored,&error))
        return failure("deserialization failed");
    if(restored.mainPage().size!=designer.panelSize() ||
       !restored.lifecycle.initializeEnabled || !restored.lifecycle.shutdownEnabled)
        return failure("dimensions/lifecycle failed JSON round-trip");

    designer.setRunning(true);
    if(width->isEnabled() || initialize->isEnabled() || fill->isEnabled())
        return failure("Properties remain editable during Run mode");
    designer.setRunning(false);
    if(!width->isEnabled() || !initialize->isEnabled())
        return failure("Properties did not restore in Design mode");
    qInfo() << "PASS: consolidated Panel/Page Properties and lifecycle persistence";
    return 0;
}
