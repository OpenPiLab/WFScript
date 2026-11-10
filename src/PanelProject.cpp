#include "PanelProject.h"
#include "PanelEventCatalog.h"
#include "RunInformationSchema.h"
#include "TestReportStorage.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QtGlobal>
#include <utility>

namespace {
bool fail(QString* error,const QString& message) {
    if (error) *error=message;
    return false;
}
bool validPageId(const QString& id) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z_][A-Za-z0-9_-]{0,63}$"));
    return re.match(id).hasMatch();
}
bool validLibraryName(const QString& name) {
    if(name.isEmpty() || name.size()>180 || name.contains(QLatin1Char('\\')) ||
       name.startsWith(QLatin1Char('/')) || name.contains(QLatin1Char(':'))) return false;
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]*$"));
    for(const QString& part : name.split(QLatin1Char('/')))
        if(part.isEmpty() || part==QStringLiteral(".") || part==QStringLiteral("..") || !re.match(part).hasMatch())return false;
    return name.endsWith(QStringLiteral(".js"),Qt::CaseInsensitive) ||
           name.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive);
}
bool validScriptName(const QString& name) {
    return !name.isEmpty() && QFileInfo(name).fileName()==name &&
           !name.contains(QLatin1Char('\\')) &&
           name.endsWith(QStringLiteral(".js"),Qt::CaseInsensitive);
}
bool validateControls(const QJsonArray& controls,QString* error) {
    if(controls.size()>200)return fail(error,QStringLiteral("Too many controls on page (limit 200)"));
    QSet<QString> ids;
    for(const auto& value : controls) {
        if(!value.isObject())return fail(error,QStringLiteral("Invalid control entry"));
        const QJsonObject o=value.toObject();
        const QString id=o.value(QStringLiteral("id")).toString();
        if(id.isEmpty() || ids.contains(id))return fail(error,QStringLiteral("Missing or duplicate control id: %1").arg(id));
        if(!o.value(QStringLiteral("type")).isString() ||
           !o.value(QStringLiteral("x")).isDouble() || !o.value(QStringLiteral("y")).isDouble() ||
           !o.value(QStringLiteral("width")).isDouble() || !o.value(QStringLiteral("height")).isDouble())
            return fail(error,QStringLiteral("Invalid control fields: %1").arg(id));
        if(o.contains(QStringLiteral("events")) && !o.value(QStringLiteral("events")).isObject())
            return fail(error,QStringLiteral("Invalid event bindings for %1").arg(id));
        if(!PanelEventCatalog::validateBindings(o.value(QStringLiteral("type")).toString(),
                                                o.value(QStringLiteral("events")).toObject(),error))return false;
        const int x=o.value(QStringLiteral("x")).toInt();
        const int y=o.value(QStringLiteral("y")).toInt();
        const int w=o.value(QStringLiteral("width")).toInt();
        const int h=o.value(QStringLiteral("height")).toInt();
        // Legacy panels were permitted to contain controls with negative X/Y.
        // Keep those existing projects loadable; users can move controls back
        // into the visible page through the graphical designer.
        if(w<25 || h<22 || w>3000 || h>3000 ||
           static_cast<qint64>(x)+w>8192 || static_cast<qint64>(y)+h>8192)
            return fail(error,QStringLiteral("Invalid control geometry: %1").arg(id));
        ids.insert(id);
    }
    return true;
}
}

PanelProject::PanelProject() { pages.append(PanelPage{}); }
PanelProject PanelProject::newProject(const QString& name,const QString& source) {
    PanelProject project;
    project.scriptName=name;
    project.script=source;
    return project;
}
const PanelPage& PanelProject::mainPage() const { return pages.first(); }
PanelPage& PanelProject::mainPage() { return pages.first(); }
PanelPage* PanelProject::findPage(const QString& id) {
    for(auto& page : pages)if(page.id==id)return &page;
    return nullptr;
}
const PanelPage* PanelProject::findPage(const QString& id) const {
    for(const auto& page : pages)if(page.id==id)return &page;
    return nullptr;
}
bool PanelProject::addPage(const QString& id,const QString& name,const QSize& size,QString* error) {
    if(!validPageId(id) || findPage(id) || name.trimmed().isEmpty() || name.size()>80 ||
       size.width()<320 || size.height()<240 || size.width()>8192 || size.height()>8192)
        return fail(error,QStringLiteral("Invalid or duplicate page ID, name or dimensions"));
    for(const auto& existing:pages)
        if(existing.name.compare(name.trimmed(),Qt::CaseInsensitive)==0)
            return fail(error,QStringLiteral("Page name already exists: %1").arg(name.trimmed()));
    PanelPage page;
    page.id=id; page.name=name.trimmed();page.size=size;
    pages.append(page);
    return true;
}
bool PanelProject::deletePage(const QString& id,QString* error) {
    if(pages.size()<=1)return fail(error,QStringLiteral("The last page cannot be deleted"));
    for(int i=0;i<pages.size();++i)if(pages.at(i).id==id) {
        pages.removeAt(i); // Widget collection belongs to the page and is removed with it.
        if(activePageId==id)activePageId=pages.first().id;
        return true;
    }
    return fail(error,QStringLiteral("Page does not exist: %1").arg(id));
}
bool PanelProject::validate(QString* error) const {
    if(!validScriptName(scriptName))return fail(error,QStringLiteral("Invalid main script name"));
    if(!appearance.valid())return fail(error,QStringLiteral("Invalid Panel appearance"));
    if(script.toUtf8().size()>2*1024*1024)return fail(error,QStringLiteral("Main script exceeds 2 MiB"));
    if(pages.isEmpty() || pages.size()>64)return fail(error,QStringLiteral("Invalid page count"));
    if(!findPage(activePageId))return fail(error,QStringLiteral("Active Page does not exist: %1").arg(activePageId));
    qint64 embeddedBase64Size=0;
    QSet<QString> pageIds;
    QSet<QString> pageNames;
    QSet<QString> controlIds; // globally unique: runtime ui.get/set uses IDs, not page qualifiers
    for(const auto& page : pages) {
        if(!validPageId(page.id) || pageIds.contains(page.id) || page.name.trimmed().isEmpty() ||
           page.name.size()>80 || page.size.width()<320 || page.size.height()<240 ||
           page.size.width()>8192 || page.size.height()>8192)
            return fail(error,QStringLiteral("Invalid or duplicate page definition: %1").arg(page.id));
        if(!PanelAppearance::validColor(page.background))
            return fail(error,QStringLiteral("Invalid Page background: %1").arg(page.id));
        pageIds.insert(page.id);
        if(pageNames.contains(page.name.trimmed().toCaseFolded()))
            return fail(error,QStringLiteral("Duplicate Page name: %1").arg(page.name));
        pageNames.insert(page.name.trimmed().toCaseFolded());
        if(!validateControls(page.controls,error))return false;
        for(const auto& value : page.controls) {
            const QString id=value.toObject().value(QStringLiteral("id")).toString();
            if(controlIds.contains(id))return fail(error,QStringLiteral("Widget ID is used on more than one page: %1").arg(id));
            controlIds.insert(id);
            const QJsonObject c=value.toObject();
            if(c.value(QStringLiteral("type")).toString()==QStringLiteral("image")) {
                const QJsonValue raw=c.value(QStringLiteral("properties")).toObject().value(QStringLiteral("imageData"));
                if(!raw.isUndefined() && !raw.isString())return fail(error,QStringLiteral("Invalid embedded image on %1").arg(id));
                embeddedBase64Size+=raw.toString().size();
                if(embeddedBase64Size>20*1024*1024)
                    return fail(error,QStringLiteral("Project embedded images exceed 20 MiB"));
            }
        }
    }
    if(!RunInformationSchema::validate(runInformation,error))return false;
    if(!TestReportStorage::validate(reportStorage,error))return false;
    if(!reportStorage.isEmpty() && testDefinition.isEmpty())
        return fail(error,QStringLiteral("Report Storage requires a project Test Executive definition"));
    if(!runInformation.isEmpty() && testDefinition.isEmpty())
        return fail(error,QStringLiteral("Run Information requires a project Test Executive definition"));
    if(!testDefinition.isEmpty()) {
        if(!validLibraryName(testLibrary) || !testLibrary.endsWith(QStringLiteral(".mjs"),Qt::CaseInsensitive))
            return fail(error,QStringLiteral("Test definition requires an embedded .mjs testLibrary"));
        bool exists=false;
        for(const auto& library:libraries)
            if(library.toObject().value(QStringLiteral("name")).toString()==testLibrary)exists=true;
        if(!exists)return fail(error,QStringLiteral("Test handler library is missing: %1").arg(testLibrary));
    }
    if(libraries.size()>64)return fail(error,QStringLiteral("Too many embedded libraries"));
    QSet<QString> names;
    qsizetype bytes=0;
    for(const auto& entry : libraries) {
        if(!entry.isObject())return fail(error,QStringLiteral("Invalid library entry"));
        const QJsonObject obj=entry.toObject();
        const QString name=obj.value(QStringLiteral("name")).toString();
        if(!validLibraryName(name) || !obj.value(QStringLiteral("source")).isString() ||
           names.contains(name.toCaseFolded()))return fail(error,QStringLiteral("Invalid or duplicate library name: %1").arg(name));
        names.insert(name.toCaseFolded());
        bytes+=obj.value(QStringLiteral("source")).toString().toUtf8().size();
        if(bytes>2*1024*1024)return fail(error,QStringLiteral("Embedded libraries exceed 2 MiB"));
    }
    return true;
}
bool PanelProject::fromJson(const QJsonObject& root,const QString& fileName,PanelProject* output,QString* error) {
    if(!output)return fail(error,QStringLiteral("Missing project destination"));
    const int version=root.value(QStringLiteral("version")).toInt();
    if(root.value(QStringLiteral("format")).toString()!=QStringLiteral("WFScriptPanel") ||
       (version!=1 && version!=2) || !root.value(QStringLiteral("script")).isString())
        return fail(error,QStringLiteral("Unsupported .wfpanel format or schema version"));
    PanelProject tmp;
    tmp.path=fileName;
    tmp.script=root.value(QStringLiteral("script")).toString();
    tmp.scriptName=root.value(QStringLiteral("scriptName")).toString();
    if(tmp.scriptName.isEmpty())tmp.scriptName=QFileInfo(fileName).completeBaseName()+QStringLiteral(".js");
    tmp.description=root.value(QStringLiteral("description")).toString(tmp.description);
    if(root.contains(QStringLiteral("libraries")) && !root.value(QStringLiteral("libraries")).isArray())
        return fail(error,QStringLiteral("Libraries must be an array"));
    tmp.libraries=root.value(QStringLiteral("libraries")).toArray();
    if(root.contains(QStringLiteral("testDefinition")) && !root.value(QStringLiteral("testDefinition")).isObject())
        return fail(error,QStringLiteral("testDefinition must be an object"));
    tmp.testDefinition=root.value(QStringLiteral("testDefinition")).toObject();
    if(root.contains(QStringLiteral("runInformation")) && !root.value(QStringLiteral("runInformation")).isObject())
        return fail(error,QStringLiteral("runInformation must be an object"));
    tmp.runInformation=root.value(QStringLiteral("runInformation")).toObject();
    if(root.contains(QStringLiteral("reportStorage")) && !root.value(QStringLiteral("reportStorage")).isObject())
        return fail(error,QStringLiteral("reportStorage must be an object"));
    tmp.reportStorage=root.value(QStringLiteral("reportStorage")).toObject();
    if(root.contains(QStringLiteral("testLibrary")) && !root.value(QStringLiteral("testLibrary")).isString())
        return fail(error,QStringLiteral("testLibrary must be a string"));
    tmp.testLibrary=root.value(QStringLiteral("testLibrary")).toString();
    if(root.contains(QStringLiteral("appearance"))) {
        if(!root.value(QStringLiteral("appearance")).isObject())
            return fail(error,QStringLiteral("Appearance must be an object"));
        const auto a=root.value(QStringLiteral("appearance")).toObject();
        if((a.contains(QStringLiteral("themeMode")) && !a.value(QStringLiteral("themeMode")).isString()) ||
           (a.contains(QStringLiteral("background")) && !a.value(QStringLiteral("background")).isString()) ||
           (a.contains(QStringLiteral("showDesignGrid")) && !a.value(QStringLiteral("showDesignGrid")).isBool()))
            return fail(error,QStringLiteral("Invalid appearance fields"));
        tmp.appearance.themeMode=a.value(QStringLiteral("themeMode")).toString(QStringLiteral("follow"));
        tmp.appearance.background=a.value(QStringLiteral("background")).toString();
        tmp.appearance.showDesignGrid=a.value(QStringLiteral("showDesignGrid")).toBool(true);
    }
    if(root.contains(QStringLiteral("pageBackground")) && !root.value(QStringLiteral("pageBackground")).isString())
        return fail(error,QStringLiteral("Invalid Page background"));
    if(version==1) {
        if(!root.value(QStringLiteral("controls")).isArray())
            return fail(error,QStringLiteral("Version 1 controls must be an array"));
        PanelPage& main=tmp.mainPage();
        main.controls=root.value(QStringLiteral("controls")).toArray();
        main.background=root.value(QStringLiteral("pageBackground")).toString();
        main.size.setWidth(qBound(320,root.value(QStringLiteral("canvasWidth")).toInt(900),8192));
        main.size.setHeight(qBound(240,root.value(QStringLiteral("canvasHeight")).toInt(550),8192));
        // Preserve old projects containing widgets past the former dimensions.
        for(const auto& v:main.controls) {
            const auto c=v.toObject();
            main.size.setWidth(qMax(main.size.width(),c.value(QStringLiteral("x")).toInt()+c.value(QStringLiteral("width")).toInt()));
            main.size.setHeight(qMax(main.size.height(),c.value(QStringLiteral("y")).toInt()+c.value(QStringLiteral("height")).toInt()));
        }
    } else {
        if(!root.value(QStringLiteral("pages")).isArray())
            return fail(error,QStringLiteral("Version 2 pages must be an array"));
        tmp.pages.clear();
        const QJsonArray all=root.value(QStringLiteral("pages")).toArray();
        for(const auto& value:all) {
            if(!value.isObject())return fail(error,QStringLiteral("Invalid Page definition"));
            const QJsonObject page=value.toObject();
            if(!page.value(QStringLiteral("id")).isString() ||
               !page.value(QStringLiteral("name")).isString() ||
               !page.value(QStringLiteral("controls")).isArray() ||
               !page.value(QStringLiteral("width")).isDouble() ||
               !page.value(QStringLiteral("height")).isDouble())
                return fail(error,QStringLiteral("Page is missing required fields"));
            PanelPage entry;
            entry.id=page.value(QStringLiteral("id")).toString();
            entry.name=page.value(QStringLiteral("name")).toString();
            entry.size=QSize(page.value(QStringLiteral("width")).toInt(),
                             page.value(QStringLiteral("height")).toInt());
            if(page.contains(QStringLiteral("background")) && !page.value(QStringLiteral("background")).isString())
                return fail(error,QStringLiteral("Invalid Page background field"));
            entry.controls=page.value(QStringLiteral("controls")).toArray();
            entry.background=page.value(QStringLiteral("background")).toString();
            tmp.pages.append(entry);
        }
        tmp.activePageId=root.value(QStringLiteral("activePageId")).toString();
        if(tmp.activePageId.isEmpty() && !tmp.pages.isEmpty())tmp.activePageId=tmp.pages.first().id;
    }
    if(root.contains(QStringLiteral("lifecycle"))) {
        if(!root.value(QStringLiteral("lifecycle")).isObject())
            return fail(error,QStringLiteral("Lifecycle settings must be an object"));
        const auto l=root.value(QStringLiteral("lifecycle")).toObject();
        for(const QString& key : {QStringLiteral("initializeEnabled"),QStringLiteral("shutdownEnabled")})
            if(l.contains(key) && !l.value(key).isBool())
                return fail(error,QStringLiteral("Lifecycle setting %1 must be boolean").arg(key));
        tmp.lifecycle.initializeEnabled=l.value(QStringLiteral("initializeEnabled")).toBool(false);
        tmp.lifecycle.shutdownEnabled=l.value(QStringLiteral("shutdownEnabled")).toBool(false);
    }
    tmp.m_extensions=root;
    for(const QString& key : {QStringLiteral("format"),QStringLiteral("version"),QStringLiteral("canvasWidth"),
             QStringLiteral("canvasHeight"),QStringLiteral("controls"),QStringLiteral("pages"),
             QStringLiteral("activePageId"),QStringLiteral("script"),QStringLiteral("scriptName"),
             QStringLiteral("libraries"),QStringLiteral("description"),QStringLiteral("lifecycle"),
             QStringLiteral("appearance"),QStringLiteral("pageBackground"),
             QStringLiteral("testDefinition"),QStringLiteral("testLibrary"),QStringLiteral("runInformation"),
             QStringLiteral("reportStorage")})
        tmp.m_extensions.remove(key);
    if(!tmp.validate(error))return false;
    *output=std::move(tmp);
    return true;
}
bool PanelProject::loadFile(const QString& name,PanelProject* output,QString* error) {
    QFile file(name);
    if(!file.open(QIODevice::ReadOnly))return fail(error,QStringLiteral("Unable to open panel: %1").arg(file.errorString()));
    if(file.size()>32*1024*1024)return fail(error,QStringLiteral("Panel exceeds 32 MiB"));
    QJsonParseError parse;
    const QJsonDocument doc=QJsonDocument::fromJson(file.readAll(),&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject())
        return fail(error,QStringLiteral("Invalid panel JSON: %1").arg(parse.errorString()));
    return fromJson(doc.object(),name,output,error);
}
bool PanelProject::toJson(QJsonObject* output,QString* error) const {
    if(!output)return fail(error,QStringLiteral("Missing output object"));
    if(!validate(error))return false;
    const bool legacy=pages.size()==1 && pages.first().id==QStringLiteral("main") &&
                      pages.first().name==QStringLiteral("Main") && activePageId==QStringLiteral("main");
    QJsonObject root=m_extensions;
    root.insert(QStringLiteral("format"),QStringLiteral("WFScriptPanel"));
    root.insert(QStringLiteral("version"),legacy?1:2);
    if(legacy) {
        const auto& page=pages.first();
        root.insert(QStringLiteral("canvasWidth"),page.size.width());
        root.insert(QStringLiteral("canvasHeight"),page.size.height());
        root.insert(QStringLiteral("controls"),page.controls);
        if(!page.background.isEmpty())root.insert(QStringLiteral("pageBackground"),page.background);
    } else {
        QJsonArray all;
        for(const auto& page:pages) {
            QJsonObject pageJson{{QStringLiteral("id"),page.id},
                {QStringLiteral("name"),page.name},
                {QStringLiteral("width"),page.size.width()},
                {QStringLiteral("height"),page.size.height()},
                {QStringLiteral("controls"),page.controls}};
            if(!page.background.isEmpty())pageJson.insert(QStringLiteral("background"),page.background);
            all.append(pageJson);
        }
        root.insert(QStringLiteral("pages"),all);
        root.insert(QStringLiteral("activePageId"),activePageId);
    }
    root.insert(QStringLiteral("script"),script);
    root.insert(QStringLiteral("scriptName"),scriptName);
    root.insert(QStringLiteral("libraries"),libraries);
    if(!runInformation.isEmpty())root.insert(QStringLiteral("runInformation"),runInformation);
    if(!reportStorage.isEmpty())root.insert(QStringLiteral("reportStorage"),reportStorage);
    if(!testDefinition.isEmpty()) {
        root.insert(QStringLiteral("testDefinition"),testDefinition);
        root.insert(QStringLiteral("testLibrary"),testLibrary);
    }
    root.insert(QStringLiteral("description"),description);
    if(appearance.themeMode!=QLatin1String("follow") || !appearance.background.isEmpty() || !appearance.showDesignGrid)
        root.insert(QStringLiteral("appearance"),QJsonObject{
            {QStringLiteral("themeMode"),appearance.themeMode},
            {QStringLiteral("background"),appearance.background},
            {QStringLiteral("showDesignGrid"),appearance.showDesignGrid}});
    if(lifecycle.initializeEnabled || lifecycle.shutdownEnabled)
        root.insert(QStringLiteral("lifecycle"),QJsonObject{
            {QStringLiteral("initializeEnabled"),lifecycle.initializeEnabled},
            {QStringLiteral("shutdownEnabled"),lifecycle.shutdownEnabled}});
    *output=root;
    return true;
}
bool PanelProject::saveFile(const QString& name,QString* error) const {
    QJsonObject root;
    if(!toJson(&root,error))return false;
    const QByteArray bytes=QJsonDocument(root).toJson(QJsonDocument::Indented);
    if(bytes.size()>32*1024*1024)return fail(error,QStringLiteral("Panel exceeds 32 MiB"));
    QSaveFile out(name);
    if(!out.open(QIODevice::WriteOnly))return fail(error,out.errorString());
    if(out.write(bytes)!=bytes.size() || !out.commit())return fail(error,out.errorString());
    return true;
}
