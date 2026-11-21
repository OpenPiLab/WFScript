#pragma once

// RC1.5.11: purely declarative expansion of bounded loops/sweeps.
// No JS evaluation, no instrument access and no runtime recursion. The existing
// TestRunner still receives a flat, fully validated v1 sequence before RUN.
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include "TestSequenceConditions.h"
#include "TestSequenceCapacity.h"
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <QPair>
#include <functional>
#include <algorithm>
#include <QStringList>
#include <cmath>

namespace TestSequenceLoops {
namespace detail {
inline bool reject(QString* error,const QString& text) {if(error)*error=text;return false;}
inline bool ident(const QString& s) {return QRegularExpression(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,39}$")).match(s).hasMatch();}
inline bool finite(double d) {return std::isfinite(d);}
inline QString valueText(const QJsonValue& v) {
    if(v.isString())return v.toString();
    if(v.isDouble())return QString::number(v.toDouble(),'g',12);
    if(v.isBool())return v.toBool()?QStringLiteral("true"):QStringLiteral("false");
    return QStringLiteral("null");
}
inline bool axis(const QJsonObject& axis,QVector<QJsonValue>* values,QString* error) {
    values->clear();
    for(auto it=axis.begin();it!=axis.end();++it)
        if(!QSet<QString>{QStringLiteral("parameter"),QStringLiteral("start"),
                          QStringLiteral("stop"),QStringLiteral("increment"),
                          QStringLiteral("values")}.contains(it.key()))
            return reject(error,QStringLiteral("Unknown sweep axis field '%1'").arg(it.key()));
    if(axis.contains(QStringLiteral("values")) &&
       (axis.contains(QStringLiteral("start")) || axis.contains(QStringLiteral("stop")) ||
        axis.contains(QStringLiteral("increment"))))
        return reject(error,QStringLiteral("Use values OR start/stop/increment for an axis"));
    if(axis.contains(QStringLiteral("values"))) {
        if(!axis.value(QStringLiteral("values")).isArray())return reject(error,QStringLiteral("Sweep values must be an array"));
        const QJsonArray input=axis.value(QStringLiteral("values")).toArray();
        if(input.isEmpty()||input.size()>TestSequenceCapacity::maxSweepAxisPoints)return reject(error,QStringLiteral("Sweep values require 1..%1 entries").arg(TestSequenceCapacity::maxSweepAxisPoints));
        for(const QJsonValue& v:input) {
            if(!(v.isString()||v.isBool()||(v.isDouble()&&finite(v.toDouble()))))
                return reject(error,QStringLiteral("Sweep values must be finite numbers, strings or booleans"));
            values->append(v);
        }
        return true;
    }
    for(const QString& k:{QStringLiteral("start"),QStringLiteral("stop"),QStringLiteral("increment")})
        if(!axis.value(k).isDouble() || !finite(axis.value(k).toDouble()))
            return reject(error,QStringLiteral("Sweep range needs finite start, stop, increment"));
    const double start=axis.value(QStringLiteral("start")).toDouble();
    const double stop=axis.value(QStringLiteral("stop")).toDouble();
    const double inc=axis.value(QStringLiteral("increment")).toDouble();
    if(inc==0.0 || (stop-start)*inc<0)
        return reject(error,QStringLiteral("Sweep increment must move toward stop"));
    const double n=(stop-start)/inc;
    if(n<0 || n>double(TestSequenceCapacity::maxSweepAxisPoints-1)+1e-8)return reject(error,QStringLiteral("Sweep has more than %1 setpoints").arg(TestSequenceCapacity::maxSweepAxisPoints));
    const int count=static_cast<int>(std::floor(n+1e-8))+1;
    if(count<1||count>TestSequenceCapacity::maxSweepAxisPoints || std::fabs((start+(count-1)*inc)-stop)>1e-7*std::max(1.0,std::fabs(stop)))
        return reject(error,QStringLiteral("Sweep stop must be reached exactly by its increment"));
    for(int i=0;i<count;++i)values->append(start+i*inc);
    return true;
}
inline bool evaluateBound(QJsonValue& v,const QJsonObject& params,QString* error) {
    if(v.isDouble())return finite(v.toDouble())?true:reject(error,QStringLiteral("Non-finite limit"));
    if(!v.isObject())return reject(error,QStringLiteral("Limit must be numeric or {param,factor,offset}"));
    const QJsonObject expr=v.toObject();
    if(expr.size()>3 || !expr.value(QStringLiteral("param")).isString())
        return reject(error,QStringLiteral("Limit expression must be {param,factor,offset}"));
    const QString key=expr.value(QStringLiteral("param")).toString();
    if(!params.value(key).isDouble()||!finite(params.value(key).toDouble()))
        return reject(error,QStringLiteral("Limit references missing/non-numeric parameter '%1'").arg(key));
    const QJsonValue offset=expr.value(QStringLiteral("offset"));
    if(!offset.isUndefined()&&(!offset.isDouble()||!finite(offset.toDouble())))
        return reject(error,QStringLiteral("Limit offset must be a finite number"));
    const QJsonValue factor=expr.value(QStringLiteral("factor"));
    if(!factor.isUndefined()&&(!factor.isDouble()||!finite(factor.toDouble())))
        return reject(error,QStringLiteral("Limit factor must be a finite number"));
    const double bound=params.value(key).toDouble()*(factor.isUndefined()?1.0:factor.toDouble())+
                       (offset.isUndefined()?0.0:offset.toDouble());
    if(!finite(bound))return reject(error,QStringLiteral("Computed limit overflow"));
    v=bound;
    return true;
}
inline bool resolveLimits(QJsonObject* step,const QJsonObject& params,QString* error) {
    if(step->contains(QStringLiteral("limits"))) {
        QJsonObject obj=step->value(QStringLiteral("limits")).toObject();
        for(const QString& k:{QStringLiteral("low"),QStringLiteral("high")}) {
            QJsonValue v=obj.value(k);if(!evaluateBound(v,params,error))return false;obj.insert(k,v);
        }
        step->insert(QStringLiteral("limits"),obj);
    }
    if(step->contains(QStringLiteral("measurements"))) {
        QJsonObject rules=step->value(QStringLiteral("measurements")).toObject();
        for(const QString& path:rules.keys()) {
            QJsonObject rule=rules.value(path).toObject();
            // Named result rules always carry low/high; 'each' wildcard is a path, not a nested rule.
            for(const QString& k:{QStringLiteral("low"),QStringLiteral("high")}) {
                QJsonValue v=rule.value(k);if(!evaluateBound(v,params,error))
                    return reject(error,QStringLiteral("Measurement %1: %2").arg(path,error?*error:QString()));
                rule.insert(k,v);
            }
            QString label=rule.value(QStringLiteral("name")).toString();
            if(!label.isEmpty()) {
                for(auto it=params.begin();it!=params.end();++it)
                    label.replace(QStringLiteral("${%1}").arg(it.key()),valueText(it.value()));
                rule.insert(QStringLiteral("name"),label.left(120));
            }
            rules.insert(path,rule);
        }
        step->insert(QStringLiteral("measurements"),rules);
    }
    return true;
}
struct Context {
    QJsonArray output;
    QSet<QString> ids;
    QString* error;
    int counter=0;
    QJsonArray variables;
};
inline bool visit(const QJsonObject& node,const QJsonObject& inherited,
                  const QString& group,int depth,bool repeated,Context* ctx,
                  const QJsonArray& guards=QJsonArray{}) {
    if(depth>3)return reject(ctx->error,QStringLiteral("Maximum loop nesting depth is 3"));
    const bool conditional=node.contains(QStringLiteral("condition"));
    if(node.contains(QStringLiteral("otherwise")))
        return reject(ctx->error,QStringLiteral("ELSE must be a child of IF"));
    if(conditional) {
        if(group!=QLatin1String("main"))return reject(ctx->error,QStringLiteral("IF is permitted in Main only"));
        if(!node.value(QStringLiteral("steps")).isArray() || node.value(QStringLiteral("steps")).toArray().isEmpty())
            return reject(ctx->error,QStringLiteral("IF requires at least one THEN child"));
        const QJsonObject condition=node.value(QStringLiteral("condition")).toObject();
        if(!TestSequenceConditions::validate(condition,ctx->variables,ctx->error))return false;
        for(auto it=node.begin();it!=node.end();++it)
            if(!QSet<QString>{QStringLiteral("id"),QStringLiteral("name"),QStringLiteral("description"),
                  QStringLiteral("group"),QStringLiteral("condition"),QStringLiteral("steps")}.contains(it.key()))
                return reject(ctx->error,QStringLiteral("Unsupported IF property '%1'").arg(it.key()));
        const QString original=node.value(QStringLiteral("id")).toString();
        if(original.isEmpty()||node.value(QStringLiteral("name")).toString().isEmpty())
            return reject(ctx->error,QStringLiteral("IF requires an ID and name"));
        QJsonObject marker{{QStringLiteral("id"),original},
            {QStringLiteral("name"),node.value(QStringLiteral("name"))},
            {QStringLiteral("group"),group},{QStringLiteral("type"),QStringLiteral("action")},
            {QStringLiteral("mode"),QStringLiteral("condition")},{QStringLiteral("condition"),condition}};
        if(node.contains(QStringLiteral("description")))marker.insert(QStringLiteral("description"),node.value(QStringLiteral("description")));
        if(!inherited.isEmpty())marker.insert(QStringLiteral("parameters"),inherited);
        if(!guards.isEmpty())marker.insert(QStringLiteral("guards"),guards);
        if(repeated) {
            marker.insert(QStringLiteral("templateId"),original);
            marker.insert(QStringLiteral("id"),original.left(48)+QStringLiteral("__%1").arg(++ctx->counter,4,10,QLatin1Char('0')));
        }
        QString displayName=marker.value(QStringLiteral("name")).toString();
        for(auto it=inherited.begin();it!=inherited.end();++it)
            displayName.replace(QStringLiteral("${%1}").arg(it.key()),valueText(it.value()));
        marker.insert(QStringLiteral("name"),displayName.left(120));
        const QString markerId=marker.value(QStringLiteral("id")).toString();
        if(ctx->ids.contains(markerId))return reject(ctx->error,QStringLiteral("Duplicate conditional ID: %1").arg(markerId));
        ctx->ids.insert(markerId);ctx->output.append(marker);
        if(ctx->output.size()>TestSequenceCapacity::maxExpandedSteps)return reject(ctx->error,
            QStringLiteral("Expanded sequence exceeds %1 executable steps").arg(TestSequenceCapacity::maxExpandedSteps));
        QJsonArray thenGuards=guards,elseGuards=guards;
        thenGuards.append(QJsonObject{{QStringLiteral("id"),markerId},{QStringLiteral("value"),true}});
        elseGuards.append(QJsonObject{{QStringLiteral("id"),markerId},{QStringLiteral("value"),false}});
        bool hasElse=false;
        for(const auto& childValue:node.value(QStringLiteral("steps")).toArray()) {
            if(!childValue.isObject())return reject(ctx->error,QStringLiteral("IF child must be an object"));
            const QJsonObject child=childValue.toObject();
            if(child.value(QStringLiteral("otherwise")).toBool(false)) {
                if(hasElse)return reject(ctx->error,QStringLiteral("IF can contain only one ELSE"));
                hasElse=true;
                if(!child.value(QStringLiteral("steps")).isArray() || child.value(QStringLiteral("steps")).toArray().isEmpty())
                    return reject(ctx->error,QStringLiteral("ELSE must have at least one child"));
                for(const auto& elseValue:child.value(QStringLiteral("steps")).toArray()) {
                    if(!elseValue.isObject()||!visit(elseValue.toObject(),inherited,group,depth+1,repeated,ctx,elseGuards))return false;
                }
            } else {
                if(hasElse)return reject(ctx->error,QStringLiteral("THEN children must precede ELSE"));
                if(!visit(child,inherited,group,depth+1,repeated,ctx,thenGuards))return false;
            }
        }
        return true;
    }
    const bool hasChildren=node.contains(QStringLiteral("steps"));
    const bool hasLoop=node.contains(QStringLiteral("loop"));
    const bool hasRepeat=node.contains(QStringLiteral("repeat"));
    if(hasChildren && !(hasLoop||hasRepeat))
        return reject(ctx->error,QStringLiteral("Loop group needs loop or repeat configuration"));
    if(hasChildren && (node.value(QStringLiteral("id")).toString().isEmpty() ||
       node.value(QStringLiteral("name")).toString().isEmpty()))
        return reject(ctx->error,QStringLiteral("Loop group needs a name and ID"));
    if((hasLoop||hasRepeat) && group!=QStringLiteral("main"))
        return reject(ctx->error,QStringLiteral("Loops and sweeps are permitted in Main only (Cleanup always runs once)"));
    QVector<QPair<QString,QVector<QJsonValue>>> axes;
    if(hasLoop) {
        if(!node.value(QStringLiteral("loop")).isObject())return reject(ctx->error,QStringLiteral("loop must be an object"));
        const QJsonObject loop=node.value(QStringLiteral("loop")).toObject();
        if(loop.contains(QStringLiteral("axes")) && loop.size()!=1)
            return reject(ctx->error,QStringLiteral("loop.axes cannot be mixed with range fields"));
        QJsonArray specs;
        if(loop.contains(QStringLiteral("axes"))) {
            if(!loop.value(QStringLiteral("axes")).isArray())return reject(ctx->error,QStringLiteral("loop.axes must be an array"));
            specs=loop.value(QStringLiteral("axes")).toArray();
        } else specs.append(loop);
        if(specs.isEmpty()||specs.size()>3)return reject(ctx->error,QStringLiteral("A sweep supports 1..3 axes"));
        for(const auto& entry:specs) {
            if(!entry.isObject())return reject(ctx->error,QStringLiteral("Sweep axis must be an object"));
            const auto a=entry.toObject();
            const QString parameter=a.value(QStringLiteral("parameter")).toString();
            if(!ident(parameter)||inherited.contains(parameter))
                return reject(ctx->error,QStringLiteral("Invalid or duplicate axis parameter '%1'").arg(parameter));
            for(const auto& previous:axes)if(previous.first==parameter)
                return reject(ctx->error,QStringLiteral("Duplicate sweep parameter '%1'").arg(parameter));
            QVector<QJsonValue> values;
            if(!axis(a,&values,ctx->error))return false;
            axes.append(QPair<QString,QVector<QJsonValue>>(parameter,values));
        }
    }
    int repeatCount=1;
    QString repeatIndex=QStringLiteral("iteration");
    if(hasRepeat) {
        if(!node.value(QStringLiteral("repeat")).isObject())return reject(ctx->error,QStringLiteral("repeat must be an object"));
        const QJsonObject repeat=node.value(QStringLiteral("repeat")).toObject();
        for(auto it=repeat.begin();it!=repeat.end();++it)
            if(it.key()!=QStringLiteral("count") && it.key()!=QStringLiteral("index"))
                return reject(ctx->error,QStringLiteral("Unknown repeat property '%1'").arg(it.key()));
        const QJsonValue count=repeat.value(QStringLiteral("count"));
        if(!count.isDouble()||count.toDouble()<1||count.toDouble()>TestSequenceCapacity::maxRepeatCount||std::floor(count.toDouble())!=count.toDouble())
            return reject(ctx->error,QStringLiteral("Repeat count must be an integer 1..%1").arg(TestSequenceCapacity::maxRepeatCount));
        repeatCount=static_cast<int>(count.toDouble());
        repeatIndex=repeat.value(QStringLiteral("index")).toString(QStringLiteral("iteration"));
        if(!ident(repeatIndex)||inherited.contains(repeatIndex))return reject(ctx->error,QStringLiteral("Invalid repeat index name"));
        for(const auto& a:axes)if(a.first==repeatIndex)return reject(ctx->error,QStringLiteral("Repeat index conflicts with a sweep axis"));
    }
    std::function<bool(int,QJsonObject)> cycle;
    cycle=[&](int axisIndex,QJsonObject current)->bool {
        if(axisIndex<axes.size()) {
            const auto& a=axes.at(axisIndex);
            for(const auto& v:a.second){current.insert(a.first,v);if(!cycle(axisIndex+1,current))return false;}
            return true;
        }
        for(int iteration=0;iteration<repeatCount;++iteration) {
            QJsonObject vars=current;
            if(hasRepeat)vars.insert(repeatIndex,iteration);
            if(hasChildren) {
                if(!node.value(QStringLiteral("steps")).isArray())return reject(ctx->error,QStringLiteral("Loop steps must be an array"));
                const QJsonArray children=node.value(QStringLiteral("steps")).toArray();
                if(children.isEmpty())return reject(ctx->error,QStringLiteral("Loop must contain at least one step"));
                for(const auto& item:children) {
                    if(!item.isObject())return reject(ctx->error,QStringLiteral("Loop child must be an object"));
                    const auto child=item.toObject();
                    if(child.contains(QStringLiteral("group")) && child.value(QStringLiteral("group")).toString()!=group)
                        return reject(ctx->error,QStringLiteral("Loop child cannot change its parent group"));
                    if(!visit(child,vars,group,depth+1,repeated||hasLoop||hasRepeat,ctx,guards))return false;
                }
            } else {
                QJsonObject step=node;
                step.remove(QStringLiteral("loop"));step.remove(QStringLiteral("repeat"));
                if(!guards.isEmpty())step.insert(QStringLiteral("guards"),guards);
                const QString originalId=step.value(QStringLiteral("id")).toString();
                if(originalId.isEmpty())return reject(ctx->error,QStringLiteral("Step ID must be nonempty"));
                QJsonObject params=step.value(QStringLiteral("parameters")).toObject();
                // Inherited sweep parameters override identically named defaults.
                for(auto it=vars.begin();it!=vars.end();++it)params.insert(it.key(),it.value());
                if(!params.isEmpty())step.insert(QStringLiteral("parameters"),params);
                if(!resolveLimits(&step,params,ctx->error))return false;
                QString name=step.value(QStringLiteral("name")).toString();
                for(auto it=vars.begin();it!=vars.end();++it)
                    name.replace(QStringLiteral("${%1}").arg(it.key()),valueText(it.value()));
                if((repeated||hasLoop||hasRepeat) && name==step.value(QStringLiteral("name")).toString()) {
                    QStringList items;for(auto it=vars.begin();it!=vars.end();++it)items<<it.key()+QStringLiteral("=")+valueText(it.value());
                    if(!items.isEmpty())name+=QStringLiteral(" [")+items.join(QStringLiteral(", "))+QStringLiteral("]");
                }
                step.insert(QStringLiteral("name"),name.left(120));
                if(!step.contains(QStringLiteral("group")))step.insert(QStringLiteral("group"),group);
                if(repeated||hasLoop||hasRepeat) {
                    step.insert(QStringLiteral("templateId"),originalId);
                    step.insert(QStringLiteral("id"),originalId.left(48)+QStringLiteral("__%1").arg(++ctx->counter,4,10,QLatin1Char('0')));
                }
                const QString id=step.value(QStringLiteral("id")).toString();
                if(ctx->ids.contains(id))return reject(ctx->error,QStringLiteral("Duplicate expanded step ID '%1'").arg(id));
                ctx->ids.insert(id);
                ctx->output.append(step);
                if(ctx->output.size()>TestSequenceCapacity::maxExpandedSteps)return reject(ctx->error,
            QStringLiteral("Expanded sequence exceeds %1 executable steps").arg(TestSequenceCapacity::maxExpandedSteps));
            }
        }
        return true;
    };
    return cycle(0,inherited);
}
} // namespace detail

inline bool expand(const QJsonObject& definition,QJsonObject* expanded,QString* error=nullptr) {
    if(!expanded)return detail::reject(error,QStringLiteral("Missing output definition"));
    const QJsonArray source=definition.value(QStringLiteral("steps")).toArray();
    if(source.isEmpty())return detail::reject(error,QStringLiteral("Sequence has no steps"));
    detail::Context ctx;ctx.error=error;ctx.variables=definition.value(QStringLiteral("variables")).toArray();
    if(definition.contains(QStringLiteral("variables")) && !definition.value(QStringLiteral("variables")).isArray())
        return detail::reject(error,QStringLiteral("Test variables must be an array"));
    if(!TestSequenceConditions::validateVariables(ctx.variables,error))return false;
    QSet<QString> authoredIds;
    for(const QJsonValue& value:source) {
        if(!value.isObject())return detail::reject(error,QStringLiteral("Step must be a JSON object"));
        const QJsonObject node=value.toObject();
        const QString id=node.value(QStringLiteral("id")).toString();
        if(id.isEmpty()||id.size()>64||authoredIds.contains(id))
            return detail::reject(error,QStringLiteral("Top-level step/group IDs must be distinct and nonempty"));
        authoredIds.insert(id);
        const QString group=node.value(QStringLiteral("group")).toString(QStringLiteral("main"));
        if(!QSet<QString>{QStringLiteral("setup"),QStringLiteral("main"),
                          QStringLiteral("cleanup")}.contains(group))
            return detail::reject(error,QStringLiteral("Invalid group '%1'").arg(group));
        if(node.contains(QStringLiteral("steps")) && !node.contains(QStringLiteral("condition"))) {
            for(auto it=node.begin();it!=node.end();++it)
                if(!QSet<QString>{QStringLiteral("id"),QStringLiteral("name"),
                    QStringLiteral("description"),QStringLiteral("group"),
                    QStringLiteral("loop"),QStringLiteral("repeat"),QStringLiteral("steps")}.contains(it.key()))
                    return detail::reject(error,QStringLiteral("Unknown loop group property '%1'").arg(it.key()));
        }
    }
    // The native executive owns Setup/Main/Cleanup ordering. Expand each group
    // in its original order to keep one iteration's children adjacent.
    for(const QString& group:{QStringLiteral("setup"),QStringLiteral("main"),QStringLiteral("cleanup")}) {
        for(const auto& v:source) {
            if(!v.isObject())return detail::reject(error,QStringLiteral("Sequence step must be an object"));
            const QJsonObject node=v.toObject();
            if(node.value(QStringLiteral("group")).toString(QStringLiteral("main"))!=group)continue;
            if(!detail::visit(node,QJsonObject{},group,0,false,&ctx))return false;
        }
    }
    QJsonObject copy=definition;copy.insert(QStringLiteral("steps"),ctx.output);
    *expanded=copy;
    return true;
}
} // namespace TestSequenceLoops
