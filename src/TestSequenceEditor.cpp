#include "TestSequenceEditor.h"
#include "StructuredTestResults.h"
#include "TestSequenceLoops.h"
#include "TestSequenceCapacity.h"
#include "TestSequenceConditions.h"
#include "ScriptEditor.h"
#include "RunInformationEditor.h"
#include "TestReportStorage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QShowEvent>
#include <QScrollArea>
#include <QFrame>
#include <cmath>
#include <QAbstractItemView>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSignalBlocker>
#include <QAbstractItemModel>
#include <QTextDocument>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFontDatabase>
#include <QTextCursor>
#include <functional>
#include <QSet>
#include <QHash>
#include <algorithm>
#include <tuple>
#include <utility>
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
QVector<int> pathForItem(const QTreeWidgetItem* item) {
    QVector<int> result;
    if(!item)return result;
    const QString encoded=item->data(0,Qt::UserRole).toString();
    if(encoded.isEmpty())return result;
    for(const QString& segment:encoded.split(QLatin1Char('/'))) {
        bool ok=false;const int n=segment.toInt(&ok);
        if(!ok || n<0)return {};
        result.append(n);
    }
    return result;
}
QString pathString(const QVector<int>& path) {
    QStringList parts;
    for(int n:path)parts.append(QString::number(n));
    return parts.join(QLatin1Char('/'));
}
bool containerNode(const QJsonObject& node) {
    return node.contains(QStringLiteral("steps"));
}
QString axisOverview(const QJsonObject& container) {
    const QJsonObject loop=container.value(QStringLiteral("loop")).toObject();
    const QJsonObject repeat=container.value(QStringLiteral("repeat")).toObject();
    QStringList descriptions;
    if(container.contains(QStringLiteral("loop"))) {
        QJsonArray axes=loop.value(QStringLiteral("axes")).toArray();
        if(axes.isEmpty())axes.append(loop);
        for(const auto& v:axes) {
            const auto a=v.toObject();
            const QString name=a.value(QStringLiteral("parameter")).toString();
            if(a.contains(QStringLiteral("values")))
                descriptions<<QStringLiteral("%1: %2 values").arg(name).arg(a.value(QStringLiteral("values")).toArray().size());
            else descriptions<<QStringLiteral("%1: %2 → %3 by %4")
                .arg(name,QString::number(a.value(QStringLiteral("start")).toDouble(),'g',6),
                     QString::number(a.value(QStringLiteral("stop")).toDouble(),'g',6),
                     QString::number(a.value(QStringLiteral("increment")).toDouble(),'g',6));
        }
    }
    if(container.contains(QStringLiteral("repeat")))
        descriptions<<QStringLiteral("repeat %1×").arg(repeat.value(QStringLiteral("count")).toInt());
    return descriptions.join(QStringLiteral("  ·  "));
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

// Display what actually STARTS the operation. Polling callbacks are a
// completion mechanism, not the procedure that the engineer is looking for.
// This is presentation-only: the saved schema and executor are untouched.
QString procedureSummary(const QJsonObject& step) {
    if(step.contains(QStringLiteral("steps"))) {
        const auto list=step.value(QStringLiteral("steps")).toArray();
        return QStringLiteral("Repeat %1 procedure(s)").arg(list.size());
    }
    const QString mode=step.value(QStringLiteral("mode")).toString(QStringLiteral("once"));
    const auto handlers=step.value(QStringLiteral("handlers")).toObject();
    if(mode==QLatin1String("delay"))
        return QStringLiteral("Wait %1 ms").arg(step.value(QStringLiteral("durationMs")).toInt());
    if(mode==QLatin1String("once") || mode==QLatin1String("callback")) {
        QString name=step.value(QStringLiteral("handler")).toString();
        if(name.isEmpty())name=handlers.value(QStringLiteral("start")).toString();
        return name.isEmpty()?QStringLiteral("(No procedure)"):name+QStringLiteral("()");
    }
    const QString start=handlers.value(QStringLiteral("start")).toString();
    if(!start.isEmpty())return start+QStringLiteral("()");
    const QString poll=handlers.value(QStringLiteral("poll")).toString();
    return poll.isEmpty()?QStringLiteral("(No start handler)"):
        QStringLiteral("Poll: %1()").arg(poll);
}
QString displayRuleLimit(const QJsonValue& value) {
    if(value.isObject()) {
        const QJsonObject obj=value.toObject();
        const QString param=obj.value(QStringLiteral("param")).toString();
        if(!param.isEmpty() && obj.value(QStringLiteral("factor")).isDouble())
            return QStringLiteral("%1 * %2").arg(param,
                QString::number(obj.value(QStringLiteral("factor")).toDouble(),'g',12));
        if(!param.isEmpty() && obj.value(QStringLiteral("offset")).isDouble())
            return QStringLiteral("%1 + %2").arg(param,
                QString::number(obj.value(QStringLiteral("offset")).toDouble(),'g',12));
    }
    return value.isDouble()?QString::number(value.toDouble(),'g',12):QString();
}
bool parseRuleLimit(const QString& text,QJsonValue* result) {
    bool numeric=false;const double num=text.trimmed().toDouble(&numeric);
    if(numeric && std::isfinite(num)){*result=QJsonValue(num);return true;}
    static const QRegularExpression pattern(QStringLiteral(
        R"(^\s*([A-Za-z_$][A-Za-z0-9_$]*)\s*([*+])\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*$)"));
    const auto match=pattern.match(text);
    if(!match.hasMatch())return false;
    bool ok=false;const double amount=match.captured(3).toDouble(&ok);
    if(!ok||!std::isfinite(amount))return false;
    *result=QJsonObject{{QStringLiteral("param"),match.captured(1)},
        {match.captured(2)==QLatin1String("*")?QStringLiteral("factor"):QStringLiteral("offset"),amount}};
    return true;
}
QString acceptanceSummary(const QJsonObject& step) {
    if(step.contains(QStringLiteral("steps"))) {
        QStringList names;
        for(const auto& v:step.value(QStringLiteral("steps")).toArray())
            names<<v.toObject().value(QStringLiteral("name")).toString();
        return names.join(QStringLiteral(" → "));
    }
    if(step.value(QStringLiteral("type")).toString()!=QLatin1String("numeric"))
        return QStringLiteral("—");
    const QJsonObject named=step.value(QStringLiteral("measurements")).toObject();
    if(!named.isEmpty())return QStringLiteral("%1 named check(s): %2")
        .arg(named.size()).arg(named.keys().join(QStringLiteral(", ")));
    const auto limits=step.value(QStringLiteral("limits")).toObject();
    const QString unit=step.value(QStringLiteral("unit")).toString();
    if(limits.value(QStringLiteral("low")).isObject()) {
        const auto low=limits.value(QStringLiteral("low")).toObject();
        const auto high=limits.value(QStringLiteral("high")).toObject();
        return QStringLiteral("%1 + [%2, %3]")
            .arg(low.value(QStringLiteral("param")).toString(),
                 QString::number(low.value(QStringLiteral("offset")).toDouble(),'g',6),
                 QString::number(high.value(QStringLiteral("offset")).toDouble(),'g',6));
    }
    if(!limits.value(QStringLiteral("low")).isDouble() ||
       !limits.value(QStringLiteral("high")).isDouble())return QStringLiteral("(Limits not set)");
    const QString bounds=QStringLiteral("%1 – %2")
        .arg(QString::number(limits.value(QStringLiteral("low")).toDouble(),'g',10),
             QString::number(limits.value(QStringLiteral("high")).toDouble(),'g',10));
    return unit.isEmpty()?bounds:bounds+QStringLiteral(" ")+unit;
}
QString executionTooltip(const QJsonObject& step) {
    const QString mode=step.value(QStringLiteral("mode")).toString(QStringLiteral("once"));
    const auto handlers=step.value(QStringLiteral("handlers")).toObject();
    QStringList parts;
    parts << QStringLiteral("Step: %1").arg(step.value(QStringLiteral("id")).toString());
    parts << QStringLiteral("Procedure: %1").arg(procedureSummary(step));
    if(mode==QLatin1String("callback"))
        parts << QStringLiteral("Complete using TestRunner.complete(step.token, result) or fail(step.token, reason)");
    if(step.contains(QStringLiteral("parameters")))
        parts << QStringLiteral("Parameters: %1").arg(QString::fromUtf8(
            QJsonDocument(step.value(QStringLiteral("parameters")).toObject()).toJson(QJsonDocument::Compact)));
    if(mode==QLatin1String("poll")) {
        parts << QStringLiteral("Completion poll: %1()")
                     .arg(handlers.value(QStringLiteral("poll")).toString());
        parts << QStringLiteral("Polling interval: %1 ms")
                     .arg(step.value(QStringLiteral("intervalMs")).toInt(50));
    }
    const QString finish=handlers.value(QStringLiteral("finish")).toString();
    if(!finish.isEmpty())parts << QStringLiteral("On finish: %1()").arg(finish);
    if(step.value(QStringLiteral("type")).toString()==QLatin1String("numeric"))
        parts << QStringLiteral("Acceptance: %1").arg(acceptanceSummary(step));
    const QString description=step.value(QStringLiteral("description")).toString();
    if(!description.isEmpty())parts << description;
    return parts.join(QLatin1Char('\n'));
}
QStringList displayColumns(const QJsonObject& step) {
    return {step.value(QStringLiteral("group")).toString(QStringLiteral("main")),
            (step.contains(QStringLiteral("steps"))?QStringLiteral("⟳ "):QString())+
                step.value(QStringLiteral("name")).toString(),
            procedureSummary(step),
            step.value(QStringLiteral("type")).toString(QStringLiteral("test")),
            step.value(QStringLiteral("mode")).toString(QStringLiteral("once")),
            acceptanceSummary(step),
            step.value(QStringLiteral("description")).toString()};
}

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
        QStringLiteral("title"),QStringLiteral("config"),QStringLiteral("steps"),QStringLiteral("variables")};
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
    if(doc.contains(QStringLiteral("variables")) && (!doc.value(QStringLiteral("variables")).isArray() ||
        !TestSequenceConditions::validateVariables(doc.value(QStringLiteral("variables")).toArray(),error)))return false;
    if(!doc.value(QStringLiteral("steps")).isArray())return errorOut(error,QStringLiteral("Missing steps array"));
    QJsonObject expanded;
    if(!TestSequenceLoops::expand(doc,&expanded,error))return false;
    const QJsonArray steps=expanded.value(QStringLiteral("steps")).toArray();
    if(steps.isEmpty() || steps.size()>TestSequenceCapacity::maxExpandedSteps)
        return errorOut(error,QStringLiteral("A sequence must contain 1..%1 executable steps").arg(TestSequenceCapacity::maxExpandedSteps));
    QSet<QString> ids;
    static const QRegularExpression name(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
    for(int i=0;i<steps.size();++i){
        if(!steps[i].isObject())return errorOut(error,QStringLiteral("Step %1 is not an object").arg(i+1));
        const QJsonObject step=steps[i].toObject();
        const QSet<QString> stepKeys={QStringLiteral("id"),QStringLiteral("name"),
            QStringLiteral("description"),QStringLiteral("group"),QStringLiteral("type"),
            QStringLiteral("mode"),QStringLiteral("timeoutMs"),QStringLiteral("intervalMs"),
            QStringLiteral("durationMs"),QStringLiteral("handler"),QStringLiteral("handlers"),
            QStringLiteral("limits"),QStringLiteral("unit"),QStringLiteral("resultWidget"),
            QStringLiteral("parameters"),QStringLiteral("measurements"),QStringLiteral("templateId"),
            QStringLiteral("condition"),QStringLiteral("guards")};
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
           !QStringList{QStringLiteral("once"),QStringLiteral("poll"),QStringLiteral("callback"),
                        QStringLiteral("delay"),QStringLiteral("condition")}.contains(mode))
            return errorOut(error,QStringLiteral("Step %1 has an unsupported group, type, or execution mode").arg(i+1));
        if(mode==QLatin1String("condition")) {
            if(type!=QLatin1String("action") || group!=QLatin1String("main") ||
               !step.value(QStringLiteral("condition")).isObject() ||
               !TestSequenceConditions::validate(step.value(QStringLiteral("condition")).toObject(),
                  doc.value(QStringLiteral("variables")).toArray(),error))
                return errorOut(error,QStringLiteral("Invalid IF step '%1': %2").arg(id,error?*error:QString()));
            continue;
        }
        if(type==QStringLiteral("numeric")) {
            if(mode==QStringLiteral("delay"))return errorOut(error,QStringLiteral("Step %1: numeric steps cannot be Delay; use an Action delay followed by a numeric test").arg(i+1));
            const QJsonObject named=step.value(QStringLiteral("measurements")).toObject();
            if(step.contains(QStringLiteral("measurements"))) {
                if(!step.value(QStringLiteral("measurements")).isObject() ||
                   step.contains(QStringLiteral("limits")) || step.contains(QStringLiteral("unit")) ||
                   step.contains(QStringLiteral("resultWidget")))
                    return errorOut(error,QStringLiteral("Step %1: named results replace scalar limits/unit/widget").arg(i+1));
                QVector<StructuredTestResults::Rule> parsed;
                QString reason;
                if(!StructuredTestResults::parse(named.toVariantMap(),&parsed,&reason))
                    return errorOut(error,QStringLiteral("Step %1: %2").arg(i+1).arg(reason));
            } else {
                const QJsonObject limits=step.value(QStringLiteral("limits")).toObject();
                if(!limits.value(QStringLiteral("low")).isDouble() ||
                   !limits.value(QStringLiteral("high")).isDouble() ||
                   limits.value(QStringLiteral("low")).toDouble()>limits.value(QStringLiteral("high")).toDouble())
                    return errorOut(error,QStringLiteral("Step %1: numeric limits require low <= high").arg(i+1));
            }
        } else if(step.contains(QStringLiteral("measurements")))
            return errorOut(error,QStringLiteral("Step %1: named results require a numeric step").arg(i+1));
        for(const auto& item:std::initializer_list<std::tuple<QString,int,int>>{
            {QStringLiteral("timeoutMs"),0,86400000},{QStringLiteral("durationMs"),0,86400000},
            {QStringLiteral("intervalMs"),10,60000}}) {
            const QString key=std::get<0>(item);
            if(step.contains(key) && !integer(step.value(key),std::get<1>(item),std::get<2>(item)))
                return errorOut(error,QStringLiteral("Step %1: invalid %2").arg(i+1).arg(key));
        }
        if(mode==QStringLiteral("delay") && step.value(QStringLiteral("durationMs")).toInt()<=0)
            return errorOut(error,QStringLiteral("Step %1: delay duration must be positive").arg(i+1));
        if(step.contains(QStringLiteral("parameters"))) {
            if(!step.value(QStringLiteral("parameters")).isObject() ||
               QJsonDocument(step.value(QStringLiteral("parameters")).toObject()).toJson(QJsonDocument::Compact).size()>8192)
                return errorOut(error,QStringLiteral("Step %1 has invalid/oversized parameters JSON").arg(i+1));
        }
        if(step.contains(QStringLiteral("handler")) && step.contains(QStringLiteral("handlers")))
            return errorOut(error,QStringLiteral("Step %1 has both handler and handlers").arg(i+1));
        if(step.contains(QStringLiteral("handler")) &&
           (!step.value(QStringLiteral("handler")).isString() ||
            (mode!=QStringLiteral("once") && mode!=QStringLiteral("callback"))))
            return errorOut(error,QStringLiteral("Step %1: handler is only for once steps").arg(i+1));
        if(step.contains(QStringLiteral("handlers")) && !step.value(QStringLiteral("handlers")).isObject())
            return errorOut(error,QStringLiteral("Step %1: handlers must be an object").arg(i+1));
        const QString handler=step.value(QStringLiteral("handler")).toString();
        const QJsonObject handlers=step.value(QStringLiteral("handlers")).toObject();
        if(mode==QStringLiteral("once") && !name.match(handler).hasMatch())
            return errorOut(error,QStringLiteral("Step %1: once requires a named exported function").arg(i+1));
        if(mode==QStringLiteral("callback") && !name.match(handler.isEmpty()
                ?handlers.value(QStringLiteral("start")).toString():handler).hasMatch())
            return errorOut(error,QStringLiteral("Step %1: callback requires a named exported function").arg(i+1));
        if(mode==QStringLiteral("callback") && handlers.contains(QStringLiteral("poll")))
            return errorOut(error,QStringLiteral("Step %1: callback mode does not use polling").arg(i+1));
        if(mode==QStringLiteral("poll") && !name.match(handlers.value(QStringLiteral("poll")).toString()).hasMatch())
            return errorOut(error,QStringLiteral("Step %1: poll requires handlers.poll").arg(i+1));
        for(auto it=handlers.begin();it!=handlers.end();++it)
            if(!QStringList{QStringLiteral("start"),QStringLiteral("poll"),QStringLiteral("finish")}.contains(it.key()) ||
               !name.match(it.value().toString()).hasMatch())
                return errorOut(error,QStringLiteral("Step %1 has an invalid handler name").arg(i+1));
    }
    return true;
}

class ExpandedPreviewModel final : public QAbstractTableModel {
public:
    explicit ExpandedPreviewModel(QObject* parent):QAbstractTableModel(parent){}
    QVector<QStringList> rows;
    int rowCount(const QModelIndex& parent=QModelIndex()) const override {return parent.isValid()?0:rows.size();}
    int columnCount(const QModelIndex& parent=QModelIndex()) const override {return parent.isValid()?0:7;}
    QVariant headerData(int section,Qt::Orientation orient,int role) const override {
        if(role!=Qt::DisplayRole||orient!=Qt::Horizontal)return {};
        return QStringList{QStringLiteral("#"),QStringLiteral("Group"),
            QStringLiteral("Operation (expanded)"),QStringLiteral("Procedure"),
            QStringLiteral("Resolved parameters"),QStringLiteral("Acceptance checks"),
            QStringLiteral("Authored source")}.value(section);
    }
    QVariant data(const QModelIndex& index,int role) const override {
        if(!index.isValid()||index.row()<0||index.row()>=rows.size())return {};
        if(role==Qt::DisplayRole||role==Qt::ToolTipRole)return rows.at(index.row()).value(index.column());
        if(role==Qt::TextAlignmentRole && index.column()==0)return int(Qt::AlignRight|Qt::AlignVCenter);
        return {};
    }
    void replace(QVector<QStringList>&& replacement) {
        beginResetModel();rows=std::move(replacement);endResetModel();
    }
};


TestSequenceEditor::TestSequenceEditor(const QJsonObject& definition,const QJsonObject& runInformation,
                                       const QStringList& namedExports,QWidget* parent,
                                       const QJsonObject& reportStorage,
                                       const QString& libraryName,const QString& librarySource)
    :QDialog(parent),m_definition(definition),m_runInformation(runInformation),
     m_reportStorage(reportStorage),m_exports(namedExports),
     m_libraryName(libraryName),m_librarySource(librarySource) {
    setWindowTitle(QStringLiteral("Visual Test Sequence Editor — WFScript RC1.5.14.1"));
    setObjectName(QStringLiteral("visualTestSequenceEditor"));
    setWindowModality(Qt::NonModal);
    // A true top-level resizable window with Windows minimize/maximize buttons.
    // No fixed 1580px content minimum: the action bar must stay on-screen.
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setSizeGripEnabled(true);
    setMinimumSize(740,480);
    resize(1440,840);

    auto* outer=new QVBoxLayout(this);
    outer->setContentsMargins(10,8,10,8);
    outer->setSpacing(5);
    m_tabs=new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("testSequenceEditorTabs"));
    outer->addWidget(m_tabs,1);

    // STEP DETAILS: three compact, always-visible property columns above a
    // full-width table. There are no disclosure controls or scroll areas
    // separating On Start from its corresponding completion handler.
    auto* editTab=new QWidget(m_tabs);
    auto* editLayout=new QVBoxLayout(editTab);
    editLayout->setContentsMargins(4,5,4,3);
    editLayout->setSpacing(5);
    auto* detailColumns=new QWidget(editTab);
    detailColumns->setObjectName(QStringLiteral("testSequenceDetailsColumns"));
    m_detailColumns=detailColumns;
    auto* detailLayout=new QHBoxLayout(detailColumns);
    detailLayout->setContentsMargins(0,0,0,0);
    detailLayout->setSpacing(10);

    auto* operation=new QGroupBox(QStringLiteral("Step identity"),detailColumns);
    auto* opForm=new QFormLayout(operation);
    opForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    opForm->setContentsMargins(8,8,8,8);
    opForm->setVerticalSpacing(4);
    m_stepId=new QLineEdit(operation);m_stepName=new QLineEdit(operation);
    m_description=new QPlainTextEdit(operation);
    m_description->setObjectName(QStringLiteral("testSequenceStepDescription"));
    m_description->setFixedHeight(60);
    m_description->setTabChangesFocus(true);
    m_group=new QComboBox(operation);m_group->setObjectName(QStringLiteral("testSequenceGroupCombo"));
    m_group->addItems({"setup","main","cleanup"});
    m_type=new QComboBox(operation);m_type->addItems({"action","test","numeric"});
    opForm->addRow(QStringLiteral("Step name"),m_stepName);
    opForm->addRow(QStringLiteral("Description"),m_description);
    opForm->addRow(QStringLiteral("Step ID"),m_stepId);
    opForm->addRow(QStringLiteral("Group"),m_group);
    opForm->addRow(QStringLiteral("Type"),m_type);
    detailLayout->addWidget(operation,3);

    auto* execution=new QGroupBox(QStringLiteral("Procedure and completion"),detailColumns);
    execution->setObjectName(QStringLiteral("testSequenceExecutionGroup"));
    auto* executeForm=new QFormLayout(execution);
    executeForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    executeForm->setContentsMargins(8,8,8,8);
    executeForm->setVerticalSpacing(4);
    m_mode=new QComboBox(execution);m_mode->addItems({"once","poll","callback","delay"});
    m_handler=namedCombo(m_exports,execution);m_start=namedCombo(m_exports,execution);
    m_poll=namedCombo(m_exports,execution);m_finish=namedCombo(m_exports,execution);
    m_handler->setObjectName(QStringLiteral("testSequenceOnceHandler"));
    m_start->setObjectName(QStringLiteral("testSequenceStartHandler"));
    m_poll->setObjectName(QStringLiteral("testSequencePollHandler"));
    m_handler->setToolTip(QStringLiteral("Exported library function called by a Once step."));
    m_start->setToolTip(QStringLiteral("Exported procedure called once at the start of a polling step."));
    m_poll->setToolTip(QStringLiteral("Exported function that checks for completion after the start procedure."));
    m_finish->setToolTip(QStringLiteral("Optional exported function called when the step finishes."));
    m_timeout=new QSpinBox(execution);m_timeout->setRange(0,86400000);m_timeout->setSuffix(QStringLiteral(" ms"));
    m_interval=new QSpinBox(execution);m_interval->setRange(10,60000);m_interval->setValue(50);m_interval->setSuffix(QStringLiteral(" ms"));
    m_duration=new QSpinBox(execution);m_duration->setRange(0,86400000);m_duration->setSuffix(QStringLiteral(" ms"));
    executeForm->addRow(QStringLiteral("Mode"),m_mode);
    // The same small Edit control sits beside each named handler. There is
    // no disconnected toolbar to explain while authoring a step.
    auto handlerRow=[execution](QComboBox* combo,const QString& buttonId)->QWidget* {
        auto* row=new QWidget(execution);
        auto* layout=new QHBoxLayout(row);
        layout->setContentsMargins(0,0,0,0);layout->setSpacing(4);
        auto* button=new QPushButton(QStringLiteral("Edit"),row);
        button->setObjectName(buttonId);
        button->setMaximumWidth(55);
        layout->addWidget(combo,1);layout->addWidget(button);
        return row;
    };
    executeForm->addRow(QStringLiteral("Once / callback"),handlerRow(m_handler,QStringLiteral("testSequenceOpenOnceCode")));
    executeForm->addRow(QStringLiteral("On start"),handlerRow(m_start,QStringLiteral("testSequenceOpenProcedureCode")));
    executeForm->addRow(QStringLiteral("Polling handler"),handlerRow(m_poll,QStringLiteral("testSequenceOpenPollCode")));
    executeForm->addRow(QStringLiteral("On finish"),handlerRow(m_finish,QStringLiteral("testSequenceOpenFinishCode")));
    executeForm->addRow(QStringLiteral("Timeout"),m_timeout);
    executeForm->addRow(QStringLiteral("Poll interval"),m_interval);
    executeForm->addRow(QStringLiteral("Delay"),m_duration);
    detailLayout->addWidget(execution,4);

    auto* measurement=new QGroupBox(QStringLiteral("Measurement / acceptance"),detailColumns);
    measurement->setObjectName(QStringLiteral("testSequenceMeasurementGroup"));
    auto* measureForm=new QFormLayout(measurement);
    measureForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    measureForm->setContentsMargins(8,8,8,8);
    measureForm->setVerticalSpacing(6);
    m_limitLow=new QDoubleSpinBox(measurement);m_limitHigh=new QDoubleSpinBox(measurement);
    m_limitLow->setObjectName(QStringLiteral("testSequenceLimitLow"));
    m_limitHigh->setObjectName(QStringLiteral("testSequenceLimitHigh"));
    for(QDoubleSpinBox* spin:{m_limitLow,m_limitHigh}){
        spin->setRange(-1e12,1e12);spin->setDecimals(6);
    }
    m_numericUnit=new QLineEdit(measurement);m_numericWidget=new QLineEdit(measurement);
    m_relativeLimits=new QCheckBox(QStringLiteral("Limits relative to sweep parameter"),measurement);
    m_relativeLimits->setObjectName(QStringLiteral("testSequenceRelativeLimits"));
    m_limitParameter=new QLineEdit(measurement);
    m_limitParameter->setObjectName(QStringLiteral("testSequenceRelativeParameter"));
    m_limitParameter->setPlaceholderText(QStringLiteral("voltage"));
    m_lowOffset=new QDoubleSpinBox(measurement);m_highOffset=new QDoubleSpinBox(measurement);
    for(auto* spin:{m_lowOffset,m_highOffset}){spin->setRange(-1e12,1e12);spin->setDecimals(6);}
    m_lowOffset->setObjectName(QStringLiteral("testSequenceRelativeLowOffset"));
    m_highOffset->setObjectName(QStringLiteral("testSequenceRelativeHighOffset"));
    measureForm->addRow(QStringLiteral("Lower limit"),m_limitLow);
    measureForm->addRow(QStringLiteral("Upper limit"),m_limitHigh);
    measureForm->addRow(m_relativeLimits);
    measureForm->addRow(QStringLiteral("Sweep parameter"),m_limitParameter);
    measureForm->addRow(QStringLiteral("Lower offset"),m_lowOffset);
    measureForm->addRow(QStringLiteral("Upper offset"),m_highOffset);
    measureForm->addRow(QStringLiteral("Units"),m_numericUnit);
    measureForm->addRow(QStringLiteral("Result widget ID"),m_numericWidget);
    m_parameters=new QPlainTextEdit(measurement);
    m_parameters->setObjectName(QStringLiteral("testSequenceParametersJson"));
    m_parameters->setPlaceholderText(QStringLiteral("{\n  \"voltage\": 5.0\n}"));
    m_parameters->setFixedHeight(78);
    m_parameters->setTabChangesFocus(true);
    measureForm->addRow(QStringLiteral("Step parameters (JSON)"),m_parameters);
    m_hint=new QLabel(QStringLiteral("Scalar result: one number. Named result: return a JSON object/array and configure fields in Named Results & Limits. Callback steps call TestRunner.complete(step.token,result)."),measurement);
    m_hint->setWordWrap(true);
    measureForm->addRow(m_hint);
    m_structuredResults=new QCheckBox(QStringLiteral("Evaluate named fields from a JSON object/array"),measurement);
    m_structuredResults->setObjectName(QStringLiteral("testSequenceStructuredResults"));
    measureForm->addRow(m_structuredResults);
    auto* openNamed=new QPushButton(QStringLiteral("Edit named results and limits…"),measurement);
    openNamed->setObjectName(QStringLiteral("testSequenceOpenNamedResults"));
    measureForm->addRow(openNamed);
    detailLayout->addWidget(measurement,3);
    // Sweep and Repeat are ordinary parent rows. Their settings use the
    // SAME Step Details tab; no JSON authoring tab or secondary dialog.
    m_loopDetails=new QWidget(editTab);
    m_loopDetails->setObjectName(QStringLiteral("testSequenceContainerDetails"));
    auto* containerColumns=new QHBoxLayout(m_loopDetails);
    containerColumns->setContentsMargins(0,0,0,0);
    containerColumns->setSpacing(10);
    auto* identity=new QGroupBox(QStringLiteral("Sweep / Repeat identity"),m_loopDetails);
    auto* identityForm=new QFormLayout(identity);
    m_loopName=new QLineEdit(identity);m_loopName->setObjectName(QStringLiteral("testSequenceContainerName"));
    m_loopId=new QLineEdit(identity);m_loopId->setObjectName(QStringLiteral("testSequenceContainerId"));
    m_loopDescription=new QPlainTextEdit(identity);m_loopDescription->setFixedHeight(65);
    m_loopKind=new QComboBox(identity);m_loopKind->setObjectName(QStringLiteral("testSequenceContainerKind"));
    m_loopKind->addItems({QStringLiteral("Sweep"),QStringLiteral("Repeat")});
    identityForm->addRow(QStringLiteral("Name"),m_loopName);
    identityForm->addRow(QStringLiteral("ID"),m_loopId);
    identityForm->addRow(QStringLiteral("Type"),m_loopKind);
    identityForm->addRow(QStringLiteral("Description"),m_loopDescription);
    containerColumns->addWidget(identity,3);
    auto* axesBox=new QGroupBox(QStringLiteral("Sweep parameters — each combination runs the child steps"),m_loopDetails);
    auto* axesLayout=new QVBoxLayout(axesBox);
    m_axisTable=new QTableWidget(axesBox);
    m_axisTable->setObjectName(QStringLiteral("testSequenceSweepAxes"));
    m_axisTable->setColumnCount(6);
    m_axisTable->setHorizontalHeaderLabels({QStringLiteral("Parameter"),QStringLiteral("Kind"),
        QStringLiteral("Start"),QStringLiteral("Stop"),QStringLiteral("Increment"),QStringLiteral("Values (comma-separated)")});
    m_axisTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_axisTable->horizontalHeader()->setStretchLastSection(true);
    m_axisTable->setColumnWidth(0,95);m_axisTable->setColumnWidth(1,80);
    m_axisTable->setColumnWidth(2,70);m_axisTable->setColumnWidth(3,70);
    m_axisTable->setColumnWidth(4,80);
    m_axisTable->verticalHeader()->setVisible(false);
    m_axisTable->setMinimumWidth(470);m_axisTable->setMinimumHeight(140);
    axesLayout->addWidget(m_axisTable);
    auto* axisButtons=new QHBoxLayout;
    auto* addAxisButton=new QPushButton(QStringLiteral("+ Add parameter"),axesBox);
    addAxisButton->setObjectName(QStringLiteral("testSequenceAddSweepAxis"));
    auto* removeAxisButton=new QPushButton(QStringLiteral("Remove parameter"),axesBox);
    axisButtons->addWidget(addAxisButton);axisButtons->addWidget(removeAxisButton);axisButtons->addStretch();
    axesLayout->addLayout(axisButtons);
    containerColumns->addWidget(axesBox,5);
    auto* repeatBox=new QGroupBox(QStringLiteral("Repeat"),m_loopDetails);
    auto* repeatForm=new QFormLayout(repeatBox);
    m_sweepRepeat=new QCheckBox(QStringLiteral("Repeat each sweep combination"),repeatBox);
    m_sweepRepeat->setObjectName(QStringLiteral("testSequenceSweepRepeatEnabled"));
    m_loopRepeatCount=new QSpinBox(repeatBox);m_loopRepeatCount->setRange(1,TestSequenceCapacity::maxRepeatCount);
    m_loopRepeatCount->setObjectName(QStringLiteral("testSequenceRepeatCount"));
    m_loopIndex=new QLineEdit(repeatBox);m_loopIndex->setObjectName(QStringLiteral("testSequenceRepeatIndex"));
    m_loopIndex->setPlaceholderText(QStringLiteral("iteration"));
    repeatForm->addRow(m_sweepRepeat);
    repeatForm->addRow(QStringLiteral("Count"),m_loopRepeatCount);
    repeatForm->addRow(QStringLiteral("Index name"),m_loopIndex);
    m_loopPreview=new QLabel(QStringLiteral("Select a sweep or repeat to edit its settings."),repeatBox);
    m_loopPreview->setWordWrap(true);repeatForm->addRow(m_loopPreview);
    containerColumns->addWidget(repeatBox,2);
    auto* detailsPanel=new QWidget(editTab);
    auto* panelStack=new QVBoxLayout(detailsPanel);
    panelStack->setContentsMargins(0,0,0,0);
    panelStack->addWidget(detailColumns);
    panelStack->addWidget(m_loopDetails);
    m_loopDetails->hide();
    // RC1.5.14: IF/ELSE uses the same Step Details workspace as Sweep/Repeat.
    m_conditionDetails=new QWidget(editTab);
    m_conditionDetails->setObjectName(QStringLiteral("testSequenceConditionDetails"));
    auto* conditionColumns=new QHBoxLayout(m_conditionDetails);
    conditionColumns->setContentsMargins(0,0,0,0);
    auto* conditionIdentity=new QGroupBox(QStringLiteral("Conditional step"),m_conditionDetails);
    auto* conditionIdentityForm=new QFormLayout(conditionIdentity);
    m_conditionName=new QLineEdit(conditionIdentity);
    m_conditionId=new QLineEdit(conditionIdentity);
    m_conditionName->setObjectName(QStringLiteral("testSequenceConditionName"));
    conditionIdentityForm->addRow(QStringLiteral("Name"),m_conditionName);
    conditionIdentityForm->addRow(QStringLiteral("ID"),m_conditionId);
    conditionColumns->addWidget(conditionIdentity,2);
    m_conditionEvaluation=new QGroupBox(QStringLiteral("IF — compare a recorded value"),m_conditionDetails);
    auto* conditionForm=new QFormLayout(m_conditionEvaluation);
    m_conditionSource=new QComboBox(m_conditionEvaluation);
    m_conditionSource->addItems({QStringLiteral("Test variable"),QStringLiteral("Sweep/repeat parameter"),
        QStringLiteral("Recorded measurement"),QStringLiteral("Step status"),
        QStringLiteral("Run Information"),QStringLiteral("Test counter")});
    m_conditionSource->setObjectName(QStringLiteral("testSequenceConditionSource"));
    m_conditionKey=new QComboBox(m_conditionEvaluation);m_conditionKey->setEditable(true);
    m_conditionKey->setObjectName(QStringLiteral("testSequenceConditionKey"));
    m_conditionStep=new QComboBox(m_conditionEvaluation);m_conditionStep->setEditable(true);
    m_conditionStep->setObjectName(QStringLiteral("testSequenceConditionStep"));
    m_conditionPath=new QLineEdit(m_conditionEvaluation);
    m_conditionPath->setObjectName(QStringLiteral("testSequenceConditionMeasurementPath"));
    m_conditionPath->setPlaceholderText(QStringLiteral("e.g. vpp; leave blank for scalar"));
    m_conditionOperator=new QComboBox(m_conditionEvaluation);
    m_conditionOperator->addItems({QStringLiteral("<"),QStringLiteral("<="),QStringLiteral("=="),
        QStringLiteral("!="),QStringLiteral(">="),QStringLiteral(">")});
    m_conditionOperator->setObjectName(QStringLiteral("testSequenceConditionOperator"));
    m_conditionValueType=new QComboBox(m_conditionEvaluation);
    m_conditionValueType->addItems({QStringLiteral("Number"),QStringLiteral("Text"),QStringLiteral("Boolean")});
    m_conditionValue=new QLineEdit(m_conditionEvaluation);m_conditionValue->setText(QStringLiteral("50"));
    m_conditionValue->setObjectName(QStringLiteral("testSequenceConditionCompareValue"));
    auto* valueRow=new QWidget(m_conditionEvaluation);auto* valueLayout=new QHBoxLayout(valueRow);
    valueLayout->setContentsMargins(0,0,0,0);valueLayout->addWidget(m_conditionValueType,1);valueLayout->addWidget(m_conditionValue,3);
    conditionForm->addRow(QStringLiteral("Data source"),m_conditionSource);
    conditionForm->addRow(QStringLiteral("Variable / field"),m_conditionKey);
    conditionForm->addRow(QStringLiteral("Originating step"),m_conditionStep);
    conditionForm->addRow(QStringLiteral("Result field path"),m_conditionPath);
    conditionForm->addRow(QStringLiteral("Comparison"),m_conditionOperator);
    conditionForm->addRow(QStringLiteral("Against"),valueRow);
    auto* help=new QLabel(QStringLiteral("The condition reads recorded data only. Missing values and type mismatches cause an execution ERROR, never an implicit false result."),m_conditionEvaluation);
    help->setWordWrap(true);conditionForm->addRow(help);
    conditionColumns->addWidget(m_conditionEvaluation,5);
    panelStack->addWidget(m_conditionDetails);
    m_conditionDetails->hide();
    auto* detailsScroll=new QScrollArea(editTab);
    detailsScroll->setObjectName(QStringLiteral("testSequenceDetailsScroll"));
    detailsScroll->setWidgetResizable(true);
    detailsScroll->setFrameShape(QFrame::NoFrame);
    detailsScroll->setWidget(detailsPanel);
    detailsScroll->setMinimumHeight(160);
    detailsScroll->setMaximumHeight(480);
    editLayout->addWidget(detailsScroll,0);
    connect(addAxisButton,&QPushButton::clicked,this,[this]{
        if(m_axisTable->rowCount()<3){addAxis(QJsonObject{{QStringLiteral("parameter"),QStringLiteral("parameter%1").arg(m_axisTable->rowCount()+1)},
            {QStringLiteral("start"),0},{QStringLiteral("stop"),1},{QStringLiteral("increment"),1}});m_modified=true;m_stepDirty=true;}
    });
    connect(removeAxisButton,&QPushButton::clicked,this,[this]{
        const int r=m_axisTable->currentRow();if(r>=0){m_axisTable->removeRow(r);m_modified=true;m_stepDirty=true;}
    });
    connect(m_loopKind,&QComboBox::currentTextChanged,this,[this]{setContainerVisibility();});
    connect(m_sweepRepeat,&QCheckBox::toggled,this,[this]{setContainerVisibility();});
    m_stepOverview=new QLabel(editTab);
    m_stepOverview->setObjectName(QStringLiteral("testSequenceSelectedSummary"));
    m_stepOverview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_stepOverview->setWordWrap(true);
    m_stepOverview->setMaximumHeight(35);
    editLayout->addWidget(m_stepOverview);
    auto* toolbar=new QHBoxLayout;
    m_countLabel=new QLabel(editTab);
    m_countLabel->setObjectName(QStringLiteral("testSequenceStepCounts"));
    toolbar->addWidget(m_countLabel);
    toolbar->addStretch();
    auto* executePreviewButton=new QPushButton(QStringLiteral("Preview Execution…"),editTab);
    executePreviewButton->setObjectName(QStringLiteral("testSequenceOpenExecutionPreview"));
    executePreviewButton->setToolTip(QStringLiteral("Show the exact ordered steps, inherited sweep/repeat parameters, and resolved acceptance limits before RUN. Nothing is executed."));
    toolbar->addWidget(executePreviewButton);
    auto* apply=new QPushButton(QStringLiteral("Apply Step Changes"),editTab);
    apply->setObjectName(QStringLiteral("applyTestStepButton"));
    toolbar->addWidget(apply);
    auto* add=new QPushButton(QStringLiteral("+ Step"),editTab);
    auto* addSweep=new QPushButton(QStringLiteral("+ Sweep"),editTab);
    addSweep->setObjectName(QStringLiteral("addTestSweepButton"));
    addSweep->setToolTip(QStringLiteral("Insert a Sweep inside a selected IF/ELSE or Sweep/Repeat, or before the selected step."));
    auto* addRepeat=new QPushButton(QStringLiteral("+ Repeat"),editTab);
    addRepeat->setObjectName(QStringLiteral("addTestRepeatButton"));
    auto* addIf=new QPushButton(QStringLiteral("+ IF"),editTab);
    addIf->setObjectName(QStringLiteral("testSequenceAddIf"));
    auto* addElse=new QPushButton(QStringLiteral("+ ELSE"),editTab);
    addElse->setObjectName(QStringLiteral("testSequenceAddElse"));
    auto* addChildButton=new QPushButton(QStringLiteral("+ Child Step"),editTab);
    addChildButton->setObjectName(QStringLiteral("testSequenceAddChildButton"));
    auto* indent=new QPushButton(QStringLiteral("Indent"),editTab);
    indent->setObjectName(QStringLiteral("testSequenceIndentButton"));
    auto* outdent=new QPushButton(QStringLiteral("Outdent"),editTab);
    outdent->setObjectName(QStringLiteral("testSequenceOutdentButton"));
    auto* duplicate=new QPushButton(QStringLiteral("Duplicate"),editTab);
    duplicate->setObjectName(QStringLiteral("testSequenceDuplicateButton"));
    auto* remove=new QPushButton(QStringLiteral("Remove"),editTab);
    auto* up=new QPushButton(QStringLiteral("Move Up"),editTab);
    auto* down=new QPushButton(QStringLiteral("Move Down"),editTab);
    add->setObjectName(QStringLiteral("addTestStepButton"));
    remove->setObjectName(QStringLiteral("removeTestStepButton"));
    up->setObjectName(QStringLiteral("moveTestStepUpButton"));
    down->setObjectName(QStringLiteral("moveTestStepDownButton"));
    editLayout->addLayout(toolbar);
    // Two compact, clearly grouped rows remain usable on smaller screens.
    // The Save/Cancel bar is outside the tabs and never scrolls off-screen.
    auto* stepActions=new QGridLayout;
    stepActions->setHorizontalSpacing(5);stepActions->setVerticalSpacing(4);
    stepActions->addWidget(add,0,0);
    stepActions->addWidget(addSweep,0,1);
    stepActions->addWidget(addRepeat,0,2);
    stepActions->addWidget(addChildButton,0,3);
    stepActions->addWidget(duplicate,0,4);
    stepActions->addWidget(indent,1,0);
    stepActions->addWidget(outdent,1,1);
    stepActions->addWidget(up,1,2);
    stepActions->addWidget(down,1,3);
    stepActions->addWidget(remove,1,4);
    stepActions->addWidget(addIf,2,0);
    stepActions->addWidget(addElse,2,1);
    editLayout->addLayout(stepActions);
    m_table=new QTreeWidget(editTab);
    m_table->setObjectName(QStringLiteral("testSequenceStepTable"));
    m_table->setColumnCount(7);
    m_table->setHeaderLabels({QStringLiteral("Group / step / operation"),
        QStringLiteral("Procedure (start)"),QStringLiteral("Type"),QStringLiteral("Mode"),
        QStringLiteral("Limits"),QStringLiteral("Description"),QStringLiteral("Step ID")});
    m_table->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->header()->setStretchLastSection(true);
    m_table->setColumnWidth(0,390);m_table->setColumnWidth(1,190);
    m_table->setColumnWidth(2,95);m_table->setColumnWidth(3,90);
    m_table->setColumnWidth(4,185);m_table->setColumnWidth(5,360);
    m_table->setRootIsDecorated(true);
    m_table->setUniformRowHeights(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(170);
    m_table->setToolTip(QStringLiteral("Select a row to edit it above. Sweep and Repeat rows expand to show their child steps."));
    editLayout->addWidget(m_table,1);
    m_tabs->addTab(editTab,QStringLiteral("Step Details"));

    // RC1.5.13: human-readable, read-only execution preview. It calls the
    // SAME TestSequenceLoops::expand() used by the native Test Executive,
    // rather than inventing a second expansion or exposing raw generated JSON.
    m_executionTab=new QWidget(m_tabs);
    auto* executionLayout=new QVBoxLayout(m_executionTab);
    auto* executionToolbar=new QHBoxLayout;
    auto* executionTitle=new QLabel(QStringLiteral("Execution Preview — exact order after sweep and repeat expansion"),m_executionTab);
    QFont titleFont=executionTitle->font();titleFont.setBold(true);
    executionTitle->setFont(titleFont);
    executionToolbar->addWidget(executionTitle);
    executionToolbar->addStretch();
    executionToolbar->addWidget(new QLabel(QStringLiteral("Show:"),m_executionTab));
    m_executionScope=new QComboBox(m_executionTab);
    m_executionScope->setObjectName(QStringLiteral("testSequencePreviewScope"));
    m_executionScope->addItems({QStringLiteral("Entire sequence"),QStringLiteral("Selected row and its children")});
    executionToolbar->addWidget(m_executionScope);
    auto* refreshExecution=new QPushButton(QStringLiteral("Refresh"),m_executionTab);
    refreshExecution->setObjectName(QStringLiteral("testSequenceRefreshExecutionPreview"));
    executionToolbar->addWidget(refreshExecution);
    auto* backToTree=new QPushButton(QStringLiteral("Back to Step Details"),m_executionTab);
    executionToolbar->addWidget(backToTree);
    executionLayout->addLayout(executionToolbar);
    m_executionSummary=new QLabel(m_executionTab);
    m_executionSummary->setObjectName(QStringLiteral("testSequenceExecutionPreviewSummary"));
    m_executionSummary->setWordWrap(true);
    executionLayout->addWidget(m_executionSummary);
    m_executionTable=new QTableWidget(m_executionTab);
    m_executionTable->setObjectName(QStringLiteral("testSequenceExecutionPreviewTable"));
    m_executionTable->setColumnCount(7);
    m_executionTable->setHorizontalHeaderLabels({QStringLiteral("#"),QStringLiteral("Group"),
        QStringLiteral("Operation (expanded)"),QStringLiteral("Procedure"),
        QStringLiteral("Resolved parameters"),QStringLiteral("Acceptance checks"),
        QStringLiteral("Authored source")});
    m_executionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_executionTable->horizontalHeader()->setStretchLastSection(true);
    m_executionTable->setColumnWidth(0,45);m_executionTable->setColumnWidth(1,72);
    m_executionTable->setColumnWidth(2,295);m_executionTable->setColumnWidth(3,150);
    m_executionTable->setColumnWidth(4,240);m_executionTable->setColumnWidth(5,330);
    m_executionTable->verticalHeader()->setVisible(false);
    m_executionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_executionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_executionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_executionTable->setAlternatingRowColors(true);
    m_executionTable->setSortingEnabled(false); // execution order is never sorted
    m_executionTable->setWordWrap(false);
    m_executionTable->setMinimumHeight(220);
    executionLayout->addWidget(m_executionTable,1);
    m_executionVirtualTable=new QTableView(m_executionTab);
    m_executionVirtualTable->setObjectName(QStringLiteral("testSequenceVirtualExecutionPreviewTable"));
    m_executionVirtualModel=new ExpandedPreviewModel(m_executionVirtualTable);
    m_executionVirtualTable->setModel(m_executionVirtualModel);
    m_executionVirtualTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_executionVirtualTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_executionVirtualTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_executionVirtualTable->setAlternatingRowColors(true);
    m_executionVirtualTable->verticalHeader()->hide();
    m_executionVirtualTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_executionVirtualTable->verticalHeader()->setDefaultSectionSize(27);
    m_executionVirtualTable->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_executionVirtualTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_executionVirtualTable->horizontalHeader()->setStretchLastSection(true);
    for(int column=0;column<6;++column)
        m_executionVirtualTable->setColumnWidth(column,
            QVector<int>{45,72,295,150,240,330}.at(column));
    executionLayout->addWidget(m_executionVirtualTable,1);
    m_executionVirtualTable->hide();
    auto* executionHelp=new QLabel(QStringLiteral(
        "Preview only — no JavaScript or instruments run. Numbers are execution order, not authored row numbers. "
        "Each sweep setpoint and repeat iteration becomes a separate step with its own parameters and limits. "
        "Array-wide [*] checks are evaluated when runtime data arrives."),m_executionTab);
    executionHelp->setWordWrap(true);
    executionLayout->addWidget(executionHelp);
    m_tabs->addTab(m_executionTab,QStringLiteral("Execution Preview"));
    connect(executePreviewButton,&QPushButton::clicked,this,[this]{
        const bool selectedContainer=containerNode(nodeAt(m_selectedPath));
        {
            const QSignalBlocker blocker(m_executionScope);
            m_executionScope->setCurrentIndex(selectedContainer?1:0);
        }
        m_tabs->setCurrentWidget(m_executionTab);
        showExecutionPreview();
    });
    connect(m_executionScope,QOverload<int>::of(&QComboBox::currentIndexChanged),this,
        [this](int){showExecutionPreview();});
    connect(refreshExecution,&QPushButton::clicked,this,[this]{showExecutionPreview();});
    connect(backToTree,&QPushButton::clicked,this,[this,editTab]{m_tabs->setCurrentWidget(editTab);});
    // Named Result Rules: a straightforward table, not JSON syntax and not a
    // popup. Selecting a sequence row chooses the rules shown on this tab.
    auto* rulesTab=new QWidget(m_tabs);
    auto* rulesLayout=new QVBoxLayout(rulesTab);
    m_rulesHeading=new QLabel(QStringLiteral("Select a numeric step on Step Details, then define the fields returned by its procedure."),rulesTab);
    m_rulesHeading->setWordWrap(true);rulesLayout->addWidget(m_rulesHeading);
    auto* rulesHelp=new QLabel(QStringLiteral(
        "Result paths: vpp, frequency, metrics.rms, voltages[0], or temperatures[*] (every item). "
        "Low/High may be numbers or sweep expressions such as amplitude * 0.85. "
        "Only named fields with rules are evaluated; extra fields are retained in the report. "
        "One numeric step can report multiple PASS/FAIL measurements."),rulesTab);
    rulesHelp->setWordWrap(true);rulesLayout->addWidget(rulesHelp);
    m_rulesTable=new QTableWidget(rulesTab);
    m_rulesTable->setObjectName(QStringLiteral("testSequenceNamedResultsTable"));
    m_rulesTable->setColumnCount(6);
    m_rulesTable->setHorizontalHeaderLabels({QStringLiteral("Result field / path"),QStringLiteral("Display name"),
        QStringLiteral("Lower limit"),QStringLiteral("Upper limit"),QStringLiteral("Unit"),QStringLiteral("Result widget ID (optional)")});
    m_rulesTable->horizontalHeader()->setStretchLastSection(true);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_rulesTable->setColumnWidth(0,185);m_rulesTable->setColumnWidth(1,230);
    m_rulesTable->setColumnWidth(2,130);m_rulesTable->setColumnWidth(3,130);
    m_rulesTable->setColumnWidth(4,100);
    m_rulesTable->setAlternatingRowColors(true);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    rulesLayout->addWidget(m_rulesTable,1);
    auto* rulesButtons=new QHBoxLayout;
    auto* addRule=new QPushButton(QStringLiteral("+ Add measurement"),rulesTab);
    addRule->setObjectName(QStringLiteral("testSequenceAddResultRule"));
    auto* removeRule=new QPushButton(QStringLiteral("Remove selected"),rulesTab);
    removeRule->setObjectName(QStringLiteral("testSequenceRemoveResultRule"));
    auto* returnSteps=new QPushButton(QStringLiteral("Back to Step Details"),rulesTab);
    rulesButtons->addWidget(addRule);rulesButtons->addWidget(removeRule);
    rulesButtons->addStretch();rulesButtons->addWidget(returnSteps);
    rulesLayout->addLayout(rulesButtons);
    m_tabs->addTab(rulesTab,QStringLiteral("Named Results & Limits"));
    connect(openNamed,&QPushButton::clicked,this,[this,rulesTab]{
        if(m_type->currentText()==QLatin1String("numeric")) {
            m_structuredResults->setChecked(true);
            m_tabs->setCurrentWidget(rulesTab);
        }
    });
    connect(returnSteps,&QPushButton::clicked,this,[this,editTab]{m_tabs->setCurrentWidget(editTab);});
    connect(addRule,&QPushButton::clicked,this,[this]{
        const int row=m_rulesTable->rowCount();
        m_rulesTable->insertRow(row);
        const QStringList defaults={row==0?QStringLiteral("value"):QStringLiteral("metric%1").arg(row+1),
            QStringLiteral("Measurement %1").arg(row+1),QStringLiteral("0"),QStringLiteral("1"),QString(),QString()};
        for(int i=0;i<defaults.size();++i)m_rulesTable->setItem(row,i,new QTableWidgetItem(defaults.at(i)));
        m_rulesTable->setCurrentCell(row,0);
        m_modified=true;m_stepDirty=true;
    });
    connect(removeRule,&QPushButton::clicked,this,[this]{
        const int row=m_rulesTable->currentRow();
        if(row>=0){m_rulesTable->removeRow(row);m_modified=true;m_stepDirty=true;}
    });
    connect(m_rulesTable,&QTableWidget::itemChanged,this,[this]{if(!m_loading)m_modified=true;});
    connect(m_structuredResults,&QCheckBox::toggled,this,[this]{
        setModeVisibility();if(!m_loading)m_modified=true;
    });


    // Project-owned UUT schema, embedded directly instead of another popup.
    auto* runTab=new QWidget(m_tabs);
    auto* runLayout=new QVBoxLayout(runTab);
    m_runEditor=new RunInformationEditor(m_runInformation,runTab);
    m_runEditor->setWindowFlags(Qt::Widget);
    if(auto* runButtons=m_runEditor->findChild<QDialogButtonBox*>())runButtons->hide();
    runLayout->addWidget(m_runEditor);
    // Declared test variables make the IF picker discoverable. Undeclared
    // variables created by JavaScript still work but are not listed ahead of RUN.
    auto* variablesTab=new QWidget(m_tabs);
    auto* variablesLayout=new QVBoxLayout(variablesTab);
    variablesLayout->addWidget(new QLabel(QStringLiteral("Declare values that test scripts may read or set using TestRunner.variable(name) and TestRunner.setVariable(name, value). Optional initial values are restored at each RUN."),variablesTab));
    m_variablesTable=new QTableWidget(variablesTab);
    m_variablesTable->setObjectName(QStringLiteral("testSequenceVariableCatalog"));
    m_variablesTable->setColumnCount(4);
    m_variablesTable->setHorizontalHeaderLabels({QStringLiteral("Name"),QStringLiteral("Type"),QStringLiteral("Initial value (optional)"),QStringLiteral("Description")});
    m_variablesTable->horizontalHeader()->setStretchLastSection(true);
    m_variablesTable->setColumnWidth(0,220);m_variablesTable->setColumnWidth(1,150);
    m_variablesTable->setColumnWidth(2,200);
    variablesLayout->addWidget(m_variablesTable,1);
    auto* variableActions=new QHBoxLayout;
    auto* addVariable=new QPushButton(QStringLiteral("+ Add variable"),variablesTab);
    auto* removeVariable=new QPushButton(QStringLiteral("Remove selected"),variablesTab);
    variableActions->addWidget(addVariable);variableActions->addWidget(removeVariable);variableActions->addStretch();
    variablesLayout->addLayout(variableActions);
    connect(addVariable,&QPushButton::clicked,this,[this]{
        const int row=m_variablesTable->rowCount();m_variablesTable->insertRow(row);
        m_variablesTable->setItem(row,0,new QTableWidgetItem(QStringLiteral("temperature")));
        auto* type=new QComboBox(m_variablesTable);type->addItems({QStringLiteral("number"),QStringLiteral("string"),QStringLiteral("boolean")});
        m_variablesTable->setCellWidget(row,1,type);
        m_variablesTable->setItem(row,2,new QTableWidgetItem(QString()));
        m_variablesTable->setItem(row,3,new QTableWidgetItem(QStringLiteral("Measured by a test procedure")));
        connect(type,&QComboBox::currentTextChanged,this,[this]{m_variablesDirty=true;m_modified=true;});
        m_variablesDirty=true;m_modified=true;
    });
    connect(removeVariable,&QPushButton::clicked,this,[this]{
        const int row=m_variablesTable->currentRow();if(row>=0){m_variablesTable->removeRow(row);m_variablesDirty=true;m_modified=true;}
    });
    connect(m_variablesTable,&QTableWidget::itemChanged,this,[this]{if(!m_loading){m_variablesDirty=true;m_modified=true;}});
    m_tabs->addTab(variablesTab,QStringLiteral("Test Variables"));
    m_tabs->addTab(runTab,QStringLiteral("Run Information"));

    auto* reportTab=new QWidget(m_tabs);
    auto* reportLayout=new QVBoxLayout(reportTab);
    auto* reportForm=new QFormLayout;
    m_reportAutoSave=new QCheckBox(QStringLiteral("Archive a JSON report after each completed test"),reportTab);
    m_reportAutoSave->setChecked(TestReportStorage::enabled(m_reportStorage));
    reportForm->addRow(m_reportAutoSave);
    auto* directoryRow=new QWidget(reportTab);
    auto* directoryLayout=new QHBoxLayout(directoryRow);
    directoryLayout->setContentsMargins(0,0,0,0);
    m_reportDirectory=new QLineEdit(reportTab);
    m_reportDirectory->setObjectName(QStringLiteral("testSequenceReportDirectory"));
    m_reportDirectory->setPlaceholderText(QStringLiteral("Default: Documents/WFScript Reports"));
    m_reportDirectory->setText(m_reportStorage.value(QStringLiteral("directory")).toString());
    auto* browse=new QPushButton(QStringLiteral("Browse…"),directoryRow);
    directoryLayout->addWidget(m_reportDirectory,1);directoryLayout->addWidget(browse);
    reportForm->addRow(QStringLiteral("Reports folder"),directoryRow);
    reportLayout->addLayout(reportForm);
    m_reportPath=new QLabel(reportTab);
    m_reportPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_reportPath->setWordWrap(true);
    reportLayout->addWidget(m_reportPath);
    auto* reportInfo=new QLabel(QStringLiteral("PASS, FAIL, ERROR and ABORTED reports are archived by the native Test Executive after Cleanup. Existing report files are not overwritten."),reportTab);
    reportInfo->setWordWrap(true);
    reportLayout->addWidget(reportInfo);
    reportLayout->addStretch();
    m_tabs->addTab(reportTab,QStringLiteral("Report Storage"));
    const auto updateReportPath=[this]{
        QJsonObject policy{{QStringLiteral("autoSave"),m_reportAutoSave->isChecked()},
                           {QStringLiteral("directory"),m_reportDirectory->text().trimmed()}};
        m_reportPath->setText(QStringLiteral("Resolved folder: %1").arg(TestReportStorage::directory(policy)));
    };
    connect(m_reportDirectory,&QLineEdit::textChanged,this,updateReportPath);
    connect(m_reportAutoSave,&QCheckBox::toggled,this,updateReportPath);
    connect(browse,&QPushButton::clicked,this,[this]{
        const QString folder=QFileDialog::getExistingDirectory(this,QStringLiteral("Choose reports directory"),
            TestReportStorage::directory(QJsonObject{{QStringLiteral("directory"),m_reportDirectory->text()}}));
        if(!folder.isEmpty())m_reportDirectory->setText(folder);
    });
    updateReportPath();

    auto* settingsTab=new QWidget(m_tabs);
    auto* settingsLayout=new QVBoxLayout(settingsTab);
    auto* settingsForm=new QFormLayout;
    m_title=new QLineEdit(settingsTab);
    m_title->setObjectName(QStringLiteral("testSequenceTitle"));
    m_id=new QLineEdit(settingsTab);
    m_sequenceId=new QLineEdit(settingsTab);m_tableId=new QLineEdit(settingsTab);
    m_logId=new QLineEdit(settingsTab);m_summaryId=new QLineEdit(settingsTab);
    m_totalTimeout=new QSpinBox(settingsTab);
    m_totalTimeout->setRange(0,86400000);m_totalTimeout->setSuffix(QStringLiteral(" ms"));
    m_stopOnFailure=new QCheckBox(QStringLiteral("Stop Main on failure (Cleanup still runs)"),settingsTab);
    settingsForm->addRow(QStringLiteral("Test title"),m_title);
    settingsForm->addRow(QStringLiteral("Definition ID"),m_id);
    settingsForm->addRow(QStringLiteral("Total timeout"),m_totalTimeout);
    settingsForm->addRow(m_stopOnFailure);
    settingsForm->addRow(QStringLiteral("Sequence widget ID"),m_sequenceId);
    settingsForm->addRow(QStringLiteral("Results table ID"),m_tableId);
    settingsForm->addRow(QStringLiteral("Event log ID"),m_logId);
    settingsForm->addRow(QStringLiteral("Summary widget ID"),m_summaryId);
    settingsLayout->addLayout(settingsForm);
    settingsLayout->addStretch();
    m_tabs->addTab(settingsTab,QStringLiteral("Sequence Settings"));

    auto* codeTab=new QWidget(m_tabs);
    auto* libraryLayout=new QVBoxLayout(codeTab);
    auto* codeInfo=new QLabel(QStringLiteral("Embedded test library: %1 — select a step, then use Edit procedure/poll code to jump to its function. The entire library is editable here; Save to Panel stores code and definition together. Code is not executed while editing.").arg(m_libraryName.isEmpty()?QStringLiteral("(no library selected)"):m_libraryName),codeTab);
    codeInfo->setWordWrap(true);libraryLayout->addWidget(codeInfo);
    m_sourceEditor=new ScriptEditor(codeTab);
    m_sourceEditor->setObjectName(QStringLiteral("testSequenceLibrarySourceEditor"));
    m_sourceEditor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_sourceEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_sourceEditor->setPlainText(m_librarySource);
    m_sourceEditor->setReadOnly(m_libraryName.isEmpty());
    libraryLayout->addWidget(m_sourceEditor,1);
    m_tabs->addTab(codeTab,QStringLiteral("Test Library JavaScript"));

    m_preview=new QPlainTextEdit(m_tabs);
    m_preview->setObjectName(QStringLiteral("testDefinitionJsonPreview"));
    m_preview->setReadOnly(true);m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_tabs->addTab(m_preview,QStringLiteral("JSON Preview (read-only)"));

    auto* footer=new QHBoxLayout;
    auto* load=new QPushButton(QStringLiteral("Import .wftest.json…"),this);
    auto* exportButton=new QPushButton(QStringLiteral("Export .wftest.json…"),this);
    auto* preview=new QPushButton(QStringLiteral("JSON Preview"),this);
    preview->setObjectName(QStringLiteral("showTestDefinitionJsonButton"));
    auto* dataTools=new QHBoxLayout;
    auto* previewExpanded=new QPushButton(QStringLiteral("Preview Executed Steps"),this);
    previewExpanded->setObjectName(QStringLiteral("previewExpandedTestLoops"));
    dataTools->addWidget(load);dataTools->addWidget(exportButton);
    dataTools->addWidget(preview);dataTools->addWidget(previewExpanded);
    dataTools->addStretch();
    connect(previewExpanded,&QPushButton::clicked,this,[this]{
        {
            const QSignalBlocker blocker(m_executionScope);
            m_executionScope->setCurrentIndex(0);
        }
        m_tabs->setCurrentWidget(m_executionTab);
        showExecutionPreview();
    });
    settingsLayout->insertLayout(1,dataTools);
    auto* saveAdvice=new QLabel(QStringLiteral("Save to Panel, then save the .wfpanel project to disk."),this);
    saveAdvice->setWordWrap(true);
    footer->addWidget(saveAdvice,1);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    actions->setObjectName(QStringLiteral("testSequenceEditorActions"));
    actions->button(QDialogButtonBox::Save)->setText(QStringLiteral("Save to Panel"));
    actions->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Close without saving"));
    footer->addWidget(actions);outer->addLayout(footer);
    connect(add,&QPushButton::clicked,this,[this]{addStep();});
    connect(addSweep,&QPushButton::clicked,this,[this]{addContainer(false);});
    connect(addRepeat,&QPushButton::clicked,this,[this]{addContainer(true);});
    connect(addChildButton,&QPushButton::clicked,this,[this]{addChild();});
    connect(addIf,&QPushButton::clicked,this,[this]{addConditional();});
    connect(addElse,&QPushButton::clicked,this,[this]{addElseBranch();});
    connect(indent,&QPushButton::clicked,this,[this]{indentStep();});
    connect(outdent,&QPushButton::clicked,this,[this]{outdentStep();});
    connect(duplicate,&QPushButton::clicked,this,[this]{duplicateStep();});
    connect(remove,&QPushButton::clicked,this,[this]{removeStep();});
    connect(up,&QPushButton::clicked,this,[this]{moveStep(-1);});
    connect(down,&QPushButton::clicked,this,[this]{moveStep(1);});
    connect(apply,&QPushButton::clicked,this,[this]{
        QString error;
        if(!applyStep(&error))QMessageBox::warning(this,QStringLiteral("Invalid step"),error);
        else populatePath(m_selectedPath);
    });
    connect(m_table,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem* item,QTreeWidgetItem*){
        if(m_loading)return;
        const QVector<int> requested=item?pathForItem(item):QVector<int>{};
        QString error;
        if(!applyStep(&error)){
            QMessageBox::warning(this,QStringLiteral("Invalid step"),error);
            selectPath(m_selectedPath);return;
        }
        loadPath(requested);
        // Do not destroy QTreeWidgetItems synchronously inside Qt's
        // currentItemChanged delivery; rebuild after the selection event.
        QTimer::singleShot(0,this,[this,requested]{
            if(m_selectedPath==requested)populatePath(requested);
        });
    });
    connect(m_table,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int column){
        if(!item || column!=1)return;
        const QVector<int> path=pathForItem(item);
        if(path.isEmpty())return;
        QString error;if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
        openFunction(procedureSummary(nodeAt(path)).remove(QLatin1String("()")));
    });
    connect(m_table,&QTreeWidget::itemCollapsed,this,[this](QTreeWidgetItem* item){
        if(item && !item->data(0,Qt::UserRole).toString().isEmpty())
            m_collapsedIds.insert(nodeAt(pathForItem(item)).value(QStringLiteral("id")).toString());
    });
    connect(m_table,&QTreeWidget::itemExpanded,this,[this](QTreeWidgetItem* item){
        if(item && !item->data(0,Qt::UserRole).toString().isEmpty())
            m_collapsedIds.remove(nodeAt(pathForItem(item)).value(QStringLiteral("id")).toString());
    });
    connect(m_mode,&QComboBox::currentTextChanged,this,[this]{setModeVisibility();});
    connect(m_type,&QComboBox::currentTextChanged,this,[this]{setModeVisibility();});
    for(const auto& pair:std::initializer_list<std::pair<const char*,QComboBox*>>{
            {"testSequenceOpenOnceCode",m_handler},
            {"testSequenceOpenProcedureCode",m_start},
            {"testSequenceOpenPollCode",m_poll},
            {"testSequenceOpenFinishCode",m_finish}}) {
        auto* button=findChild<QPushButton*>(QString::fromLatin1(pair.first));
        const QString role=QString::fromLatin1(pair.first);
        connect(button,&QPushButton::clicked,this,[this,combo=pair.second,role]{
            editHandler(combo,role);
        });
    }
    connect(actions,&QDialogButtonBox::accepted,this,[this]{acceptChanges();});
    connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(load,&QPushButton::clicked,this,[this]{importJson();});
    connect(exportButton,&QPushButton::clicked,this,[this]{exportJson();});
    connect(preview,&QPushButton::clicked,this,[this]{showPreview();m_tabs->setCurrentWidget(m_preview);});
    connect(m_tabs,&QTabWidget::currentChanged,this,[this](int index){
        if(m_tabs->widget(index)==m_preview)showPreview();
        else if(m_tabs->widget(index)==m_executionTab)showExecutionPreview();
    });
    loadDocument();
    // Track operator edits, not programmatic field loading on row selection.
    const auto markChanged=[this]{if(!m_loading)m_modified=true;};
    for(auto* field:{m_stepId,m_stepName,m_numericUnit,m_numericWidget,m_id,
        m_title,m_sequenceId,m_tableId,m_logId,m_summaryId,m_reportDirectory})
        connect(field,&QLineEdit::textEdited,this,markChanged);
    connect(m_description,&QPlainTextEdit::textChanged,this,markChanged);
    connect(m_parameters,&QPlainTextEdit::textChanged,this,markChanged);
    for(auto* field:{m_loopId,m_loopName,m_loopIndex,m_limitParameter})
        connect(field,&QLineEdit::textEdited,this,markChanged);
    connect(m_loopDescription,&QPlainTextEdit::textChanged,this,markChanged);
    connect(m_axisTable,&QTableWidget::itemChanged,this,[this]{if(!m_loading)m_modified=true;});
    connect(m_loopKind,&QComboBox::currentTextChanged,this,markChanged);
    connect(m_sweepRepeat,&QCheckBox::toggled,this,markChanged);
    connect(m_relativeLimits,&QCheckBox::toggled,this,[this,markChanged]{setModeVisibility();markChanged();});
    connect(m_loopRepeatCount,QOverload<int>::of(&QSpinBox::valueChanged),this,markChanged);
    for(auto* spin:{m_lowOffset,m_highOffset})
        connect(spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,markChanged);
    connect(m_sourceEditor,&QPlainTextEdit::textChanged,this,markChanged);
    for(auto* combo:{m_group,m_type,m_mode,m_handler,m_start,m_poll,m_finish})
        connect(combo,&QComboBox::currentTextChanged,this,markChanged);
    for(auto* spin:{m_timeout,m_interval,m_duration,m_totalTimeout})
        connect(spin,QOverload<int>::of(&QSpinBox::valueChanged),this,markChanged);
    for(auto* spin:{m_limitLow,m_limitHigh})
        connect(spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,markChanged);
    const auto markStepChanged=[this]{if(!m_loading){m_stepDirty=true;m_modified=true;}};
    for(auto* field:{m_stepId,m_stepName,m_numericUnit,m_numericWidget,
                     m_loopId,m_loopName,m_loopIndex,m_limitParameter})
        connect(field,&QLineEdit::textEdited,this,markStepChanged);
    for(auto* text:{m_description,m_parameters,m_loopDescription})
        connect(text,&QPlainTextEdit::textChanged,this,markStepChanged);
    for(auto* combo:{m_group,m_type,m_mode,m_handler,m_start,m_poll,m_finish,m_loopKind})
        connect(combo,&QComboBox::currentTextChanged,this,markStepChanged);
    for(auto* spin:{m_timeout,m_interval,m_duration,m_loopRepeatCount})
        connect(spin,QOverload<int>::of(&QSpinBox::valueChanged),this,markStepChanged);
    for(auto* spin:{m_limitLow,m_limitHigh,m_lowOffset,m_highOffset})
        connect(spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,markStepChanged);
    for(auto* checkbox:{m_relativeLimits,m_structuredResults,m_sweepRepeat})
        connect(checkbox,&QCheckBox::toggled,this,markStepChanged);
    connect(m_axisTable,&QTableWidget::itemChanged,this,[this]{if(!m_loading){m_stepDirty=true;m_modified=true;}});
    connect(m_rulesTable,&QTableWidget::itemChanged,this,[this]{if(!m_loading){m_stepDirty=true;m_modified=true;}});
    for(auto* field:{m_conditionName,m_conditionId,m_conditionPath,m_conditionValue})
        connect(field,&QLineEdit::textEdited,this,markStepChanged);
    for(auto* combo:{m_conditionSource,m_conditionKey,m_conditionStep,m_conditionOperator,m_conditionValueType})
        connect(combo,&QComboBox::currentTextChanged,this,markStepChanged);
    connect(m_conditionSource,&QComboBox::currentTextChanged,this,[this]{
        if(!m_loading)refreshConditionPicker();
    });
    connect(m_stopOnFailure,&QCheckBox::toggled,this,markChanged);
    connect(m_reportAutoSave,&QCheckBox::toggled,this,markChanged);
    if(auto* fields=m_runEditor->findChild<QTableWidget*>()){
        const auto runEdited=[this]{m_runInfoChanged=true;m_modified=true;};
        connect(fields,&QTableWidget::itemChanged,this,runEdited);
        connect(fields->model(),&QAbstractItemModel::rowsInserted,this,runEdited);
        connect(fields->model(),&QAbstractItemModel::rowsRemoved,this,runEdited);
        for(auto* combo:fields->findChildren<QComboBox*>())
            connect(combo,&QComboBox::currentTextChanged,this,runEdited);
        for(auto* check:fields->findChildren<QCheckBox*>())
            connect(check,&QCheckBox::toggled,this,runEdited);
    }
}

void TestSequenceEditor::reject() {
    QString error;
    const bool validRunInfo=!m_runInfoChanged || m_runEditor->applyFields(&error);
    const QJsonObject currentRunInfo=m_runInfoChanged?m_runEditor->schema():m_runInformation;
    const bool reportChanged=m_reportAutoSave->isChecked()!=TestReportStorage::enabled(m_reportStorage) ||
        m_reportDirectory->text().trimmed()!=m_reportStorage.value(QStringLiteral("directory")).toString().trimmed();
    if((m_modified || !validRunInfo || currentRunInfo!=m_runInformation || reportChanged ||
        (m_sourceEditor->document()->isModified() && m_sourceEditor->toPlainText()!=m_librarySource)) &&
        QMessageBox::question(this,QStringLiteral("Discard unsaved test changes?"),
            QStringLiteral("The Test Sequence Editor has changes that have not been saved to the Panel. Discard them?"),
            QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Discard)
        return;
    QDialog::reject();
}

void TestSequenceEditor::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if(!m_firstShow)return;
    m_firstShow=false;
    // Keep ordinary window chrome and restore/maximize controls. Never
    // re-maximize after the operator manually restores the window.
    QTimer::singleShot(0,this,[this]{
        if(isVisible() && !isMaximized())showMaximized();
    });
}

void TestSequenceEditor::loadDocument() {
    m_loading=true;
    m_variablesTable->setRowCount(0);
    for(const auto& entry:m_definition.value(QStringLiteral("variables")).toArray()) {
        const auto variable=entry.toObject();const int row=m_variablesTable->rowCount();m_variablesTable->insertRow(row);
        m_variablesTable->setItem(row,0,new QTableWidgetItem(variable.value(QStringLiteral("name")).toString()));
        auto* type=new QComboBox(m_variablesTable);type->addItems({QStringLiteral("number"),QStringLiteral("string"),QStringLiteral("boolean")});
        type->setCurrentText(variable.value(QStringLiteral("type")).toString(QStringLiteral("number")));
        m_variablesTable->setCellWidget(row,1,type);
        connect(type,&QComboBox::currentTextChanged,this,[this]{if(!m_loading){m_variablesDirty=true;m_modified=true;}});
        QString initial;if(variable.contains(QStringLiteral("initial"))) {
            const auto v=variable.value(QStringLiteral("initial"));initial=v.isString()?v.toString():v.isBool()?(v.toBool()?QStringLiteral("true"):QStringLiteral("false")):QString::number(v.toDouble(),'g',12);
        }
        m_variablesTable->setItem(row,2,new QTableWidgetItem(initial));
        m_variablesTable->setItem(row,3,new QTableWidgetItem(variable.value(QStringLiteral("description")).toString()));
    }
    m_variablesDirty=false;
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
QJsonObject TestSequenceEditor::nodeAt(const QVector<int>& path) const {
    if(path.isEmpty() || path.first()<0 || path.first()>=m_steps.size())return {};
    QJsonObject current=m_steps.at(path.first());
    for(int i=1;i<path.size();++i) {
        const QJsonArray children=current.value(QStringLiteral("steps")).toArray();
        if(path.at(i)<0||path.at(i)>=children.size())return {};
        current=children.at(path.at(i)).toObject();
    }
    return current;
}
bool TestSequenceEditor::writeNode(const QVector<int>& path,const QJsonObject& node) {
    if(path.isEmpty()||path.first()<0||path.first()>=m_steps.size())return false;
    if(path.size()==1){m_steps[path.first()]=node;return true;}
    QVector<int> parent=path;parent.removeLast();
    const QJsonObject oldParent=nodeAt(parent);
    QJsonArray children=oldParent.value(QStringLiteral("steps")).toArray();
    if(path.last()<0||path.last()>=children.size())return false;
    children.replace(path.last(),node);
    QJsonObject modified=oldParent;modified.insert(QStringLiteral("steps"),children);
    return writeNode(parent,modified);
}
QJsonArray TestSequenceEditor::siblingArray(const QVector<int>& path) const {
    if(path.size()<2){QJsonArray array;for(const auto& s:m_steps)array.append(s);return array;}
    QVector<int> parent=path;parent.removeLast();
    return nodeAt(parent).value(QStringLiteral("steps")).toArray();
}
bool TestSequenceEditor::writeSiblings(const QVector<int>& path,const QJsonArray& siblings) {
    if(path.size()<2){m_steps.clear();for(const auto& v:siblings)m_steps.append(v.toObject());return true;}
    QVector<int> parent=path;parent.removeLast();
    QJsonObject group=nodeAt(parent);
    if(!group.contains(QStringLiteral("steps")))return false;
    group.insert(QStringLiteral("steps"),siblings);
    return writeNode(parent,group);
}
void TestSequenceEditor::selectPath(const QVector<int>& path) {
    QTreeWidgetItem* match=nullptr;
    const QString encoded=pathString(path);
    std::function<void(QTreeWidgetItem*)> visit=[&](QTreeWidgetItem* item){
        if(item->data(0,Qt::UserRole).toString()==encoded && !encoded.isEmpty())match=item;
        for(int i=0;i<item->childCount();++i)visit(item->child(i));
    };
    for(int i=0;i<m_table->topLevelItemCount();++i)visit(m_table->topLevelItem(i));
    const QSignalBlocker blocker(m_table);
    if(match){
        QTreeWidgetItem* ancestor=match->parent();
        while(ancestor){ancestor->setExpanded(true);ancestor=ancestor->parent();}
        m_table->setCurrentItem(match,0);
        m_table->scrollToItem(match);
    } else m_table->setCurrentItem(nullptr);
}
void TestSequenceEditor::populatePath(const QVector<int>& path) {
    populateTable(path.isEmpty()?-1:path.first());
    if(!path.isEmpty()){selectPath(path);loadPath(path);}
}
void TestSequenceEditor::populateTable(int selected) {
    const QSignalBlocker blocker(m_table);
    m_loading=true;
    m_table->clear();
    auto makeSection=[this](const QString& group,const QString& title)->QTreeWidgetItem* {
        auto* item=new QTreeWidgetItem(m_table);
        item->setText(0,title);
        item->setFlags(Qt::ItemIsEnabled);
        QFont font=item->font(0);font.setBold(true);item->setFont(0,font);
        item->setData(0,Qt::UserRole,QString());
        item->setExpanded(true);
        item->setToolTip(0,group==QLatin1String("cleanup")
            ?QStringLiteral("Cleanup runs once, even when Main is stopped")
            :QStringLiteral("Steps in this section run in their displayed order"));
        return item;
    };
    QTreeWidgetItem* setup=makeSection(QStringLiteral("setup"),QStringLiteral("SETUP"));
    QTreeWidgetItem* main=makeSection(QStringLiteral("main"),QStringLiteral("MAIN"));
    QTreeWidgetItem* cleanup=makeSection(QStringLiteral("cleanup"),QStringLiteral("CLEANUP"));
    std::function<void(QTreeWidgetItem*,const QJsonObject&,const QVector<int>&)> append=
        [&](QTreeWidgetItem* parent,const QJsonObject& step,const QVector<int>& path) {
        const bool container=containerNode(step);
        auto* item=new QTreeWidgetItem(parent);
        const QStringList old=displayColumns(step);
        item->setText(0,step.value(QStringLiteral("name")).toString());
        item->setText(1,step.contains(QStringLiteral("condition"))?QStringLiteral("Evaluate recorded data"):
            step.value(QStringLiteral("otherwise")).toBool(false)?QStringLiteral("False branch"):
            container?axisOverview(step):old.value(2));
        item->setText(2,step.contains(QStringLiteral("condition"))?QStringLiteral("IF"):
            step.value(QStringLiteral("otherwise")).toBool(false)?QStringLiteral("ELSE"):
            container?(step.contains(QStringLiteral("loop"))?QStringLiteral("Sweep"):QStringLiteral("Repeat")):old.value(3));
        item->setText(3,container?(step.contains(QStringLiteral("condition"))?QStringLiteral("Decision"):QStringLiteral("Group")):old.value(4));
        item->setText(4,container?QStringLiteral("%1 child step(s)").arg(step.value(QStringLiteral("steps")).toArray().size()):old.value(5));
        item->setText(5,old.value(6));
        item->setText(6,step.value(QStringLiteral("id")).toString());
        item->setData(0,Qt::UserRole,pathString(path));
        item->setToolTip(0,container?QStringLiteral("Select to edit this parent above; expand to edit its indented child steps")
            :executionTooltip(step));
        for(int c=1;c<7;++c)item->setToolTip(c,item->toolTip(0));
        if(container) {
            QFont f=item->font(0);f.setBold(true);item->setFont(0,f);
            const QJsonArray children=step.value(QStringLiteral("steps")).toArray();
            for(int i=0;i<children.size();++i){QVector<int> childPath=path;childPath.append(i);
                append(item,children.at(i).toObject(),childPath);}
            item->setExpanded(!m_collapsedIds.contains(step.value(QStringLiteral("id")).toString()));
        }
    };
    for(int i=0;i<m_steps.size();++i) {
        const QJsonObject step=m_steps.at(i);
        const QString group=step.value(QStringLiteral("group")).toString(QStringLiteral("main"));
        QTreeWidgetItem* parent=group==QLatin1String("setup")?setup:
            group==QLatin1String("cleanup")?cleanup:main;
        append(parent,step,QVector<int>{i});
    }
    QJsonObject snapshot=m_definition;QJsonArray authored;
    for(const auto& a:m_steps)authored.append(a);
    snapshot.insert(QStringLiteral("steps"),authored);
    QJsonObject expanded;QString expansionError;
    const bool valid=TestSequenceLoops::expand(snapshot,&expanded,&expansionError);
    const int executable=valid?expanded.value(QStringLiteral("steps")).toArray().size():-1;
    m_countLabel->setText(executable>=0
        ?QStringLiteral("%1 authored rows  →  %2 executable steps   ·   Select a row to edit it above")
            .arg(m_steps.size()).arg(executable)
        :QStringLiteral("Sweep configuration needs attention: %1").arg(expansionError));
    m_loading=false;
    if(selected>=0 && selected<m_steps.size()){
        const QVector<int> path{selected};selectPath(path);loadPath(path);
    }else{m_selected=-1;m_selectedPath.clear();}
}
void TestSequenceEditor::loadStep(int row) {loadPath(QVector<int>{row});}
void TestSequenceEditor::addAxis(const QJsonObject& axis) {
    if(m_axisTable->rowCount()>=3)return;
    const int row=m_axisTable->rowCount();m_axisTable->insertRow(row);
    const bool values=axis.contains(QStringLiteral("values"));
    const QStringList parts={axis.value(QStringLiteral("parameter")).toString(),QString(),
        QString::number(axis.value(QStringLiteral("start")).toDouble(),'g',12),
        QString::number(axis.value(QStringLiteral("stop")).toDouble(),'g',12),
        QString::number(axis.value(QStringLiteral("increment")).toDouble(),'g',12),QString()};
    for(int c:{0,2,3,4,5})m_axisTable->setItem(row,c,new QTableWidgetItem(parts.at(c)));
    auto* kind=new QComboBox(m_axisTable);kind->addItems({QStringLiteral("Range"),QStringLiteral("Values")});
    kind->setCurrentText(values?QStringLiteral("Values"):QStringLiteral("Range"));
    m_axisTable->setCellWidget(row,1,kind);
    if(values){
        QStringList readable;
        for(const auto& v:axis.value(QStringLiteral("values")).toArray()){
            if(v.isString())readable<<v.toString();
            else readable<<QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
        }
        m_axisTable->item(row,5)->setText(readable.join(QStringLiteral(", ")));
    }
    connect(kind,&QComboBox::currentTextChanged,this,[this]{if(!m_loading){m_modified=true;m_stepDirty=true;}});
}
bool TestSequenceEditor::readAxes(QJsonObject* output,QString* error) const {
    if(m_axisTable->rowCount()<1||m_axisTable->rowCount()>3)
        return errorOut(error,QStringLiteral("A sweep needs one to three parameters"));
    QJsonArray axes;
    for(int r=0;r<m_axisTable->rowCount();++r) {
        auto cell=[this,r](int c){auto* item=m_axisTable->item(r,c);return item?item->text().trimmed():QString();};
        const QString parameter=cell(0);
        if(parameter.isEmpty())return errorOut(error,QStringLiteral("Sweep parameter name is required"));
        auto* kind=qobject_cast<QComboBox*>(m_axisTable->cellWidget(r,1));
        QJsonObject axis{{QStringLiteral("parameter"),parameter}};
        if(kind && kind->currentText()==QLatin1String("Values")) {
            QJsonArray values;
            const QString text=cell(5);
            if(text.startsWith(QLatin1Char('['))) {
                QJsonParseError parse;
                const QJsonDocument doc=QJsonDocument::fromJson(text.toUtf8(),&parse);
                if(parse.error!=QJsonParseError::NoError||!doc.isArray())
                    return errorOut(error,QStringLiteral("Invalid value list for %1").arg(parameter));
                values=doc.array();
            } else {
                for(const QString& raw:text.split(QLatin1Char(','),Qt::SkipEmptyParts)) {
                    const QString entry=raw.trimmed();
                    if(entry.isEmpty())continue;
                    bool ok=false;const double n=entry.toDouble(&ok);
                    if(ok && std::isfinite(n))values.append(n);
                    else if(entry==QLatin1String("true"))values.append(true);
                    else if(entry==QLatin1String("false"))values.append(false);
                    else values.append(entry);
                }
            }
            if(values.isEmpty())return errorOut(error,QStringLiteral("Enter one or more values for %1").arg(parameter));
            axis.insert(QStringLiteral("values"),values);
        } else {
            bool okStart=false,okStop=false,okInc=false;
            const double start=cell(2).toDouble(&okStart);
            const double stop=cell(3).toDouble(&okStop);
            const double inc=cell(4).toDouble(&okInc);
            if(!okStart||!okStop||!okInc||!std::isfinite(start)||!std::isfinite(stop)||!std::isfinite(inc)||inc==0)
                return errorOut(error,QStringLiteral("%1 needs numeric Start, Stop, and nonzero Increment").arg(parameter));
            axis.insert(QStringLiteral("start"),start);
            axis.insert(QStringLiteral("stop"),stop);
            axis.insert(QStringLiteral("increment"),inc);
        }
        axes.append(axis);
    }
    if(axes.size()==1)*output=axes.at(0).toObject();
    else *output=QJsonObject{{QStringLiteral("axes"),axes}};
    return true;
}
void TestSequenceEditor::setContainerVisibility() {
    if(!m_loopDetails)return;
    const bool sweep=m_loopKind->currentText()==QLatin1String("Sweep");
    m_axisTable->setEnabled(sweep);
    m_sweepRepeat->setEnabled(sweep);
    const bool repeats=!sweep||m_sweepRepeat->isChecked();
    m_loopRepeatCount->setEnabled(repeats);
    m_loopIndex->setEnabled(repeats);
    m_loopPreview->setText(sweep?QStringLiteral("Each combination runs all indented child steps in order."):
        QStringLiteral("Repeat all indented child steps the specified number of times."));
}
void TestSequenceEditor::loadPath(const QVector<int>& path) {
    m_selectedPath=path;m_stepDirty=false;
    if(path.isEmpty()) {m_selected=-1;m_detailColumns->hide();m_loopDetails->hide();m_conditionDetails->hide();return;}
    const QJsonObject step=nodeAt(path);
    m_selected=path.first();
    m_loading=true;
    const bool group=containerNode(step);
    const bool conditional=step.contains(QStringLiteral("condition"));
    const bool otherwise=step.value(QStringLiteral("otherwise")).toBool(false);
    m_detailColumns->setVisible(!group);
    m_loopDetails->setVisible(group && !conditional && !otherwise);
    m_conditionDetails->setVisible(conditional||otherwise);
    if(conditional||otherwise) {
        m_conditionName->setText(step.value(QStringLiteral("name")).toString());
        m_conditionId->setText(step.value(QStringLiteral("id")).toString());
        m_conditionEvaluation->setVisible(conditional);
        if(conditional) {
            const auto c=step.value(QStringLiteral("condition")).toObject();
            const QString source=c.value(QStringLiteral("source")).toString();
            const QStringList sources={QStringLiteral("variable"),QStringLiteral("parameter"),QStringLiteral("measurement"),
                QStringLiteral("stepStatus"),QStringLiteral("runInfo"),QStringLiteral("counter")};
            const int index=sources.indexOf(source);m_conditionSource->setCurrentIndex(qMax(0,index));
            refreshConditionPicker();
            m_conditionKey->setCurrentText(c.value(QStringLiteral("key")).toString());
            m_conditionStep->setCurrentText(c.value(QStringLiteral("step")).toString());
            m_conditionPath->setText(c.value(QStringLiteral("path")).toString());
            m_conditionOperator->setCurrentText(c.value(QStringLiteral("operator")).toString(QStringLiteral("<")));
            const auto value=c.value(QStringLiteral("value"));
            m_conditionValueType->setCurrentText(value.isString()?QStringLiteral("Text"):
                value.isBool()?QStringLiteral("Boolean"):QStringLiteral("Number"));
            m_conditionValue->setText(value.isString()?value.toString():
                value.isBool()?(value.toBool()?QStringLiteral("true"):QStringLiteral("false")):
                QString::number(value.toDouble(),'g',12));
        }
        m_stepOverview->setText(conditional?QStringLiteral("IF chooses THEN or ELSE at runtime; preview shows both paths."):
            QStringLiteral("ELSE runs only if its parent IF evaluates false."));
        m_loading=false;return;
    }
    if(group){
        m_loopId->setText(step.value(QStringLiteral("id")).toString());
        m_loopName->setText(step.value(QStringLiteral("name")).toString());
        m_loopDescription->setPlainText(step.value(QStringLiteral("description")).toString());
        const bool sweep=step.contains(QStringLiteral("loop"));
        m_loopKind->setCurrentText(sweep?QStringLiteral("Sweep"):QStringLiteral("Repeat"));
        const QJsonObject repeat=step.value(QStringLiteral("repeat")).toObject();
        m_sweepRepeat->setChecked(sweep && step.contains(QStringLiteral("repeat")));
        m_loopRepeatCount->setValue(repeat.value(QStringLiteral("count")).toInt(3));
        m_loopIndex->setText(repeat.value(QStringLiteral("index")).toString(QStringLiteral("iteration")));
        m_axisTable->setRowCount(0);
        if(sweep){
            const QJsonObject loop=step.value(QStringLiteral("loop")).toObject();
            const QJsonArray axes=loop.value(QStringLiteral("axes")).toArray();
            if(!axes.isEmpty())for(const auto& v:axes)addAxis(v.toObject());
            else addAxis(loop);
        }
        m_stepOverview->setText(QStringLiteral("%1  ·  %2 child procedure(s). Select an indented child to edit its function and limits.")
            .arg(axisOverview(step)).arg(step.value(QStringLiteral("steps")).toArray().size()));
        setContainerVisibility();
        m_loading=false;return;
    }
    m_stepId->setText(step.value(QStringLiteral("id")).toString());
    m_stepName->setText(step.value(QStringLiteral("name")).toString());
    m_description->setPlainText(step.value(QStringLiteral("description")).toString());
    m_parameters->setPlainText(jsonPretty(step.value(QStringLiteral("parameters")).toObject()));
    m_structuredResults->setChecked(step.contains(QStringLiteral("measurements")));
    m_rulesTable->setRowCount(0);
    const auto named=step.value(QStringLiteral("measurements")).toObject();
    m_rulesHeading->setText(QStringLiteral("%1 — %2 named check(s). The procedure returns a JSON result; native C++ evaluates the configured fields.")
        .arg(step.value(QStringLiteral("name")).toString()).arg(named.size()));
    for(auto it=named.begin();it!=named.end();++it){
        const int r=m_rulesTable->rowCount();m_rulesTable->insertRow(r);
        const auto rule=it.value().toObject();
        const QStringList cells={it.key(),rule.value(QStringLiteral("name")).toString(it.key()),
            displayRuleLimit(rule.value(QStringLiteral("low"))),
            displayRuleLimit(rule.value(QStringLiteral("high"))),
            rule.value(QStringLiteral("unit")).toString(),rule.value(QStringLiteral("resultWidget")).toString()};
        for(int c=0;c<cells.size();++c)m_rulesTable->setItem(r,c,new QTableWidgetItem(cells.at(c)));
    }
    const QString inheritedGroup=path.size()>1?QStringLiteral("main"):
        step.value(QStringLiteral("group")).toString(QStringLiteral("main"));
    m_group->setCurrentText(inheritedGroup);m_group->setEnabled(path.size()==1);
    m_type->setCurrentText(step.value(QStringLiteral("type")).toString(QStringLiteral("test")));
    m_mode->setCurrentText(step.value(QStringLiteral("mode")).toString(QStringLiteral("once")));
    const QJsonObject handlers=step.value(QStringLiteral("handlers")).toObject();
    m_handler->setCurrentText(step.value(QStringLiteral("handler")).toString().isEmpty() &&
        step.value(QStringLiteral("mode")).toString()==QLatin1String("callback")
        ?handlers.value(QStringLiteral("start")).toString():step.value(QStringLiteral("handler")).toString());
    m_start->setCurrentText(handlers.value(QStringLiteral("start")).toString());
    m_poll->setCurrentText(handlers.value(QStringLiteral("poll")).toString());
    m_finish->setCurrentText(handlers.value(QStringLiteral("finish")).toString());
    m_timeout->setValue(step.value(QStringLiteral("timeoutMs")).toInt());
    m_interval->setValue(step.value(QStringLiteral("intervalMs")).toInt(50));
    m_duration->setValue(step.value(QStringLiteral("durationMs")).toInt());
    const QJsonObject limits=step.value(QStringLiteral("limits")).toObject();
    const QJsonObject low=limits.value(QStringLiteral("low")).toObject();
    const QJsonObject high=limits.value(QStringLiteral("high")).toObject();
    const bool relative=!low.isEmpty() && !high.isEmpty() &&
        !low.value(QStringLiteral("param")).toString().isEmpty();
    m_relativeLimits->setChecked(relative);
    m_limitParameter->setText(low.value(QStringLiteral("param")).toString());
    m_lowOffset->setValue(low.value(QStringLiteral("offset")).toDouble());
    m_highOffset->setValue(high.value(QStringLiteral("offset")).toDouble());
    m_limitLow->setValue(limits.value(QStringLiteral("low")).toDouble());
    m_limitHigh->setValue(limits.value(QStringLiteral("high")).toDouble());
    m_numericUnit->setText(step.value(QStringLiteral("unit")).toString());
    m_numericWidget->setText(step.value(QStringLiteral("resultWidget")).toString());
    m_stepOverview->setText(QStringLiteral("Procedure: %1    |    %2")
        .arg(procedureSummary(step),step.value(QStringLiteral("description")).toString()));
    setModeVisibility();m_loading=false;
}
void TestSequenceEditor::setModeVisibility() {
    const QString mode=m_mode->currentText();
    m_handler->setEnabled(mode==QLatin1String("once") || mode==QLatin1String("callback"));
    m_poll->setEnabled(mode==QLatin1String("poll"));
    m_start->setEnabled(mode==QLatin1String("poll") || mode==QLatin1String("delay"));
    m_finish->setEnabled(mode!=QLatin1String("once"));
    m_interval->setEnabled(mode==QLatin1String("poll"));
    m_duration->setEnabled(mode==QLatin1String("delay"));
    for(const auto& pair:std::initializer_list<std::pair<const char*,bool>>{
            {"testSequenceOpenOnceCode",m_handler->isEnabled()},
            {"testSequenceOpenProcedureCode",m_start->isEnabled()},
            {"testSequenceOpenPollCode",m_poll->isEnabled()},
            {"testSequenceOpenFinishCode",m_finish->isEnabled()}}) {
        if(auto* button=findChild<QPushButton*>(QString::fromLatin1(pair.first)))
            button->setEnabled(pair.second);
    }
    const bool numeric=m_type->currentText()==QLatin1String("numeric");
    const bool scalar=numeric && !m_structuredResults->isChecked();
    m_structuredResults->setEnabled(numeric);
    m_relativeLimits->setEnabled(scalar);
    const bool relative=scalar && m_relativeLimits->isChecked();
    m_limitParameter->setEnabled(relative);
    m_lowOffset->setEnabled(relative);m_highOffset->setEnabled(relative);
    m_limitLow->setEnabled(scalar && !relative);m_limitHigh->setEnabled(scalar && !relative);
    m_numericUnit->setEnabled(scalar);m_numericWidget->setEnabled(scalar);
    if(auto* button=findChild<QPushButton*>(QStringLiteral("testSequenceOpenNamedResults")))
        button->setEnabled(numeric);
}
bool TestSequenceEditor::applyStep(QString* error) {
    if(m_selectedPath.isEmpty() || !m_stepDirty)return true;
    const QJsonObject original=nodeAt(m_selectedPath);
    if(containerNode(original) && (original.contains(QStringLiteral("condition")) ||
                                    original.value(QStringLiteral("otherwise")).toBool(false))) {
        QJsonObject updated=original;
        updated.insert(QStringLiteral("name"),m_conditionName->text().trimmed());
        updated.insert(QStringLiteral("id"),m_conditionId->text().trimmed());
        if(updated.value(QStringLiteral("name")).toString().isEmpty() || updated.value(QStringLiteral("id")).toString().isEmpty())
            return errorOut(error,QStringLiteral("IF/ELSE requires a name and ID"));
        if(updated.contains(QStringLiteral("condition"))) {
            const QStringList sources={QStringLiteral("variable"),QStringLiteral("parameter"),QStringLiteral("measurement"),
                QStringLiteral("stepStatus"),QStringLiteral("runInfo"),QStringLiteral("counter")};
            QJsonObject condition{{QStringLiteral("source"),sources.value(m_conditionSource->currentIndex())},
                {QStringLiteral("operator"),m_conditionOperator->currentText()}};
            const QString source=condition.value(QStringLiteral("source")).toString();
            if(source==QLatin1String("measurement") || source==QLatin1String("stepStatus")) {
                condition.insert(QStringLiteral("step"),m_conditionStep->currentText().trimmed());
                if(source==QLatin1String("measurement"))condition.insert(QStringLiteral("path"),m_conditionPath->text().trimmed());
            } else condition.insert(QStringLiteral("key"),m_conditionKey->currentText().trimmed());
            const QString kind=m_conditionValueType->currentText();
            if(kind==QLatin1String("Number")) {
                bool ok=false;const double number=m_conditionValue->text().trimmed().toDouble(&ok);
                if(!ok || !std::isfinite(number))return errorOut(error,QStringLiteral("Condition needs a finite numeric value"));
                condition.insert(QStringLiteral("value"),number);
            } else if(kind==QLatin1String("Boolean")) {
                const QString value=m_conditionValue->text().trimmed().toLower();
                if(value!=QLatin1String("true") && value!=QLatin1String("false"))
                    return errorOut(error,QStringLiteral("Boolean condition must use true or false"));
                condition.insert(QStringLiteral("value"),value==QLatin1String("true"));
            } else condition.insert(QStringLiteral("value"),m_conditionValue->text());
            QJsonArray declarations;
            if(!collectVariableDeclarations(&declarations,error))return false;
            if(!TestSequenceConditions::validate(condition,declarations,error))return false;
            updated.insert(QStringLiteral("condition"),condition);
        }
        if(!writeNode(m_selectedPath,updated))return errorOut(error,QStringLiteral("Cannot update IF/ELSE"));
        m_stepDirty=false;m_modified=true;return true;
    }
    if(containerNode(original)) {
        QJsonObject group=original;
        const QString id=m_loopId->text().trimmed();
        const QString name=m_loopName->text().trimmed();
        if(id.isEmpty()||name.isEmpty())return errorOut(error,QStringLiteral("Sweep/Repeat requires a name and ID"));
        group.insert(QStringLiteral("id"),id);
        group.insert(QStringLiteral("name"),name);
        group.insert(QStringLiteral("description"),m_loopDescription->toPlainText());
        const bool sweep=m_loopKind->currentText()==QLatin1String("Sweep");
        group.remove(QStringLiteral("loop"));group.remove(QStringLiteral("repeat"));
        if(sweep){
            QJsonObject loop;
            if(!readAxes(&loop,error))return false;
            group.insert(QStringLiteral("loop"),loop);
        }
        if(!sweep || m_sweepRepeat->isChecked()){
            const QString index=m_loopIndex->text().trimmed().isEmpty()
                ?QStringLiteral("iteration"):m_loopIndex->text().trimmed();
            group.insert(QStringLiteral("repeat"),QJsonObject{
                {QStringLiteral("count"),m_loopRepeatCount->value()},
                {QStringLiteral("index"),index}});
        }
        if(!writeNode(m_selectedPath,group))return errorOut(error,QStringLiteral("Cannot update group"));
        m_modified=true;m_stepDirty=false;
        return true;
    }
    QJsonObject step=original;
    step.insert(QStringLiteral("id"),m_stepId->text().trimmed());
    step.insert(QStringLiteral("name"),m_stepName->text().trimmed());
    step.insert(QStringLiteral("description"),m_description->toPlainText());
    QJsonParseError parameterError;
    const QByteArray paramBytes=m_parameters->toPlainText().trimmed().toUtf8();
    if(paramBytes.size()>8192)return errorOut(error,QStringLiteral("Step parameters exceed 8 KiB"));
    const auto parameters=QJsonDocument::fromJson(paramBytes.isEmpty()?QByteArray("{}"):paramBytes,&parameterError);
    if(parameterError.error!=QJsonParseError::NoError || !parameters.isObject())
        return errorOut(error,QStringLiteral("Step parameters must be a valid JSON object"));
    if(!parameters.object().isEmpty() || step.contains(QStringLiteral("parameters")))
        step.insert(QStringLiteral("parameters"),parameters.object());
    step.insert(QStringLiteral("group"),m_group->currentText());
    step.insert(QStringLiteral("type"),m_type->currentText());
    step.insert(QStringLiteral("mode"),m_mode->currentText());
    if(m_type->currentText()==QLatin1String("numeric")) {
        if(m_structuredResults->isChecked()) {
            QJsonObject named;
            if(m_rulesTable->rowCount()==0)
                return errorOut(error,QStringLiteral("Add at least one Named Result rule, or uncheck named results to use scalar limits"));
            for(int row=0;row<m_rulesTable->rowCount();++row) {
                auto cell=[this,row](int col)->QString {
                    auto* item=m_rulesTable->item(row,col);
                    return item?item->text().trimmed():QString();
                };
                const QString path=cell(0);
                if(path.isEmpty() || named.contains(path))
                    return errorOut(error,QStringLiteral("Each result path must be nonempty and unique (row %1)").arg(row+1));
                QJsonValue low,high;
                if(!parseRuleLimit(cell(2),&low)||!parseRuleLimit(cell(3),&high))
                    return errorOut(error,QStringLiteral("Named Result row %1: use a number or an expression such as amplitude * 0.85").arg(row+1));
                QJsonObject rule{{QStringLiteral("low"),low},{QStringLiteral("high"),high},
                                 {QStringLiteral("name"),cell(1)},{QStringLiteral("unit"),cell(4)}};
                if(!cell(5).isEmpty())rule.insert(QStringLiteral("resultWidget"),cell(5));
                named.insert(path,rule);
            }
            // Parameter-relative rules are resolved by TestSequenceLoops
            // before StructuredTestResults validates the numeric limits.
            step.insert(QStringLiteral("measurements"),named);
            step.remove(QStringLiteral("limits"));step.remove(QStringLiteral("unit"));
            step.remove(QStringLiteral("resultWidget"));
        } else {
            if(m_relativeLimits->isChecked()) {
                const QString param=m_limitParameter->text().trimmed();
                if(param.isEmpty()||m_lowOffset->value()>m_highOffset->value())
                    return errorOut(error,QStringLiteral("Relative limits require a sweep parameter and lower offset <= upper offset"));
                step.insert(QStringLiteral("limits"),QJsonObject{
                    {QStringLiteral("low"),QJsonObject{{QStringLiteral("param"),param},
                                                        {QStringLiteral("offset"),m_lowOffset->value()}}},
                    {QStringLiteral("high"),QJsonObject{{QStringLiteral("param"),param},
                                                         {QStringLiteral("offset"),m_highOffset->value()}}}});
            } else {
                if(m_limitLow->value()>m_limitHigh->value())
                    return errorOut(error,QStringLiteral("Numeric lower limit must be <= upper limit"));
                step.insert(QStringLiteral("limits"),QJsonObject{
                    {QStringLiteral("low"),m_limitLow->value()},
                    {QStringLiteral("high"),m_limitHigh->value()}});
            }
            step.insert(QStringLiteral("unit"),m_numericUnit->text().trimmed());
            step.insert(QStringLiteral("resultWidget"),m_numericWidget->text().trimmed());
            step.remove(QStringLiteral("measurements"));
        }
    } else {
        step.remove(QStringLiteral("measurements"));
        step.remove(QStringLiteral("limits"));step.remove(QStringLiteral("unit"));
        step.remove(QStringLiteral("resultWidget"));
    }
    step.remove(QStringLiteral("handler"));step.remove(QStringLiteral("handlers"));
    step.remove(QStringLiteral("intervalMs"));step.remove(QStringLiteral("durationMs"));
    if(m_timeout->value()>0)step.insert(QStringLiteral("timeoutMs"),m_timeout->value());
    else step.remove(QStringLiteral("timeoutMs"));
    if(m_mode->currentText()==QLatin1String("once") || m_mode->currentText()==QLatin1String("callback")) {
        if(m_handler->currentText().trimmed().isEmpty())return errorOut(error,QStringLiteral("Once/callback steps require an exported handler name"));
        if(m_mode->currentText()==QLatin1String("callback") && !m_finish->currentText().trimmed().isEmpty()) {
            step.insert(QStringLiteral("handlers"),QJsonObject{
                {QStringLiteral("start"),m_handler->currentText().trimmed()},
                {QStringLiteral("finish"),m_finish->currentText().trimmed()}});
        } else step.insert(QStringLiteral("handler"),m_handler->currentText().trimmed());
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
    // Nested child steps inherit Main from the parent. Do not change the
    // group's position or overwrite parameter-relative limits on selection.
    if(m_selectedPath.size()>1) {
        step.remove(QStringLiteral("group"));
        if(!writeNode(m_selectedPath,step))return errorOut(error,QStringLiteral("Cannot update nested step"));
    } else {
        const QString oldGroup=original.value(QStringLiteral("group")).toString(QStringLiteral("main"));
        const QString newGroup=step.value(QStringLiteral("group")).toString(QStringLiteral("main"));
        if(oldGroup!=newGroup){
            m_steps.removeAt(m_selectedPath.first());
            int target=0;
            while(target<m_steps.size() &&
                  groupRank(m_steps.at(target).value(QStringLiteral("group")).toString(QStringLiteral("main")))<=groupRank(newGroup))
                ++target;
            m_steps.insert(target,step);
            m_selectedPath=QVector<int>{target};
            m_selected=target;
            m_stepDirty=false;
            const QVector<int> moved=m_selectedPath;
            QTimer::singleShot(0,this,[this,moved]{populatePath(moved);});
            return true;
        }
        writeNode(m_selectedPath,step);
    }
    m_stepOverview->setText(QStringLiteral("Procedure: %1    |    %2")
        .arg(procedureSummary(step),step.value(QStringLiteral("description")).toString()));
    m_stepDirty=false;
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
    QJsonArray variableDefinitions;
    if(!collectVariableDeclarations(&variableDefinitions,error))return false;
    if(!variableDefinitions.isEmpty() || doc.contains(QStringLiteral("variables")))
        doc.insert(QStringLiteral("variables"),variableDefinitions);
    if(!validateDefinition(doc,error))return false;
    if(out)*out=doc;
    return true;
}
void TestSequenceEditor::editHandler(QComboBox* combo,const QString& role) {
    if(!combo || m_libraryName.isEmpty()) {
        QMessageBox::warning(this,QStringLiteral("No test library"),
            QStringLiteral("Select an embedded test library before editing handlers."));
        return;
    }
    QString name=combo->currentText().trimmed();
    if(name.isEmpty()) {
        bool ok=false;
        name=QInputDialog::getText(this,QStringLiteral("Create a handler"),
            QStringLiteral("Exported JavaScript function name:"),QLineEdit::Normal,
            m_stepId->text().trimmed()+QStringLiteral("Handler"),&ok).trimmed();
        if(!ok || name.isEmpty())return;
    }
    const QRegularExpression identifier(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
    if(!identifier.match(name).hasMatch()) {
        QMessageBox::warning(this,QStringLiteral("Invalid function name"),
            QStringLiteral("Use a valid JavaScript identifier, without parentheses."));
        return;
    }
    const QRegularExpression declaration(QStringLiteral(
        R"((?:\bexport\s+)?(?:(?:\basync\s+)?\bfunction\s+%1\s*\(|\b(?:const|let|var)\s+%1\s*=))")
        .arg(QRegularExpression::escape(name)));
    if(!declaration.match(m_sourceEditor->toPlainText()).hasMatch()) {
        if(QMessageBox::question(this,QStringLiteral("Create missing handler?"),
            QStringLiteral("%1() is not defined in %2. Create an exported function stub?")
                .arg(name,m_libraryName),QMessageBox::Yes|QMessageBox::No,
                QMessageBox::Yes)!=QMessageBox::Yes)return;
        QString stub;
        if(role==QLatin1String("testSequenceOpenPollCode")) {
            stub=QStringLiteral("export function %1(elapsedMs, runElapsedMs, step) {\n"
                "    // Return undefined while pending, or the measurement/boolean when ready.\n"
                "    return undefined;\n}\n").arg(name);
        } else if(role==QLatin1String("testSequenceOpenFinishCode")) {
            stub=QStringLiteral("export function %1(status, elapsedMs, step) {\n"
                "    // Optional post-step notification.\n}\n").arg(name);
        } else if(m_mode->currentText()==QLatin1String("callback")) {
            stub=QStringLiteral("export function %1(step) {\n"
                "    // Start an asynchronous operation using step.params.\n"
                "    // In its callback: if (!TestRunner.isActive(step.token)) return;\n"
                "    // For scalar steps, complete with a number.\n"
                "    // For named results, return {vpp: measuredVpp, frequency: measuredHz}.\n"
                "    // Configure those result fields and limits in Named Results & Limits.\n"
                "    // On success: TestRunner.complete(step.token, result);\n"
                "    // On error:   TestRunner.fail(step.token, errorMessage);\n}\n").arg(name);
        } else {
            stub=QStringLiteral("export function %1(step) {\n"
                "    // Use step.params to configure the operation.\n"
                "    return true;\n}\n").arg(name);
        }
        QTextCursor tail=m_sourceEditor->textCursor();
        tail.movePosition(QTextCursor::End);m_sourceEditor->setTextCursor(tail);
        m_sourceEditor->insertPlainText(QStringLiteral("\n")+stub);
        if(!m_exports.contains(name)) {
            m_exports.append(name);
            for(auto* item:{m_handler,m_start,m_poll,m_finish})
                if(item->findText(name)==-1)item->addItem(name);
        }
    }
    combo->setCurrentText(name);
    openFunction(name);
}

void TestSequenceEditor::openFunction(const QString& name) {
    if(name.isEmpty()){
        QMessageBox::information(this,QStringLiteral("No handler"),
            QStringLiteral("Choose an exported function in the step first."));
        return;
    }
    m_tabs->setCurrentWidget(m_sourceEditor->parentWidget());
    const QRegularExpression pattern(QStringLiteral(
        R"((?:\bexport\s+)?(?:(?:\basync\s+)?\bfunction\s+%1\s*\(|\b(?:const|let|var)\s+%1\s*=))")
        .arg(QRegularExpression::escape(name)));
    const auto match=pattern.match(m_sourceEditor->toPlainText());
    if(!match.hasMatch()){
        QMessageBox::information(this,QStringLiteral("Function not found"),
            QStringLiteral("%1() is not declared as a function in %2. Check that the selected handler is exported by the test library.")
                .arg(name,m_libraryName));
        return;
    }
    QTextCursor cursor=m_sourceEditor->textCursor();
    cursor.setPosition(match.capturedStart());
    m_sourceEditor->setTextCursor(cursor);
    m_sourceEditor->centerCursor();
    m_sourceEditor->setFocus();
}

// Preview generated steps using the same validated native expander as RUN.
// This view is never serialized into the project or used as an editor.

// Virtual read-only preview for large expansions. No QTableWidgetItem is
// allocated per cell, so a 10k-step preview remains scrollable and responsive.
void TestSequenceEditor::showExecutionPreview() {
    if(!m_executionTable || !m_executionSummary)return;
    QJsonObject authored,expanded;
    QString error;
    m_executionTable->setRowCount(0);
    static_cast<ExpandedPreviewModel*>(m_executionVirtualModel)->replace({});
    m_executionVirtualTable->hide();m_executionTable->show();
    if(!collectDocument(&authored,&error) || !TestSequenceLoops::expand(authored,&expanded,&error)) {
        m_executionSummary->setText(QStringLiteral("Cannot preview: %1. Correct the selected step and press Refresh.").arg(error));
        return;
    }

    // Link expanded leaf steps to their original place in the authored tree.
    // templateId is supplied by the existing expander for repeated leaves.
    QHash<QString,QString> sourceNames;
    std::function<void(const QJsonObject&,QStringList)> recordSources;
    recordSources=[&](const QJsonObject& node,QStringList parents) {
        const QString name=node.value(QStringLiteral("name")).toString();
        if(containerNode(node)) {
            if(node.contains(QStringLiteral("condition")))sourceNames.insert(node.value(QStringLiteral("id")).toString(),
                (parents+QStringList{name}).join(QStringLiteral(" / ")));
            parents.append(name);
            for(const QJsonValue& child:node.value(QStringLiteral("steps")).toArray())
                recordSources(child.toObject(),parents);
        } else {
            sourceNames.insert(node.value(QStringLiteral("id")).toString(),
                (parents+QStringList{name}).join(QStringLiteral(" / ")));
        }
    };
    for(const QJsonValue& node:authored.value(QStringLiteral("steps")).toArray())
        recordSources(node.toObject(),{});

    QSet<QString> selectedLeafIds;
    const bool requestedSelection=m_executionScope && m_executionScope->currentIndex()==1;
    if(requestedSelection && !m_selectedPath.isEmpty()) {
        std::function<void(const QJsonObject&)> collectLeafIds;
        collectLeafIds=[&](const QJsonObject& node) {
            if(containerNode(node)) {
                if(node.contains(QStringLiteral("condition")))selectedLeafIds.insert(node.value(QStringLiteral("id")).toString());
                for(const QJsonValue& child:node.value(QStringLiteral("steps")).toArray())
                    collectLeafIds(child.toObject());
            } else selectedLeafIds.insert(node.value(QStringLiteral("id")).toString());
        };
        collectLeafIds(nodeAt(m_selectedPath));
    }

    const QJsonArray steps=expanded.value(QStringLiteral("steps")).toArray();
    QVector<QStringList> previewRows;
    previewRows.reserve(steps.size());
    for(int index=0;index<steps.size();++index) {
        const QJsonObject step=steps.at(index).toObject();
        const QString id=step.value(QStringLiteral("id")).toString();
        const QString templateId=step.value(QStringLiteral("templateId")).toString(id);
        if(requestedSelection && !selectedLeafIds.isEmpty() && !selectedLeafIds.contains(templateId))continue;
        const QJsonObject params=step.value(QStringLiteral("parameters")).toObject();
        QStringList parameterTexts;
        for(auto it=params.begin();it!=params.end();++it) {
            QString value;
            if(it.value().isString())value=it.value().toString();
            else if(it.value().isDouble())value=QString::number(it.value().toDouble(),'g',12);
            else if(it.value().isBool())value=it.value().toBool()?QStringLiteral("true"):QStringLiteral("false");
            else value=QString::fromUtf8(QJsonDocument(QJsonArray{it.value()}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
            parameterTexts<<it.key()+QStringLiteral(" = ")+value;
        }
        const QString paramText=parameterTexts.join(QStringLiteral("  ·  "));
        QString acceptance=QStringLiteral("—");
        if(step.value(QStringLiteral("type")).toString()==QLatin1String("numeric")) {
            const QJsonObject named=step.value(QStringLiteral("measurements")).toObject();
            if(!named.isEmpty()) {
                QStringList rules;
                for(auto it=named.begin();it!=named.end();++it) {
                    const QJsonObject rule=it.value().toObject();
                    const QString unit=rule.value(QStringLiteral("unit")).toString();
                    rules<<QStringLiteral("%1: %2 – %3%4")
                        .arg(it.key(),displayRuleLimit(rule.value(QStringLiteral("low"))),
                            displayRuleLimit(rule.value(QStringLiteral("high"))),
                            unit.isEmpty()?QString():QStringLiteral(" ")+unit);
                }
                acceptance=rules.join(QStringLiteral("  ·  "));
            } else acceptance=acceptanceSummary(step);
        }
        if(step.value(QStringLiteral("mode")).toString()==QLatin1String("condition")) {
            const auto condition=step.value(QStringLiteral("condition")).toObject();
            acceptance=QStringLiteral("IF %1 %2 %3 → runtime choice")
                .arg(condition.value(QStringLiteral("source")).toString(),condition.value(QStringLiteral("operator")).toString(),
                     condition.value(QStringLiteral("value")).toVariant().toString());
        } else if(step.value(QStringLiteral("guards")).isArray()) {
            QStringList gates;
            for(const auto& v:step.value(QStringLiteral("guards")).toArray()) {
                const auto guard=v.toObject();gates<<QStringLiteral("%1: %2")
                    .arg(guard.value(QStringLiteral("id")).toString(),
                         guard.value(QStringLiteral("value")).toBool()?QStringLiteral("THEN"):QStringLiteral("ELSE"));
            }
            acceptance+=QStringLiteral("  ·  Conditional ")+gates.join(QStringLiteral(" / "));
        }
        const QString group=step.value(QStringLiteral("group")).toString(QStringLiteral("main"));
        const QString groupText=group==QLatin1String("setup")?QStringLiteral("Setup"):
            group==QLatin1String("cleanup")?QStringLiteral("Cleanup"):QStringLiteral("Main");
        const QStringList cells={QString::number(index+1),groupText,
            step.value(QStringLiteral("name")).toString(),procedureSummary(step),
            paramText.isEmpty()?QStringLiteral("—"):paramText,acceptance,
            sourceNames.value(templateId,templateId)};
        previewRows.push_back(cells);
    }
    const int total=steps.size(),shown=previewRows.size();
    if(shown>300) {
        static_cast<ExpandedPreviewModel*>(m_executionVirtualModel)->replace(std::move(previewRows));
        m_executionTable->hide();m_executionVirtualTable->show();
    } else {
        m_executionVirtualTable->hide();m_executionTable->show();
        m_executionTable->setRowCount(shown);
        for(int row=0;row<shown;++row) {
            const auto& cells=previewRows.at(row);
            for(int col=0;col<cells.size();++col) {
                auto* item=new QTableWidgetItem(cells.at(col));
                item->setToolTip(cells.at(col));
                if(col==0)item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
                m_executionTable->setItem(row,col,item);
            }
        }
    }
    const QString selection=(requestedSelection&&!selectedLeafIds.isEmpty())?
        QStringLiteral("Selected: %1  ·  ").arg(nodeAt(m_selectedPath).value(QStringLiteral("name")).toString()):
        QStringLiteral("Entire test  ·  ");
    m_executionSummary->setText(QStringLiteral("%1%2 of %3 executable steps shown  ·  %4 authored top-level rows. "
        "Setup, Main and Cleanup order is fixed. IF/ELSE branches are both shown; RUN chooses one based on recorded data.")
        .arg(selection).arg(shown).arg(total).arg(authored.value(QStringLiteral("steps")).toArray().size()));
}

void TestSequenceEditor::showPreview() {
    QJsonObject current;QString error;
    if(collectDocument(&current,&error))m_preview->setPlainText(jsonPretty(current));
    else m_preview->setPlainText(QStringLiteral("Invalid definition: ")+error);
}
void TestSequenceEditor::acceptChanges() {
    QJsonObject edited;QString error;
    if(!collectDocument(&edited,&error)){QMessageBox::warning(this,"Cannot save definition",error);return;}
    QJsonObject runFields=m_runInformation;
    if(m_runInfoChanged){
        if(!m_runEditor->applyFields(&error)){
            QMessageBox::warning(this,"Cannot save Run Information",error);return;
        }
        runFields=m_runEditor->schema();
        if(m_runInformation.isEmpty() && runFields.value(QStringLiteral("fields")).toArray().isEmpty())
            runFields=m_runInformation;
    }
    QJsonObject policy{{QStringLiteral("autoSave"),m_reportAutoSave->isChecked()},
                        {QStringLiteral("directory"),m_reportDirectory->text().trimmed()}};
    if(m_reportStorage.isEmpty() && !m_reportAutoSave->isChecked() && m_reportDirectory->text().trimmed().isEmpty())
        policy=m_reportStorage;
    if(!TestReportStorage::validate(policy,&error)) {
        QMessageBox::warning(this,"Cannot save report policy",error);return;
    }
    const QString library=m_sourceEditor->document()->isModified()
        ?m_sourceEditor->toPlainText():m_librarySource;
    if(m_saveHandler && !m_saveHandler(edited,runFields,policy,library))return;
    m_definition=edited;m_runInformation=runFields;m_reportStorage=policy;m_librarySource=library;
    accept();
}
void TestSequenceEditor::addStep() {
    if(!m_selectedPath.isEmpty() && containerNode(nodeAt(m_selectedPath))){addChild();return;}
    QString error;
    if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QSet<QString> existingIds;
    for(const auto& existing:m_steps)existingIds.insert(existing.value(QStringLiteral("id")).toString());
    QString generatedId=QStringLiteral("step_%1").arg(m_steps.size()+1);
    int suffix=1;while(existingIds.contains(generatedId))generatedId=QStringLiteral("step_%1").arg(m_steps.size()+suffix++);
    const QJsonObject node{{QStringLiteral("id"),generatedId},
        {QStringLiteral("name"),QStringLiteral("New action")},
        {QStringLiteral("group"),QStringLiteral("main")},
        {QStringLiteral("type"),QStringLiteral("action")},
        {QStringLiteral("mode"),QStringLiteral("delay")},{QStringLiteral("durationMs"),100}};
    if(m_selectedPath.size()>1){
        QJsonArray siblings=siblingArray(m_selectedPath);
        const int position=m_selectedPath.last()+1;
        QJsonObject child=node;child.remove(QStringLiteral("group"));
        siblings.insert(position,child);writeSiblings(m_selectedPath,siblings);
        QVector<int> target=m_selectedPath;target.last()=position;
        m_modified=true;populatePath(target);return;
    }
    int position=0;
    if(!m_selectedPath.isEmpty() && nodeAt(m_selectedPath).value(QStringLiteral("group")).toString()==QLatin1String("main"))
        position=m_selectedPath.first()+1;
    else while(position<m_steps.size()&&groupRank(m_steps.at(position).value(QStringLiteral("group")).toString(QStringLiteral("main")))<=1)++position;
    m_steps.insert(position,node);m_modified=true;populateTable(position);
}
void TestSequenceEditor::addContainer(bool repeat) {
    QString error;
    if(!applyStep(&error)){
        QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;
    }

    // Containers are authored at the current tree location, just like IF and
    // ordinary steps. Selecting IF/ELSE creates a child inside that branch;
    // selecting an existing leaf inserts a sibling before it so Indent can
    // make the selected step a child of the new Sweep/Repeat.
    QVector<int> parentPath;
    int insertAt=-1;
    if(!m_selectedPath.isEmpty()) {
        if(containerNode(nodeAt(m_selectedPath))) {
            parentPath=m_selectedPath;
            const QJsonObject parent=nodeAt(parentPath);
            const QJsonArray children=parent.value(QStringLiteral("steps")).toArray();
            insertAt=children.size();
            if(parent.contains(QStringLiteral("condition")) && insertAt>0 &&
               children.last().toObject().value(QStringLiteral("otherwise")).toBool(false))
                --insertAt; // THEN always precedes ELSE.
        } else if(m_selectedPath.size()>1) {
            parentPath=m_selectedPath;
            parentPath.removeLast();
            insertAt=m_selectedPath.last();
        }
    }

    // Use the native expander below for depth validation. In particular,
    // ELSE is a branch marker, not another executable nesting level, so
    // counting QTreeWidget ancestors would incorrectly reject valid nested
    // sweeps inside ELSE under an outer Sweep.

    QSet<QString> used;
    std::function<void(const QJsonObject&)> collectIds=[&](const QJsonObject& node){
        used.insert(node.value(QStringLiteral("id")).toString());
        for(const auto& v:node.value(QStringLiteral("steps")).toArray())
            collectIds(v.toObject());
    };
    for(const auto& node:m_steps)collectIds(node);
    auto freshId=[&used](const QString& stem){
        QString id=stem;int n=2;
        while(used.contains(id))id=stem+QStringLiteral("_%1").arg(n++);
        used.insert(id);return id;
    };
    const QString groupId=freshId(repeat?QStringLiteral("repeat_group"):QStringLiteral("sweep_group"));
    const QString childId=freshId(groupId+QStringLiteral("_measurement"));

    // Use short, predictable defaults. Inserting a 13-point sweep inside a
    // repeated IF could otherwise exceed the engine's bounded expanded-step capacity
    // before the user even has an opportunity to configure its range.
    QString parameter=QStringLiteral("value");
    QSet<QString> inherited;
    for(int n=1;n<=parentPath.size();++n){
        const QJsonObject ancestor=nodeAt(parentPath.mid(0,n));
        const QJsonObject loop=ancestor.value(QStringLiteral("loop")).toObject();
        if(!loop.isEmpty()) {
            QJsonArray axes=loop.value(QStringLiteral("axes")).toArray();
            if(axes.isEmpty())axes.append(loop);
            for(const auto& axis:axes)
                inherited.insert(axis.toObject().value(QStringLiteral("parameter")).toString());
        }
        if(ancestor.contains(QStringLiteral("repeat")))
            inherited.insert(ancestor.value(QStringLiteral("repeat")).toObject()
                .value(QStringLiteral("index")).toString(QStringLiteral("iteration")));
    }
    int suffix=2;
    while(inherited.contains(parameter))parameter=QStringLiteral("value%1").arg(suffix++);
    QString index=QStringLiteral("iteration");suffix=2;
    while(inherited.contains(index))index=QStringLiteral("iteration%1").arg(suffix++);
    const QString measurementParameter=repeat?index:parameter;
    const QJsonObject child{{QStringLiteral("id"),childId},
        {QStringLiteral("name"),repeat?
            QStringLiteral("Measurement pass ${%1}").arg(index):
            QStringLiteral("Measure ${%1}").arg(parameter)},
        {QStringLiteral("type"),QStringLiteral("numeric")},
        {QStringLiteral("mode"),QStringLiteral("callback")},
        {QStringLiteral("handler"),QStringLiteral("measureValue")},
        {QStringLiteral("timeoutMs"),10000},
        {QStringLiteral("limits"),QJsonObject{
            {QStringLiteral("low"),QJsonObject{{QStringLiteral("param"),measurementParameter},{QStringLiteral("offset"),-0.1}}},
            {QStringLiteral("high"),QJsonObject{{QStringLiteral("param"),measurementParameter},{QStringLiteral("offset"),0.1}}}}}};
    QJsonObject group{{QStringLiteral("id"),groupId},
        {QStringLiteral("name"),repeat?QStringLiteral("Repeat measurement"):QStringLiteral("New sweep")},
        {QStringLiteral("steps"),QJsonArray{child}}};
    if(repeat)group.insert(QStringLiteral("repeat"),QJsonObject{{QStringLiteral("count"),3},{QStringLiteral("index"),index}});
    else group.insert(QStringLiteral("loop"),QJsonObject{{QStringLiteral("parameter"),parameter},
        {QStringLiteral("start"),0},{QStringLiteral("stop"),1},{QStringLiteral("increment"),1}});

    QVector<QJsonObject> candidate=m_steps;
    QVector<int> targetPath;
    if(!parentPath.isEmpty()){
        QJsonArray children=nodeAt(parentPath).value(QStringLiteral("steps")).toArray();
        children.insert(insertAt,group);
        QJsonObject parent=nodeAt(parentPath);parent.insert(QStringLiteral("steps"),children);
        if(!writeNode(parentPath,parent))return;
        targetPath=parentPath;targetPath.append(insertAt);
    } else {
        int position=0;
        if(m_selectedPath.size()==1 &&
           nodeAt(m_selectedPath).value(QStringLiteral("group")).toString()==QLatin1String("main"))
            position=m_selectedPath.first();
        else {
            while(position<m_steps.size() &&
                  groupRank(m_steps.at(position).value(QStringLiteral("group")).toString(QStringLiteral("main")))<=1)
                ++position;
        }
        group.insert(QStringLiteral("group"),QStringLiteral("main"));
        m_steps.insert(position,group);
        targetPath={position};
    }
    // Keep the previous tree if the new group breaks the established limits
    // (nesting, duplicate parameters, or the expanded-step capacity). No incomplete
    // edit is committed to the Panel.
    QJsonObject draft=m_definition;QJsonArray authored;
    for(const auto& item:m_steps)authored.append(item);
    draft.insert(QStringLiteral("steps"),authored);
    QJsonObject expanded;
    if(!TestSequenceLoops::expand(draft,&expanded,&error)) {
        m_steps=candidate;
        QMessageBox::warning(this,QStringLiteral("Cannot insert Sweep / Repeat"),
            QStringLiteral("The selected location exceeds a native execution limit:\n%1\n"
                           "Choose a less deeply nested location or simplify the enclosing sweep.").arg(error));
        return;
    }
    m_modified=true;populatePath(targetPath);
}
bool TestSequenceEditor::collectVariableDeclarations(QJsonArray* result,QString* error) const {
    if(!result)return errorOut(error,QStringLiteral("No variable output"));
    QJsonArray entries;
    for(int r=0;r<m_variablesTable->rowCount();++r) {
        auto cell=[this,r](int col)->QString {
            const auto* item=m_variablesTable->item(r,col);
            return item?item->text().trimmed():QString();
        };
        const QString name=cell(0);
        const auto* kind=qobject_cast<QComboBox*>(m_variablesTable->cellWidget(r,1));
        const QString type=kind?kind->currentText():QStringLiteral("number");
        QJsonObject entry{{QStringLiteral("name"),name},{QStringLiteral("type"),type}};
        if(!cell(3).isEmpty())entry.insert(QStringLiteral("description"),cell(3));
        const QString initial=cell(2);
        if(!initial.isEmpty()) {
            if(type==QLatin1String("number")) {
                bool ok=false;const double value=initial.toDouble(&ok);
                if(!ok||!std::isfinite(value))return errorOut(error,QStringLiteral("Initial value for %1 must be numeric").arg(name));
                entry.insert(QStringLiteral("initial"),value);
            } else if(type==QLatin1String("boolean")) {
                if(initial!=QLatin1String("true") && initial!=QLatin1String("false"))
                    return errorOut(error,QStringLiteral("Boolean initial value for %1 must be true/false").arg(name));
                entry.insert(QStringLiteral("initial"),initial==QLatin1String("true"));
            } else entry.insert(QStringLiteral("initial"),initial);
        }
        entries.append(entry);
    }
    if(!TestSequenceConditions::validateVariables(entries,error))return false;
    *result=entries;return true;
}

void TestSequenceEditor::refreshConditionPicker() {
    if(!m_conditionSource || !m_conditionKey || !m_conditionStep)return;
    const QString oldKey=m_conditionKey->currentText(),oldStep=m_conditionStep->currentText();
    const int source=m_conditionSource->currentIndex();
    const QSignalBlocker keyBlock(m_conditionKey),stepBlock(m_conditionStep);
    m_conditionKey->clear();m_conditionStep->clear();
    QStringList keys,steps;
    if(source==0) {
        for(int row=0;row<m_variablesTable->rowCount();++row)
            if(m_variablesTable->item(row,0))keys<<m_variablesTable->item(row,0)->text();
    } else if(source==1) {
        for(int n=1;n<m_selectedPath.size();++n) {
            QVector<int> parent=m_selectedPath.mid(0,n);
            const QJsonObject ancestor=nodeAt(parent);
            const QJsonObject loop=ancestor.value(QStringLiteral("loop")).toObject();
            if(!loop.isEmpty()) {
                QJsonArray axes=loop.value(QStringLiteral("axes")).toArray();
                if(axes.isEmpty())axes.append(loop);
                for(const auto& axis:axes)keys<<axis.toObject().value(QStringLiteral("parameter")).toString();
            }
            if(ancestor.contains(QStringLiteral("repeat")))keys<<ancestor.value(QStringLiteral("repeat")).toObject().value(QStringLiteral("index")).toString(QStringLiteral("iteration"));
        }
    } else if(source==4) {
        for(const auto& v:m_runInformation.value(QStringLiteral("fields")).toArray())
            keys<<v.toObject().value(QStringLiteral("id")).toString();
    } else if(source==5) {
        keys={QStringLiteral("passedMeasurements"),QStringLiteral("failedMeasurements"),
              QStringLiteral("passedSteps"),QStringLiteral("failedSteps")};
    }
    if(source==2 || source==3) {
        std::function<void(const QJsonObject&)> visit=[&](const QJsonObject& node) {
            if(containerNode(node)) {
                for(const auto& child:node.value(QStringLiteral("steps")).toArray())visit(child.toObject());
            } else steps<<node.value(QStringLiteral("id")).toString();
        };
        for(const auto& node:m_steps)visit(node);
        steps.removeDuplicates();m_conditionStep->addItems(steps);
    }
    keys.removeAll(QString());keys.removeDuplicates();m_conditionKey->addItems(keys);
    if(!oldKey.isEmpty())m_conditionKey->setCurrentText(oldKey);
    if(!oldStep.isEmpty())m_conditionStep->setCurrentText(oldStep);
    m_conditionKey->setEnabled(source!=2 && source!=3);
    m_conditionStep->setEnabled(source==2 || source==3);
    m_conditionPath->setEnabled(source==2);
}

void TestSequenceEditor::addConditional() {
    QString error;
    if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    // A safe starter IF reads a declared test variable. No hardware is run.
    const QJsonObject child{{QStringLiteral("id"),QStringLiteral("then_action")},
        {QStringLiteral("name"),QStringLiteral("THEN — action")},
        {QStringLiteral("type"),QStringLiteral("action")},{QStringLiteral("mode"),QStringLiteral("delay")},
        {QStringLiteral("durationMs"),10}};
    QString key=QStringLiteral("failedMeasurements");
    QString source=QStringLiteral("counter");
    if(m_variablesTable->rowCount()>0 && m_variablesTable->item(0,0)) {
        key=m_variablesTable->item(0,0)->text().trimmed();
        source=QStringLiteral("variable");
    }
    QJsonObject node{{QStringLiteral("id"),QStringLiteral("if_condition")},
        {QStringLiteral("name"),QStringLiteral("IF check ")+key},
        {QStringLiteral("group"),QStringLiteral("main")},
        {QStringLiteral("condition"),QJsonObject{{QStringLiteral("source"),source},
            {QStringLiteral("key"),key},{QStringLiteral("operator"),QStringLiteral(">")},
            {QStringLiteral("value"),50.0}}},
        {QStringLiteral("steps"),QJsonArray{child}}};
    QSet<QString> names;
    std::function<void(const QJsonObject&)> collect=[&](const QJsonObject& n){
        names.insert(n.value(QStringLiteral("id")).toString());
        for(const auto& item:n.value(QStringLiteral("steps")).toArray())collect(item.toObject());
    };
    for(const auto& n:m_steps)collect(n);
    QString id=QStringLiteral("if_condition");int counter=2;
    while(names.contains(id))id=QStringLiteral("if_condition_%1").arg(counter++);
    node.insert(QStringLiteral("id"),id);
    QJsonObject uniqueChild=child;uniqueChild.insert(QStringLiteral("id"),id+QStringLiteral("_then"));
    node.insert(QStringLiteral("steps"),QJsonArray{uniqueChild});
    if(m_selectedPath.size()>1) {
        QJsonArray siblings=siblingArray(m_selectedPath);
        node.remove(QStringLiteral("group"));
        const int place=m_selectedPath.last()+1;siblings.insert(place,node);
        writeSiblings(m_selectedPath,siblings);
        QVector<int> path=m_selectedPath;path.last()=place;m_modified=true;populatePath(path);
    } else {
        int place=m_steps.size();
        if(m_selectedPath.size()==1 && nodeAt(m_selectedPath).value(QStringLiteral("group")).toString()==QLatin1String("main"))place=m_selectedPath.first()+1;
        else {place=0;while(place<m_steps.size() &&
            groupRank(m_steps.at(place).value(QStringLiteral("group")).toString(QStringLiteral("main")))<=1)++place;}
        m_steps.insert(place,node);m_modified=true;populatePath(QVector<int>{place});
    }
}

void TestSequenceEditor::addElseBranch() {
    QString error;
    if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QVector<int> parent=m_selectedPath;
    while(!parent.isEmpty() && !nodeAt(parent).contains(QStringLiteral("condition")))parent.removeLast();
    if(parent.isEmpty()) {
        QMessageBox::information(this,QStringLiteral("Select IF"),
            QStringLiteral("Select an IF row or one of its children before adding ELSE."));return;
    }
    QJsonObject parentNode=nodeAt(parent);
    QJsonArray children=parentNode.value(QStringLiteral("steps")).toArray();
    for(const auto& child:children)if(child.toObject().value(QStringLiteral("otherwise")).toBool(false)) {
        QMessageBox::information(this,QStringLiteral("ELSE exists"),QStringLiteral("This IF already has an ELSE branch."));return;
    }
    const QString id=parentNode.value(QStringLiteral("id")).toString()+QStringLiteral("_else");
    const QJsonObject action{{QStringLiteral("id"),id+QStringLiteral("_action")},
        {QStringLiteral("name"),QStringLiteral("ELSE — action")},
        {QStringLiteral("type"),QStringLiteral("action")},{QStringLiteral("mode"),QStringLiteral("delay")},
        {QStringLiteral("durationMs"),10}};
    children.append(QJsonObject{{QStringLiteral("id"),id},{QStringLiteral("name"),QStringLiteral("ELSE")},
        {QStringLiteral("otherwise"),true},{QStringLiteral("steps"),QJsonArray{action}}});
    parentNode.insert(QStringLiteral("steps"),children);
    writeNode(parent,parentNode);m_modified=true;parent.append(children.size()-1);populatePath(parent);
}

void TestSequenceEditor::addChild() {
    QString error;if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QVector<int> parentPath=m_selectedPath;
    if(!parentPath.isEmpty()&&!containerNode(nodeAt(parentPath)))parentPath.removeLast();
    if(parentPath.isEmpty()||!containerNode(nodeAt(parentPath))){
        QMessageBox::information(this,QStringLiteral("Select a parent row"),
            QStringLiteral("Select a Sweep, Repeat, IF, or ELSE row (or one of its child steps), then choose + Child Step."));return;
    }
    QJsonObject parent=nodeAt(parentPath);QJsonArray children=parent.value(QStringLiteral("steps")).toArray();
    int position=children.size();
    if(parent.contains(QStringLiteral("condition")) && position>0 &&
       children.last().toObject().value(QStringLiteral("otherwise")).toBool(false))--position;
    const QJsonObject child{{QStringLiteral("id"),QStringLiteral("child_%1").arg(position+1)},
        {QStringLiteral("name"),QStringLiteral("New action")},
        {QStringLiteral("type"),QStringLiteral("action")},{QStringLiteral("mode"),QStringLiteral("delay")},
        {QStringLiteral("durationMs"),100}};
    children.insert(position,child);parent.insert(QStringLiteral("steps"),children);
    writeNode(parentPath,parent);m_modified=true;
    parentPath.append(position);populatePath(parentPath);
}
void TestSequenceEditor::removeStep() {
    if(m_selectedPath.isEmpty())return;
    QString error;if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QJsonArray siblings=siblingArray(m_selectedPath);
    QVector<int> parentPath=m_selectedPath;parentPath.removeLast();
    const QJsonObject parent=parentPath.isEmpty()?QJsonObject{}:nodeAt(parentPath);
    const bool lastThen=parent.contains(QStringLiteral("condition")) && siblings.size()==2 &&
        siblings.last().toObject().value(QStringLiteral("otherwise")).toBool(false) &&
        m_selectedPath.last()==0;
    if((m_selectedPath.size()>1 && siblings.size()==1) ||
       (m_selectedPath.size()==1 && m_steps.size()==1) || lastThen){
        QMessageBox::information(this,QStringLiteral("Cannot remove"),
            QStringLiteral("A test and each group must keep a child step. "
                           "An IF must retain at least one THEN step before ELSE."));return;
    }
    const int position=m_selectedPath.last();siblings.removeAt(position);
    writeSiblings(m_selectedPath,siblings);m_modified=true;
    QVector<int> target=m_selectedPath;target.last()=qMin(position,siblings.size()-1);
    populatePath(target);
}
void TestSequenceEditor::moveStep(int direction) {
    if(m_selectedPath.isEmpty())return;
    QString error;if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QJsonArray siblings=siblingArray(m_selectedPath);
    const int from=m_selectedPath.last(),to=from+direction;
    if(to<0||to>=siblings.size())return;
    const QJsonObject current=siblings.at(from).toObject();
    const QJsonObject neighbor=siblings.at(to).toObject();
    // ELSE is always the final branch. Never allow moving an ELSE ahead of
    // THEN children or a THEN child behind ELSE.
    if(current.value(QStringLiteral("otherwise")).toBool(false) ||
       neighbor.value(QStringLiteral("otherwise")).toBool(false))return;
    if(m_selectedPath.size()==1 && current.value(QStringLiteral("group")).toString(QStringLiteral("main"))!=
       neighbor.value(QStringLiteral("group")).toString(QStringLiteral("main")))return;
    siblings.replace(from,neighbor);siblings.replace(to,current);
    writeSiblings(m_selectedPath,siblings);m_modified=true;
    QVector<int> target=m_selectedPath;target.last()=to;populatePath(target);
}
void TestSequenceEditor::indentStep() {
    if(m_selectedPath.isEmpty())return;
    QString error;if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QJsonArray siblings=siblingArray(m_selectedPath);
    const int position=m_selectedPath.last();
    if(position<1||!containerNode(siblings.at(position-1).toObject())){
        QMessageBox::information(this,QStringLiteral("Indent step"),
            QStringLiteral("Place a Sweep, Repeat, or IF immediately above this step, then select Indent."));return;
    }
    const QJsonObject old=siblings.at(position).toObject();
    if(old.value(QStringLiteral("otherwise")).toBool(false)){
        QMessageBox::information(this,QStringLiteral("ELSE must stay with IF"),
            QStringLiteral("An ELSE row cannot be moved into another group. "
                           "Move individual steps inside THEN or ELSE instead."));return;
    }
    if(containerNode(old)&&m_selectedPath.size()>=3){
        QMessageBox::information(this,QStringLiteral("Loop depth"),QStringLiteral("Nested loops are limited to three levels."));return;
    }
    QJsonObject parent=siblings.at(position-1).toObject();
    QJsonArray children=parent.value(QStringLiteral("steps")).toArray();
    QJsonObject child=old;child.remove(QStringLiteral("group"));
    // IF owns a terminal ELSE branch. A step indented into THEN must be
    // inserted before ELSE; appending it after ELSE invalidates the sequence.
    int childIndex=children.size();
    if(parent.contains(QStringLiteral("condition")) && childIndex>0 &&
       children.last().toObject().value(QStringLiteral("otherwise")).toBool(false))
        --childIndex;
    children.insert(childIndex,child);
    parent.insert(QStringLiteral("steps"),children);
    siblings.replace(position-1,parent);siblings.removeAt(position);
    writeSiblings(m_selectedPath,siblings);m_modified=true;
    QVector<int> target=m_selectedPath;target.last()=position-1;target.append(childIndex);
    populatePath(target);
}
void TestSequenceEditor::outdentStep() {
    if(m_selectedPath.size()<2)return;
    QString error;if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QJsonArray siblings=siblingArray(m_selectedPath);
    QVector<int> containerPath=m_selectedPath;containerPath.removeLast();
    const QJsonObject container=nodeAt(containerPath);
    if(siblings.size()<2 ||
       (container.contains(QStringLiteral("condition")) && siblings.size()==2 &&
        siblings.last().toObject().value(QStringLiteral("otherwise")).toBool(false) &&
        m_selectedPath.last()==0)){
        QMessageBox::information(this,QStringLiteral("Cannot outdent"),
            QStringLiteral("A loop must retain a child, and an IF must retain a THEN step. "
                           "Add another THEN step before moving this one."));return;
    }
    QJsonObject child=siblings.at(m_selectedPath.last()).toObject();
    if(child.value(QStringLiteral("otherwise")).toBool(false)){
        QMessageBox::information(this,QStringLiteral("ELSE must stay with IF"),
            QStringLiteral("An ELSE branch cannot be outdented. "
                           "Move its child steps into THEN or another group instead."));return;
    }
    siblings.removeAt(m_selectedPath.last());writeSiblings(m_selectedPath,siblings);
    QVector<int> parentPath=m_selectedPath;parentPath.removeLast();
    QJsonArray outside=siblingArray(parentPath);
    // Outdenting from ELSE moves the step into THEN, immediately before
    // ELSE, rather than leaving an invalid step after the terminal branch.
    const int insertAt=container.value(QStringLiteral("otherwise")).toBool(false)
        ? parentPath.last() : parentPath.last()+1;
    if(parentPath.size()==1)child.insert(QStringLiteral("group"),QStringLiteral("main"));
    outside.insert(insertAt,child);writeSiblings(parentPath,outside);
    m_modified=true;parentPath.last()=insertAt;populatePath(parentPath);
}
void TestSequenceEditor::duplicateStep() {
    if(m_selectedPath.isEmpty())return;
    QString error;if(!applyStep(&error)){QMessageBox::warning(this,QStringLiteral("Invalid step"),error);return;}
    QJsonArray siblings=siblingArray(m_selectedPath);
    QJsonObject copy=siblings.at(m_selectedPath.last()).toObject();
    const QString base=copy.value(QStringLiteral("id")).toString();
    int suffix=1;QString id;
    QSet<QString> used;
    for(const auto& v:siblings)used.insert(v.toObject().value(QStringLiteral("id")).toString());
    do{id=base.left(52)+QStringLiteral("_copy%1").arg(suffix++);}while(used.contains(id));
    copy.insert(QStringLiteral("id"),id);
    copy.insert(QStringLiteral("name"),copy.value(QStringLiteral("name")).toString()+QStringLiteral(" (copy)"));
    const int target=m_selectedPath.last()+1;siblings.insert(target,copy);
    writeSiblings(m_selectedPath,siblings);m_modified=true;
    QVector<int> path=m_selectedPath;path.last()=target;populatePath(path);
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
    m_definition=doc.object();loadDocument();m_modified=true;
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
