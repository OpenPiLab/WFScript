#include "TestSequenceEditor.h"
#include "RunInformationEditor.h"
#include "TestReportStorage.h"
#include <QDesktopServices>
#include <QUrl>
#include <QDir>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSet>
#include <algorithm>
#include <tuple>
#include <initializer_list>

namespace {
bool errorOut(QString* error,const QString& message) {
    if(error)*error=message;
    return false;
}
int groupRank(const QString& group) {
    if(group==QLatin1String("setup"))return 0;
    if(group==QLatin1String("main"))return 1;
    return 2;
}
QString jsonPretty(const QJsonObject& object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)).trimmed();
}
QComboBox* namedCombo(const QStringList& exports,QWidget* parent) {
    auto* combo=new QComboBox(parent);
    combo->setEditable(true);
    combo->addItem(QString());
    combo->addItems(exports);
    combo->setToolTip(QStringLiteral("Named exported function from a Panel .mjs library. Editable for advanced exports."));
    return combo;
}
// The Test Executive owns the policy, not a Panel widget or operator script.
// The default folder is Documents/WFScript Reports; custom paths are absolute.
class ReportStorageDialog final : public QDialog {
public:
    explicit ReportStorageDialog(const QJsonObject& settings,QWidget* parent=nullptr)
        :QDialog(parent) {
        setObjectName(QStringLiteral("reportStorageEditor"));
        setWindowTitle(QStringLiteral("Automatic Test Report Storage"));
        setMinimumWidth(590);
        auto* layout=new QVBoxLayout(this);
        auto* intro=new QLabel(QStringLiteral(
            "One project-owned report policy. After each terminal test result and Cleanup, "
            "the native TestRunner automatically archives a full JSON execution record. "
            "No JavaScript report-saving code is required."),this);
        intro->setWordWrap(true);layout->addWidget(intro);
        m_save=new QCheckBox(QStringLiteral("Automatically save JSON reports after every test"),this);
        m_save->setObjectName(QStringLiteral("autoArchiveCheck"));
        m_save->setChecked(TestReportStorage::enabled(settings));layout->addWidget(m_save);
        layout->addWidget(new QLabel(QStringLiteral("Reports directory (leave blank for Documents/WFScript Reports)"),this));
        auto* locationRow=new QHBoxLayout;
        m_directory=new QLineEdit(this);
        m_directory->setObjectName(QStringLiteral("autoArchiveDirectory"));
        m_directory->setPlaceholderText(QStringLiteral("Default: Documents/WFScript Reports"));
        m_directory->setText(settings.value(QStringLiteral("directory")).toString());
        locationRow->addWidget(m_directory,1);
        auto* browse=new QPushButton(QStringLiteral("Browse..."),this);
        locationRow->addWidget(browse);layout->addLayout(locationRow);
        m_actual=new QLabel(this);m_actual->setWordWrap(true);
        layout->addWidget(m_actual);
        auto* show=new QPushButton(QStringLiteral("Open Results Folder"),this);
        show->setObjectName(QStringLiteral("openArchiveDirectory"));
        layout->addWidget(show);
        auto* note=new QLabel(QStringLiteral(
            "PASS, FAIL, ERROR and ABORTED runs are saved as separate files using "
            "UTC time, UUT, status and a unique execution ID. "
            "If a file cannot be saved, the console and Run Report display an error; "
            "the test verdict is never changed. Old reports are not deleted or overwritten."),this);
        note->setWordWrap(true);layout->addWidget(note);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
        buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Save Policy"));
        layout->addWidget(buttons);
        connect(browse,&QPushButton::clicked,this,[this]{
            const QString current=TestReportStorage::directory(currentSettings());
            const QString folder=QFileDialog::getExistingDirectory(this,QStringLiteral("Choose Reports Folder"),current);
            if(!folder.isEmpty())m_directory->setText(folder);
        });
        connect(m_directory,&QLineEdit::textChanged,this,[this]{refreshPath();});
        connect(show,&QPushButton::clicked,this,[this]{
            const QString path=TestReportStorage::directory(currentSettings());
            if(path.isEmpty()){QMessageBox::warning(this,"Reports folder","No Documents folder is available. Select a path.");return;}
            if(!QDir(path).exists()){
                QMessageBox::information(this,"Reports folder",
                    QStringLiteral("The folder does not exist yet. It will be created after the next completed run:\n%1").arg(path));
                return;
            }
            if(!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                QMessageBox::warning(this,"Reports folder",QStringLiteral("Unable to open: %1").arg(path));
        });
        connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,this,[this]{
            QString error;
            if(!TestReportStorage::validate(currentSettings(),&error)){
                QMessageBox::warning(this,"Invalid report storage policy",error);return;
            }
            accept();
        });
        refreshPath();
    }
    QJsonObject settings() const {return currentSettings();}
private:
    QCheckBox* m_save=nullptr;
    QLineEdit* m_directory=nullptr;
    QLabel* m_actual=nullptr;
    QJsonObject currentSettings() const {
        return QJsonObject{{QStringLiteral("autoSave"),m_save->isChecked()},
                           {QStringLiteral("directory"),m_directory->text().trimmed()}};
    }
    void refreshPath() {m_actual->setText(QStringLiteral("Resolved directory: %1").arg(TestReportStorage::directory(currentSettings())));}
};
}

bool TestSequenceEditor::extractModuleDefinition(const QString& source,QJsonObject* out,
                                                 int* begin,int* end,QString* error) {
    if(!out)return errorOut(error,QStringLiteral("Missing definition destination"));
    // Fail closed rather than trying to evaluate arbitrary JavaScript source.
    static const QRegularExpression assignment(QStringLiteral(
        R"((?m)^\s*export\s+const\s+definition\s*=\s*)"));
    const auto match=assignment.match(source);
    if(!match.hasMatch())
        return errorOut(error,QStringLiteral("Expected a literal 'export const definition = { ... }' in TestStationSequence.mjs"));
    int pos=match.capturedEnd();
    while(pos<source.size() && source[pos].isSpace())++pos;
    if(pos>=source.size() || source[pos]!=QLatin1Char('{'))
        return errorOut(error,QStringLiteral("The exported definition must be a literal JSON object (not a function call)"));
    const int start=pos;
    bool inString=false,escaped=false;
    int depth=0,finish=-1;
    for(;pos<source.size();++pos) {
        const QChar c=source[pos];
        if(inString) {
            if(escaped){escaped=false;continue;}
            if(c==QLatin1Char('\\')){escaped=true;continue;}
            if(c==QLatin1Char('"'))inString=false;
            continue;
        }
        if(c==QLatin1Char('"')){inString=true;continue;}
        if(c==QLatin1Char('{'))++depth;
        if(c==QLatin1Char('}')){
            if(--depth==0){finish=pos+1;break;}
        }
    }
    if(finish<0)return errorOut(error,QStringLiteral("Unterminated definition object"));
    QJsonParseError parse;
    const auto document=QJsonDocument::fromJson(source.mid(start,finish-start).toUtf8(),&parse);
    if(parse.error!=QJsonParseError::NoError || !document.isObject())
        return errorOut(error,QStringLiteral("The embedded definition is not strict JSON: %1. Use quoted keys/strings and no comments inside the object.").arg(parse.errorString()));
    *out=document.object();
    if(begin)*begin=start;
    if(end)*end=finish;
    return true;
}

bool TestSequenceEditor::replaceModuleDefinition(const QString& source,const QJsonObject& document,
                                                 QString* result,QString* error) {
    if(!result)return errorOut(error,QStringLiteral("Missing output destination"));
    if(!validateDefinition(document,error))return false;
    QJsonObject previous;
    int start=-1,end=-1;
    if(!extractModuleDefinition(source,&previous,&start,&end,error))return false;
    QString replaced=source;
    replaced.replace(start,end-start,jsonPretty(document));
    *result=replaced;
    return true;
}

bool TestSequenceEditor::validateDefinition(const QJsonObject& doc,QString* error) {
    static const QRegularExpression definitionId(QStringLiteral("^[A-Za-z][A-Za-z0-9_-]{0,63}$"));
    const QSet<QString> topKeys={QStringLiteral("schemaVersion"),QStringLiteral("id"),
        QStringLiteral("title"),QStringLiteral("config"),QStringLiteral("steps")};
    for(auto it=doc.begin();it!=doc.end();++it)
        if(!topKeys.contains(it.key()))
            return errorOut(error,QStringLiteral("Unsupported definition property: %1").arg(it.key()));
    if(doc.value(QStringLiteral("schemaVersion")).toInt(-1)!=1)
        return errorOut(error,QStringLiteral("Definition schemaVersion must be 1"));
    const QString id=doc.value(QStringLiteral("id")).toString();
    if(!definitionId.match(id).hasMatch())
        return errorOut(error,QStringLiteral("Definition ID must begin with an ASCII letter and contain only letters, digits, - or _ (max 64)"));
    const QString title=doc.value(QStringLiteral("title")).toString().trimmed();
    if(title.isEmpty() || title.size()>120)
        return errorOut(error,QStringLiteral("Test title must contain 1..120 characters"));
    if(!doc.value(QStringLiteral("config")).isObject())
        return errorOut(error,QStringLiteral("Missing config object"));
    const QJsonObject config=doc.value(QStringLiteral("config")).toObject();
    const QSet<QString> configKeys={QStringLiteral("title"),QStringLiteral("sequenceId"),QStringLiteral("tableId"),
        QStringLiteral("logId"),QStringLiteral("summaryId"),QStringLiteral("stopOnFailure"),
        QStringLiteral("totalTimeoutMs")};
    for(auto it=config.begin();it!=config.end();++it)
        if(!configKeys.contains(it.key()))
            return errorOut(error,QStringLiteral("Unsupported config property: %1").arg(it.key()));
    if(config.contains(QStringLiteral("title")) && config.value(QStringLiteral("title")).toString()!=title)
        return errorOut(error,QStringLiteral("config.title must match the definition title"));
    for(const QString& field:{QStringLiteral("sequenceId"),QStringLiteral("tableId"),QStringLiteral("logId"),QStringLiteral("summaryId")}) {
        const auto v=config.value(field);
        if(!v.isUndefined() && (!v.isString() || v.toString().size()>100))
            return errorOut(error,QStringLiteral("Invalid widget ID in config.%1").arg(field));
    }
    if(config.contains(QStringLiteral("stopOnFailure")) && !config.value(QStringLiteral("stopOnFailure")).isBool())
        return errorOut(error,QStringLiteral("stopOnFailure must be a boolean"));
    auto integer=[](QJsonValue v,int low,int high) {
        if(!v.isDouble())return false;
        const double d=v.toDouble();return d>=low && d<=high && d==static_cast<double>(static_cast<int>(d));
    };
    if(config.contains(QStringLiteral("totalTimeoutMs")) && !integer(config.value(QStringLiteral("totalTimeoutMs")),0,86400000))
        return errorOut(error,QStringLiteral("totalTimeoutMs must be an integer 0..86400000"));
    if(!doc.value(QStringLiteral("steps")).isArray())return errorOut(error,QStringLiteral("Missing steps array"));
    const QJsonArray steps=doc.value(QStringLiteral("steps")).toArray();
    if(steps.isEmpty() || steps.size()>100)return errorOut(error,QStringLiteral("A sequence must contain 1..100 steps"));
    QSet<QString> ids;
    static const QRegularExpression name(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
    for(int i=0;i<steps.size();++i){
        if(!steps[i].isObject())return errorOut(error,QStringLiteral("Step %1 is not an object").arg(i+1));
        const QJsonObject step=steps[i].toObject();
        const QSet<QString> stepKeys={QStringLiteral("id"),QStringLiteral("name"),
            QStringLiteral("description"),QStringLiteral("group"),QStringLiteral("type"),
            QStringLiteral("mode"),QStringLiteral("timeoutMs"),QStringLiteral("intervalMs"),
            QStringLiteral("durationMs"),QStringLiteral("handler"),QStringLiteral("handlers"),
            QStringLiteral("limits"),QStringLiteral("unit"),QStringLiteral("resultWidget")};
        for(auto it=step.begin();it!=step.end();++it)
            if(!stepKeys.contains(it.key()))
                return errorOut(error,QStringLiteral("Step %1 contains unsupported property %2")
                    .arg(i+1).arg(it.key()));
        const QString id=step.value(QStringLiteral("id")).toString().trimmed();
        const QString label=step.value(QStringLiteral("name")).toString().trimmed();
        if(id.isEmpty()||id.size()>64||ids.contains(id))return errorOut(error,QStringLiteral("Step %1 has a missing or duplicate ID").arg(i+1));
        ids.insert(id);
        if(label.isEmpty()||label.size()>120||step.value(QStringLiteral("description")).toString().size()>320)
            return errorOut(error,QStringLiteral("Step %1 has invalid name or description length").arg(i+1));
        const QString group=step.value(QStringLiteral("group")).toString(QStringLiteral("main"));
        const QString mode=step.value(QStringLiteral("mode")).toString(QStringLiteral("once"));
        const QString type=step.value(QStringLiteral("type")).toString(QStringLiteral("test"));
        if(!QStringList{QStringLiteral("setup"),QStringLiteral("main"),QStringLiteral("cleanup")}.contains(group) ||
           !QStringList{QStringLiteral("test"),QStringLiteral("action"),QStringLiteral("numeric")}.contains(type) ||
           !QStringList{QStringLiteral("once"),QStringLiteral("poll"),QStringLiteral("delay")}.contains(mode))
            return errorOut(error,QStringLiteral("Step %1 has an unsupported group, type, or execution mode").arg(i+1));
        if(type==QStringLiteral("numeric")) {
            if(mode==QStringLiteral("delay"))return errorOut(error,QStringLiteral("Step %1: numeric steps cannot be Delay; use an Action delay followed by a numeric test").arg(i+1));
            const QJsonObject limits=step.value(QStringLiteral("limits")).toObject();
            if(!limits.value(QStringLiteral("low")).isDouble() ||
               !limits.value(QStringLiteral("high")).isDouble() ||
               limits.value(QStringLiteral("low")).toDouble()>limits.value(QStringLiteral("high")).toDouble())
                return errorOut(error,QStringLiteral("Step %1: numeric limits require low <= high").arg(i+1));
        }
        for(const auto& item:std::initializer_list<std::tuple<QString,int,int>>{
            {QStringLiteral("timeoutMs"),0,86400000},{QStringLiteral("durationMs"),0,86400000},
            {QStringLiteral("intervalMs"),10,60000}}) {
            const QString key=std::get<0>(item);
            if(step.contains(key) && !integer(step.value(key),std::get<1>(item),std::get<2>(item)))
                return errorOut(error,QStringLiteral("Step %1: invalid %2").arg(i+1).arg(key));
        }
        if(mode==QStringLiteral("delay") && step.value(QStringLiteral("durationMs")).toInt()<=0)
            return errorOut(error,QStringLiteral("Step %1: delay duration must be positive").arg(i+1));
        if(step.contains(QStringLiteral("handler")) && step.contains(QStringLiteral("handlers")))
            return errorOut(error,QStringLiteral("Step %1 has both handler and handlers").arg(i+1));
        if(step.contains(QStringLiteral("handler")) &&
           (!step.value(QStringLiteral("handler")).isString() || mode!=QStringLiteral("once")))
            return errorOut(error,QStringLiteral("Step %1: handler is only for once steps").arg(i+1));
        if(step.contains(QStringLiteral("handlers")) && !step.value(QStringLiteral("handlers")).isObject())
            return errorOut(error,QStringLiteral("Step %1: handlers must be an object").arg(i+1));
        const QString handler=step.value(QStringLiteral("handler")).toString();
        const QJsonObject handlers=step.value(QStringLiteral("handlers")).toObject();
        if(mode==QStringLiteral("once") && !name.match(handler).hasMatch())
            return errorOut(error,QStringLiteral("Step %1: once requires a named exported function").arg(i+1));
        if(mode==QStringLiteral("poll") && !name.match(handlers.value(QStringLiteral("poll")).toString()).hasMatch())
            return errorOut(error,QStringLiteral("Step %1: poll requires handlers.poll").arg(i+1));
        for(auto it=handlers.begin();it!=handlers.end();++it)
            if(!QStringList{QStringLiteral("start"),QStringLiteral("poll"),QStringLiteral("finish")}.contains(it.key()) ||
               !name.match(it.value().toString()).hasMatch())
                return errorOut(error,QStringLiteral("Step %1 has an invalid handler name").arg(i+1));
    }
    return true;
}

TestSequenceEditor::TestSequenceEditor(const QJsonObject& definition,const QJsonObject& runInformation,
                                       const QStringList& namedExports,QWidget* parent,
                                       const QJsonObject& reportStorage)
    :QDialog(parent),m_definition(definition),m_runInformation(runInformation),
     m_reportStorage(reportStorage),m_exports(namedExports) {
    setWindowTitle(QStringLiteral("Visual Test Sequence Editor — WFScript v5.6.67"));
    setObjectName(QStringLiteral("visualTestSequenceEditor"));
    resize(1130,740);
    setMinimumSize(860,550);
    auto* outer=new QVBoxLayout(this);
    auto* notice=new QLabel(QStringLiteral(
        "Edit the data-only test definition embedded in this Panel. Test functions stay in the reusable .mjs library. "
        "New steps go into Main, before Cleanup. Use Group to move a step to Setup or Cleanup. "
        "No script or hardware executes while editing."),this);
    notice->setWordWrap(true);outer->addWidget(notice);

    auto* runInfo=new QPushButton(QStringLiteral("Run Information... (UUT fields)"),this);
    runInfo->setObjectName(QStringLiteral("editRunInformationButton"));
    runInfo->setToolTip(QStringLiteral("Define the serial number, operator, part number, choices and required fields shared by all Run Information widgets on the Panel."));
    outer->addWidget(runInfo);
    auto* runInfoHelp=new QLabel(QStringLiteral(
        "These fields belong to THIS Panel project and are used by its TestRunner. "
        "Place any number of 'Run Information (UUT)' widgets on your Pages: they all "
        "show the same pending values; RUN/NEXT captures a frozen copy. "
        "No widget-ID binding or JavaScript metadata configuration is needed."),this);
    runInfoHelp->setObjectName(QStringLiteral("runInformationProjectLinkHelp"));
    runInfoHelp->setWordWrap(true);
    outer->addWidget(runInfoHelp);
    connect(runInfo,&QPushButton::clicked,this,[this]{
        RunInformationEditor editor(m_runInformation,this);
        if(editor.exec()==QDialog::Accepted)m_runInformation=editor.schema();
    });
    auto* reportButton=new QPushButton(QStringLiteral("Report Storage... (automatic JSON archiving)"),this);
    reportButton->setObjectName(QStringLiteral("editReportStorageButton"));
    reportButton->setToolTip(QStringLiteral("Configure the folder and automatic-save policy for completed test reports in this project."));
    outer->addWidget(reportButton);
    connect(reportButton,&QPushButton::clicked,this,[this]{
        ReportStorageDialog editor(m_reportStorage,this);
        if(editor.exec()==QDialog::Accepted)m_reportStorage=editor.settings();
    });
    auto* header=new QGroupBox(QStringLiteral("Sequence settings"),this);
    auto* headers=new QGridLayout(header);
    m_id=new QLineEdit(header);m_title=new QLineEdit(header);
    m_sequenceId=new QLineEdit(header);m_tableId=new QLineEdit(header);
    m_logId=new QLineEdit(header);m_summaryId=new QLineEdit(header);
    m_totalTimeout=new QSpinBox(header);m_totalTimeout->setRange(0,86400000);m_totalTimeout->setSuffix(QStringLiteral(" ms"));
    m_stopOnFailure=new QCheckBox(QStringLiteral("Stop Main tests on failure (Cleanup still runs)"),header);
    headers->addWidget(new QLabel("Definition ID"),0,0);headers->addWidget(m_id,0,1);
    headers->addWidget(new QLabel("Test title"),0,2);headers->addWidget(m_title,0,3);
    headers->addWidget(new QLabel("Total timeout"),1,0);headers->addWidget(m_totalTimeout,1,1);
    headers->addWidget(m_stopOnFailure,1,2,1,2);
    headers->addWidget(new QLabel("Sequence widget"),2,0);headers->addWidget(m_sequenceId,2,1);
    headers->addWidget(new QLabel("Results table"),2,2);headers->addWidget(m_tableId,2,3);
    headers->addWidget(new QLabel("Event log"),3,0);headers->addWidget(m_logId,3,1);
    headers->addWidget(new QLabel("Summary widget"),3,2);headers->addWidget(m_summaryId,3,3);
    outer->addWidget(header);

    auto* splitter=new QSplitter(Qt::Horizontal,this);
    auto* left=new QWidget(splitter);auto* l=new QVBoxLayout(left);l->setContentsMargins(0,0,0,0);
    m_table=new QTableWidget(left);m_table->setObjectName(QStringLiteral("testSequenceStepTable"));
    m_table->setColumnCount(5);m_table->setHorizontalHeaderLabels({"Group","Step name","Type","Mode","Implementation"});
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    l->addWidget(m_table,1);
    auto* controls=new QHBoxLayout;
    auto* add=new QPushButton(QStringLiteral("+ Add"),left);
    auto* remove=new QPushButton(QStringLiteral("Remove"),left);
    auto* up=new QPushButton(QStringLiteral("Move Up"),left);
    auto* down=new QPushButton(QStringLiteral("Move Down"),left);
    add->setObjectName(QStringLiteral("addTestStepButton"));
    remove->setObjectName(QStringLiteral("removeTestStepButton"));
    up->setObjectName(QStringLiteral("moveTestStepUpButton"));
    down->setObjectName(QStringLiteral("moveTestStepDownButton"));
    controls->addWidget(add);controls->addWidget(remove);controls->addWidget(up);controls->addWidget(down);
    l->addLayout(controls);
    splitter->addWidget(left);

    auto* rightScroll=new QScrollArea(splitter);rightScroll->setWidgetResizable(true);
    auto* right=new QWidget(rightScroll);auto* form=new QFormLayout(right);
    m_stepId=new QLineEdit(right);m_stepName=new QLineEdit(right);m_description=new QLineEdit(right);
    m_group=new QComboBox(right);m_group->setObjectName(QStringLiteral("testSequenceGroupCombo"));m_group->addItems({"setup","main","cleanup"});
    m_type=new QComboBox(right);m_type->addItems({"action","test","numeric"});
    m_mode=new QComboBox(right);m_mode->addItems({"once","poll","delay"});
    m_handler=namedCombo(m_exports,right);m_start=namedCombo(m_exports,right);
    m_poll=namedCombo(m_exports,right);m_finish=namedCombo(m_exports,right);
    m_timeout=new QSpinBox(right);m_timeout->setRange(0,86400000);m_timeout->setSuffix(QStringLiteral(" ms"));
    m_interval=new QSpinBox(right);m_interval->setRange(10,60000);m_interval->setValue(50);m_interval->setSuffix(QStringLiteral(" ms"));
    m_duration=new QSpinBox(right);m_duration->setRange(0,86400000);m_duration->setSuffix(QStringLiteral(" ms"));
    form->addRow("Step ID",m_stepId);form->addRow("Display name",m_stepName);
    form->addRow("Description",m_description);form->addRow("Group",m_group);
    form->addRow("Step type",m_type);form->addRow("Execution mode",m_mode);
    form->addRow("Once handler",m_handler);form->addRow("On start",m_start);
    form->addRow("Polling handler",m_poll);form->addRow("On finish",m_finish);
    form->addRow("Step timeout",m_timeout);form->addRow("Poll interval",m_interval);
    form->addRow("Delay duration",m_duration);
    m_limitLow=new QDoubleSpinBox(right);m_limitHigh=new QDoubleSpinBox(right);
    for(QDoubleSpinBox* spin:{m_limitLow,m_limitHigh}) {
        spin->setRange(-1e12,1e12);spin->setDecimals(6);
    }
    m_numericUnit=new QLineEdit(right);m_numericWidget=new QLineEdit(right);
    form->addRow("Numeric lower limit",m_limitLow);
    form->addRow("Numeric upper limit",m_limitHigh);
    form->addRow("Units",m_numericUnit);
    form->addRow("Measurement widget ID",m_numericWidget);
    auto* apply=new QPushButton(QStringLiteral("Apply Step Changes"),right);apply->setObjectName(QStringLiteral("applyTestStepButton"));form->addRow(apply);
    m_hint=new QLabel(QStringLiteral("Once: handler returns a result. Poll: callback returns undefined until finished. Delay: Qt waits without calling JavaScript."),right);
    m_hint->setWordWrap(true);form->addRow(m_hint);
    rightScroll->setWidget(right);splitter->addWidget(rightScroll);
    splitter->setStretchFactor(0,3);splitter->setStretchFactor(1,2);
    outer->addWidget(splitter,1);
    m_preview=new QPlainTextEdit(this);m_preview->setObjectName(QStringLiteral("testDefinitionJsonPreview"));
    m_preview->setReadOnly(true);m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_preview->setMaximumHeight(130);m_preview->setPlaceholderText(QStringLiteral("Definition JSON preview"));
    outer->addWidget(m_preview);
    auto* footer=new QHBoxLayout;
    auto* load=new QPushButton(QStringLiteral("Import .wftest.json..."),this);
    auto* exportButton=new QPushButton(QStringLiteral("Export .wftest.json..."),this);
    auto* preview=new QPushButton(QStringLiteral("Refresh JSON Preview"),this);
    footer->addWidget(load);footer->addWidget(exportButton);footer->addWidget(preview);footer->addStretch();
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    actions->button(QDialogButtonBox::Save)->setText(QStringLiteral("Save to Panel"));
    footer->addWidget(actions);outer->addLayout(footer);
    connect(add,&QPushButton::clicked,this,[this]{addStep();});
    connect(remove,&QPushButton::clicked,this,[this]{removeStep();});
    connect(up,&QPushButton::clicked,this,[this]{moveStep(-1);});
    connect(down,&QPushButton::clicked,this,[this]{moveStep(1);});
    connect(apply,&QPushButton::clicked,this,[this]{QString error;if(!applyStep(&error))QMessageBox::warning(this,"Invalid step",error);else showPreview();});
    connect(m_table,&QTableWidget::currentCellChanged,this,[this](int row,int,int,int){
        if(m_loading)return;
        // Capture the clicked step before applying edits, which may relocate
        // the previously selected step into a different execution group.
        const QString targetId=(row>=0 && row<m_steps.size())
            ?m_steps.at(row).value(QStringLiteral("id")).toString():QString();
        QString error;
        if(!applyStep(&error)){
            QMessageBox::warning(this,"Invalid step",error);
            m_loading=true;m_table->selectRow(m_selected);m_loading=false;return;
        }
        int target=row;
        if(!targetId.isEmpty())for(int i=0;i<m_steps.size();++i)
            if(m_steps.at(i).value(QStringLiteral("id")).toString()==targetId){target=i;break;}
        if(target>=0 && target<m_steps.size()) {
            m_loading=true;m_table->setCurrentCell(target,1);m_loading=false;
            loadStep(target);
        }
    });
    connect(m_mode,&QComboBox::currentTextChanged,this,[this]{setModeVisibility();});
    connect(m_type,&QComboBox::currentTextChanged,this,[this]{setModeVisibility();});
    connect(actions,&QDialogButtonBox::accepted,this,[this]{acceptChanges();});
    connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(load,&QPushButton::clicked,this,[this]{importJson();});
    connect(exportButton,&QPushButton::clicked,this,[this]{exportJson();});
    connect(preview,&QPushButton::clicked,this,[this]{showPreview();});
    loadDocument();
}

void TestSequenceEditor::loadDocument() {
    m_loading=true;
    m_id->setText(m_definition.value(QStringLiteral("id")).toString());
    m_title->setText(m_definition.value(QStringLiteral("title")).toString());
    const QJsonObject cfg=m_definition.value(QStringLiteral("config")).toObject();
    m_sequenceId->setText(cfg.value(QStringLiteral("sequenceId")).toString());
    m_tableId->setText(cfg.value(QStringLiteral("tableId")).toString());
    m_logId->setText(cfg.value(QStringLiteral("logId")).toString());
    m_summaryId->setText(cfg.value(QStringLiteral("summaryId")).toString());
    m_totalTimeout->setValue(cfg.value(QStringLiteral("totalTimeoutMs")).toInt());
    m_stopOnFailure->setChecked(cfg.value(QStringLiteral("stopOnFailure")).toBool(true));
    m_steps.clear();
    for(const auto& step:m_definition.value(QStringLiteral("steps")).toArray())m_steps.append(step.toObject());
    // The executive always runs Setup, Main, Cleanup. Show that same order in
    // the editor, including Panels saved by the earlier editor that appended
    // Main steps after Cleanup. Stable sort retains relative order per group.
    std::stable_sort(m_steps.begin(),m_steps.end(),[](const QJsonObject& a,const QJsonObject& b){
        return groupRank(a.value(QStringLiteral("group")).toString(QStringLiteral("main")))<
               groupRank(b.value(QStringLiteral("group")).toString(QStringLiteral("main")));
    });
    m_loading=false;
    populateTable(0);
    showPreview();
}
void TestSequenceEditor::populateTable(int selected) {
    m_loading=true;m_table->setRowCount(m_steps.size());
    for(int i=0;i<m_steps.size();++i){
        const auto& step=m_steps[i];
        const auto handler=step.value(QStringLiteral("handler")).toString();
        const auto refs=step.value(QStringLiteral("handlers")).toObject();
        const QStringList values={step.value(QStringLiteral("group")).toString(QStringLiteral("main")),
            step.value(QStringLiteral("name")).toString(),step.value(QStringLiteral("type")).toString(QStringLiteral("test")),
            step.value(QStringLiteral("mode")).toString(QStringLiteral("once")),
            handler.isEmpty()?refs.value(QStringLiteral("poll")).toString():handler};
        for(int j=0;j<values.size();++j){
            auto* item=new QTableWidgetItem(values[j]);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
            m_table->setItem(i,j,item);
        }
    }
    if(selected>=0 && selected<m_steps.size()) {
        m_table->setCurrentCell(selected,1);
        m_loading=false;
        loadStep(selected);
    } else {m_loading=false;m_selected=-1;}
}
void TestSequenceEditor::loadStep(int row) {
    if(row<0||row>=m_steps.size()){m_selected=-1;return;}
    m_loading=true;m_selected=row;
    const auto step=m_steps[row];
    m_stepId->setText(step.value(QStringLiteral("id")).toString());
    m_stepName->setText(step.value(QStringLiteral("name")).toString());
    m_description->setText(step.value(QStringLiteral("description")).toString());
    m_group->setCurrentText(step.value(QStringLiteral("group")).toString(QStringLiteral("main")));
    m_type->setCurrentText(step.value(QStringLiteral("type")).toString(QStringLiteral("test")));
    m_mode->setCurrentText(step.value(QStringLiteral("mode")).toString(QStringLiteral("once")));
    const QJsonObject handlers=step.value(QStringLiteral("handlers")).toObject();
    m_handler->setCurrentText(step.value(QStringLiteral("handler")).toString());
    m_start->setCurrentText(handlers.value(QStringLiteral("start")).toString());
    m_poll->setCurrentText(handlers.value(QStringLiteral("poll")).toString());
    m_finish->setCurrentText(handlers.value(QStringLiteral("finish")).toString());
    m_timeout->setValue(step.value(QStringLiteral("timeoutMs")).toInt());
    m_interval->setValue(step.value(QStringLiteral("intervalMs")).toInt(50));
    m_duration->setValue(step.value(QStringLiteral("durationMs")).toInt());
    const QJsonObject limits=step.value(QStringLiteral("limits")).toObject();
    m_limitLow->setValue(limits.value(QStringLiteral("low")).toDouble());
    m_limitHigh->setValue(limits.value(QStringLiteral("high")).toDouble());
    m_numericUnit->setText(step.value(QStringLiteral("unit")).toString());
    m_numericWidget->setText(step.value(QStringLiteral("resultWidget")).toString());
    setModeVisibility();m_loading=false;
}
void TestSequenceEditor::setModeVisibility() {
    const QString mode=m_mode->currentText();
    m_handler->setEnabled(mode==QLatin1String("once"));
    m_poll->setEnabled(mode==QLatin1String("poll"));
    m_start->setEnabled(mode!=QLatin1String("once"));
    m_finish->setEnabled(mode!=QLatin1String("once"));
    m_interval->setEnabled(mode==QLatin1String("poll"));
    m_duration->setEnabled(mode==QLatin1String("delay"));
    const bool numeric=m_type->currentText()==QLatin1String("numeric");
    m_limitLow->setEnabled(numeric);m_limitHigh->setEnabled(numeric);
    m_numericUnit->setEnabled(numeric);m_numericWidget->setEnabled(numeric);
}
bool TestSequenceEditor::applyStep(QString* error) {
    if(m_selected<0 || m_selected>=m_steps.size())return true;
    QJsonObject step=m_steps[m_selected];
    step.insert(QStringLiteral("id"),m_stepId->text().trimmed());
    step.insert(QStringLiteral("name"),m_stepName->text().trimmed());
    step.insert(QStringLiteral("description"),m_description->text());
    step.insert(QStringLiteral("group"),m_group->currentText());
    step.insert(QStringLiteral("type"),m_type->currentText());
    step.insert(QStringLiteral("mode"),m_mode->currentText());
    if(m_type->currentText()==QLatin1String("numeric")) {
        if(m_limitLow->value()>m_limitHigh->value())
            return errorOut(error,QStringLiteral("Numeric lower limit must be <= upper limit"));
        step.insert(QStringLiteral("limits"),QJsonObject{
            {QStringLiteral("low"),m_limitLow->value()},
            {QStringLiteral("high"),m_limitHigh->value()}});
        step.insert(QStringLiteral("unit"),m_numericUnit->text().trimmed());
        step.insert(QStringLiteral("resultWidget"),m_numericWidget->text().trimmed());
    } else {
        step.remove(QStringLiteral("limits"));step.remove(QStringLiteral("unit"));
        step.remove(QStringLiteral("resultWidget"));
    }
    step.remove(QStringLiteral("handler"));step.remove(QStringLiteral("handlers"));
    step.remove(QStringLiteral("intervalMs"));step.remove(QStringLiteral("durationMs"));
    if(m_timeout->value()>0)step.insert(QStringLiteral("timeoutMs"),m_timeout->value());
    else step.remove(QStringLiteral("timeoutMs"));
    if(m_mode->currentText()==QLatin1String("once")) {
        if(m_handler->currentText().trimmed().isEmpty())return errorOut(error,QStringLiteral("Once steps require an exported handler name"));
        step.insert(QStringLiteral("handler"),m_handler->currentText().trimmed());
    } else {
        QJsonObject handlers;
        if(!m_start->currentText().trimmed().isEmpty())handlers.insert(QStringLiteral("start"),m_start->currentText().trimmed());
        if(!m_finish->currentText().trimmed().isEmpty())handlers.insert(QStringLiteral("finish"),m_finish->currentText().trimmed());
        if(m_mode->currentText()==QLatin1String("poll")) {
            if(m_poll->currentText().trimmed().isEmpty())return errorOut(error,QStringLiteral("Poll steps require a polling handler"));
            handlers.insert(QStringLiteral("poll"),m_poll->currentText().trimmed());
            step.insert(QStringLiteral("intervalMs"),m_interval->value());
        } else {
            if(m_duration->value()==0)return errorOut(error,QStringLiteral("Delay duration must exceed zero"));
            step.insert(QStringLiteral("durationMs"),m_duration->value());
        }
        if(!handlers.isEmpty())step.insert(QStringLiteral("handlers"),handlers);
    }
    const QString oldGroup=m_steps[m_selected].value(QStringLiteral("group")).toString(QStringLiteral("main"));
    const QString newGroup=step.value(QStringLiteral("group")).toString(QStringLiteral("main"));
    if(oldGroup!=newGroup) {
        // A group edit is a move: append inside that group, not after Cleanup.
        m_steps.removeAt(m_selected);
        int target=0;
        while(target<m_steps.size() &&
              groupRank(m_steps.at(target).value(QStringLiteral("group")).toString(QStringLiteral("main")))<=groupRank(newGroup))
            ++target;
        m_steps.insert(target,step);
        populateTable(target);
        return true;
    }
    m_steps[m_selected]=step;
    m_loading=true;
    const QStringList values={step.value(QStringLiteral("group")).toString(),
        step.value(QStringLiteral("name")).toString(),step.value(QStringLiteral("type")).toString(),
        step.value(QStringLiteral("mode")).toString(),
        step.value(QStringLiteral("handler")).toString(step.value(QStringLiteral("handlers")).toObject().value(QStringLiteral("poll")).toString())};
    for(int i=0;i<values.size();++i)m_table->item(m_selected,i)->setText(values[i]);
    m_loading=false;
    return true;
}
bool TestSequenceEditor::collectDocument(QJsonObject* out,QString* error) {
    if(!applyStep(error))return false;
    QJsonObject doc=m_definition;
    doc.insert(QStringLiteral("id"),m_id->text().trimmed());
    doc.insert(QStringLiteral("title"),m_title->text().trimmed());
    QJsonObject cfg=doc.value(QStringLiteral("config")).toObject();
    if(cfg.contains(QStringLiteral("title")))cfg.insert(QStringLiteral("title"),m_title->text().trimmed());
    cfg.insert(QStringLiteral("stopOnFailure"),m_stopOnFailure->isChecked());
    cfg.insert(QStringLiteral("totalTimeoutMs"),m_totalTimeout->value());
    cfg.insert(QStringLiteral("sequenceId"),m_sequenceId->text().trimmed());
    cfg.insert(QStringLiteral("tableId"),m_tableId->text().trimmed());
    cfg.insert(QStringLiteral("logId"),m_logId->text().trimmed());
    cfg.insert(QStringLiteral("summaryId"),m_summaryId->text().trimmed());
    doc.insert(QStringLiteral("config"),cfg);
    auto ordered=m_steps;
    std::stable_sort(ordered.begin(),ordered.end(),[](const QJsonObject& a,const QJsonObject& b){
        return groupRank(a.value(QStringLiteral("group")).toString())<groupRank(b.value(QStringLiteral("group")).toString());
    });
    QJsonArray array;for(const auto& step:ordered)array.append(step);
    doc.insert(QStringLiteral("steps"),array);
    if(!validateDefinition(doc,error))return false;
    if(out)*out=doc;
    return true;
}
void TestSequenceEditor::showPreview() {
    QJsonObject current;QString error;
    if(collectDocument(&current,&error))m_preview->setPlainText(jsonPretty(current));
    else m_preview->setPlainText(QStringLiteral("Invalid definition: ")+error);
}
void TestSequenceEditor::acceptChanges() {
    QJsonObject edited;QString error;
    if(!collectDocument(&edited,&error)){QMessageBox::warning(this,"Cannot save definition",error);return;}
    if(!TestReportStorage::validate(m_reportStorage,&error)) {
        QMessageBox::warning(this,"Cannot save report policy",error);return;
    }
    m_definition=edited;accept();
}
void TestSequenceEditor::addStep() {
    QString error;
    if(!applyStep(&error)){QMessageBox::warning(this,"Invalid step",error);return;}
    if(m_steps.size()>=100){QMessageBox::warning(this,"Limit","Maximum 100 steps");return;}
    QSet<QString> ids;for(const auto& s:m_steps)ids.insert(s.value(QStringLiteral("id")).toString());
    int i=1;QString id;
    do{id=QStringLiteral("step_%1").arg(i++);}while(ids.contains(id));
    // Insert within Main instead of appending after an existing Cleanup.
    // If a Main step is selected, insert directly after it; otherwise append
    // at the end of Main (immediately before Cleanup).
    int target=0;
    if(m_selected>=0 &&
       m_steps.at(m_selected).value(QStringLiteral("group")).toString(QStringLiteral("main"))==QStringLiteral("main"))
        target=m_selected+1;
    else {
        while(target<m_steps.size() &&
              groupRank(m_steps.at(target).value(QStringLiteral("group")).toString(QStringLiteral("main")))<=1)
            ++target;
    }
    m_steps.insert(target,QJsonObject{{"id",id},{"name",QStringLiteral("New test step")},
        {"group",QStringLiteral("main")},{"type",QStringLiteral("action")},
        {"mode",QStringLiteral("delay")},{"durationMs",100}});
    populateTable(target);showPreview();
}
void TestSequenceEditor::removeStep() {
    if(m_selected<0)return;
    if(m_steps.size()<2){QMessageBox::warning(this,"Cannot remove","At least one step is required");return;}
    m_steps.removeAt(m_selected);populateTable(qMin(m_selected,m_steps.size()-1));showPreview();
}
void TestSequenceEditor::moveStep(int direction) {
    QString error;
    if(!applyStep(&error)){QMessageBox::warning(this,"Invalid step",error);return;}
    const int to=m_selected+direction;
    if(to<0||to>=m_steps.size())return;
    if(groupRank(m_steps.at(m_selected).value("group").toString())!=
       groupRank(m_steps.at(to).value("group").toString())){
        m_hint->setText(QStringLiteral("A step can only move within its group. To move between Setup, Main and Cleanup, change its Group property and press Apply Step Changes."));
        return;
    }
    m_steps.swapItemsAt(m_selected,to);populateTable(to);showPreview();
}
void TestSequenceEditor::importJson() {
    const QString path=QFileDialog::getOpenFileName(this,"Import test definition",QString(),
        "WFScript test definitions (*.wftest.json)");
    if(path.isEmpty())return;
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)||f.size()>131072){QMessageBox::warning(this,"Import failed","Unable to open file or exceeds 128 KiB");return;}
    QJsonParseError parse;
    const QJsonDocument doc=QJsonDocument::fromJson(f.readAll(),&parse);
    QString error;
    if(parse.error!=QJsonParseError::NoError || !doc.isObject() ||
       !validateDefinition(doc.object(),&error)) {
        QMessageBox::warning(this,"Import rejected",parse.error==QJsonParseError::NoError?error:parse.errorString());return;
    }
    m_definition=doc.object();loadDocument();
}
void TestSequenceEditor::exportJson() {
    QJsonObject doc;QString error;
    if(!collectDocument(&doc,&error)){QMessageBox::warning(this,"Cannot export",error);return;}
    QString path=QFileDialog::getSaveFileName(this,"Export readable test definition",
        doc.value(QStringLiteral("id")).toString()+QStringLiteral(".wftest.json"),
        "WFScript test definitions (*.wftest.json)");
    if(path.isEmpty())return;
    if(!path.endsWith(QStringLiteral(".wftest.json"),Qt::CaseInsensitive))path+=QStringLiteral(".wftest.json");
    QSaveFile f(path);const QByteArray bytes=QJsonDocument(doc).toJson(QJsonDocument::Indented);
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())
        QMessageBox::warning(this,"Export failed",f.errorString());
}
