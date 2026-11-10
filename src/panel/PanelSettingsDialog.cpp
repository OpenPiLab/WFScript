#include "PanelSettingsDialog.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

PanelSettingsResult editPanelSettings(QWidget* parent,
                                      const PanelLifecycleSettings& current) {
    PanelSettingsResult result;
    result.lifecycle=current;
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("panelLifecycleSettingsDialog"));
    dialog.setWindowTitle(QStringLiteral("Panel Settings — Lifecycle"));
    dialog.resize(570,350);
    auto* layout=new QVBoxLayout(&dialog);
    auto* description=new QLabel(QStringLiteral(
        "Lifecycle functions belong to the entire panel project, not individual Pages. "
        "Each enabled hook is called once per runtime session. Changing Pages "
        "will not restart the JavaScript engine."),&dialog);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* init=new QCheckBox(QStringLiteral("Enable onInitialize()"),&dialog);
    init->setObjectName(QStringLiteral("enablePanelInitialize"));
    init->setChecked(current.initializeEnabled);
    layout->addWidget(init);
    auto* initInfo=new QLabel(QStringLiteral(
        "Runs after loading the main script and its libraries, before panel events are enabled. "
        "An exception or missing enabled function prevents the panel from entering Running."),&dialog);
    initInfo->setWordWrap(true);
    layout->addWidget(initInfo);
    auto* createInit=new QPushButton(QStringLiteral("Create / Go to onInitialize()"),&dialog);
    createInit->setObjectName(QStringLiteral("editPanelInitialize"));
    layout->addWidget(createInit);
    layout->addSpacing(12);
    auto* shutdown=new QCheckBox(QStringLiteral("Enable onShutdown()"),&dialog);
    shutdown->setObjectName(QStringLiteral("enablePanelShutdown"));
    shutdown->setChecked(current.shutdownEnabled);
    layout->addWidget(shutdown);
    auto* shutdownInfo=new QLabel(QStringLiteral(
        "Runs during a normal Stop, before native instrument cleanup. "
        "It is skipped if JavaScript was forcibly interrupted or startup failed. "
        "Native cleanup always remains WFScript's responsibility."),&dialog);
    shutdownInfo->setWordWrap(true);
    layout->addWidget(shutdownInfo);
    auto* createShutdown=new QPushButton(QStringLiteral("Create / Go to onShutdown()"),&dialog);
    createShutdown->setObjectName(QStringLiteral("editPanelShutdown"));
    layout->addWidget(createShutdown);
    layout->addStretch();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(createInit,&QPushButton::clicked,&dialog,[&]{
        result.navigateTo=QStringLiteral("onInitialize");dialog.accept();
    });
    QObject::connect(createShutdown,&QPushButton::clicked,&dialog,[&]{
        result.navigateTo=QStringLiteral("onShutdown");dialog.accept();
    });
    if(dialog.exec()!=QDialog::Accepted)return result;
    result.accepted=true;
    result.lifecycle.initializeEnabled=init->isChecked();
    result.lifecycle.shutdownEnabled=shutdown->isChecked();
    return result;
}
