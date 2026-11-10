#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QSize>
#include "panel/PanelAppearance.h"
#include <QString>
#include <QVector>

// UI-independent document model. One project owns one main JavaScript source,
// its libraries, lifecycle configuration and a set of named pages.
// Pages own groups of widgets; pages NEVER own separate script engines.
// v5.6.25: the visible Page is selected by ID; the single panel runtime
// remains alive while the Page tab changes.
struct PanelPage {
    QString id = QStringLiteral("main"); // Stable machine ID, not the label.
    QString name = QStringLiteral("Main");
    QSize size{900,550};
    QJsonArray controls;
    QString background; // optional Page-only background override
};

struct PanelLifecycleSettings {
    bool initializeEnabled = false;
    bool shutdownEnabled = false;
};

class PanelProject final {
public:
    QString path;
    QString scriptName = QStringLiteral("Panel Script.js");
    QString script;
    QJsonArray libraries;
    // One authoritative, data-only native test definition per project.
    QJsonObject testDefinition;
    QJsonObject runInformation; // UUT/operator field schema; no per-run values
    QJsonObject reportStorage; // project-owned automatic archive policy, no execution data
    QString testLibrary; // Embedded .mjs exporting the step implementations.
    QString description = QStringLiteral("Portable native Qt panel with embedded JavaScript");
    PanelLifecycleSettings lifecycle;
    PanelAppearance appearance;
    QVector<PanelPage> pages;
    QString activePageId = QStringLiteral("main");

    PanelProject();
    static PanelProject newProject(const QString& name, const QString& source);
    const PanelPage& mainPage() const;
    PanelPage& mainPage();
    // Stable page IDs, with globally unique widget IDs across all pages.
    bool addPage(const QString& id, const QString& name, const QSize& size,
                 QString* error = nullptr);
    bool deletePage(const QString& id, QString* error = nullptr);
    PanelPage* findPage(const QString& id);
    const PanelPage* findPage(const QString& id) const;
    bool validate(QString* error = nullptr) const;
    // Legacy v1 loader: automatically wraps flat controls in the Main page.
    static bool fromJson(const QJsonObject& root, const QString& fileName,
                         PanelProject* output, QString* error = nullptr);
    static bool loadFile(const QString& fileName, PanelProject* output, QString* error = nullptr);
    // Retain v1 serialization for an unmodified single Main page; use v2 for
    // multiple/custom Pages with active Page ID and per-Page widget arrays.
    bool toJson(QJsonObject* output, QString* error = nullptr) const;
    bool saveFile(const QString& fileName, QString* error = nullptr) const;
private:
    QJsonObject m_extensions; // Preserve unknown top-level v1 metadata.
};
