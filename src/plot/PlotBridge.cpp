#include "PlotBridge.h"
#include "../WaveFormsBindings.h"
#include <QRegularExpression>
#include <QMetaType>
#include <QStringList>
#include <cmath>
#include <utility>

namespace {
constexpr int kMaxPoints=250000;
bool validSeriesName(const QString& name) {
    return !name.trimmed().isEmpty() && name.size()<=64;
}
bool validColor(const QVariant& v) {
    static const QRegularExpression rgb(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return v.metaType().id()==QMetaType::QString &&
           rgb.match(v.toString()).hasMatch();
}
}
PlotBridge::PlotBridge(QSet<QString> ids, ScopeBridge* scope, QObject* parent)
    :QObject(parent),m_plotIds(std::move(ids)),m_scope(scope) {}
bool PlotBridge::reject(const QString& error) {m_lastError=error;return false;}
bool PlotBridge::validPlot(const QString& id) {
    if(!m_plotIds.contains(id))return reject(QStringLiteral("Unknown Panel plot ID: %1").arg(id));
    m_lastError.clear();
    return true;
}
bool PlotBridge::validNewSeries(const QString& id,const QString& name) {
    if(m_seriesNames.value(id).contains(name))return true;
    if(m_seriesNames.value(id).size()>=12)
        return reject(QStringLiteral("Plot %1 already contains 12 named series").arg(id));
    return true;
}
bool PlotBridge::readNumbers(const QVariantList& source,QVector<double>* dest) {
    if(source.size()<1 || source.size()>kMaxPoints)return false;
    dest->reserve(source.size());
    for(const QVariant& item:source) {
        if(!item.isValid() || item.isNull() || item.metaType().id()==QMetaType::QString ||
           item.metaType().id()==QMetaType::Bool)return false;
        bool ok=false;
        const double v=item.toDouble(&ok);
        if(!ok || !std::isfinite(v))return false;
        dest->append(v);
    }
    return true;
}
bool PlotBridge::validSeriesOptions(const QVariantMap& options) {
    for(auto it=options.cbegin();it!=options.cend();++it) {
        const QString key=it.key(); const QVariant& v=it.value();
        if(key==QLatin1String("color")) {
            if(!validColor(v))return reject(QStringLiteral("Series color must be #RRGGBB"));
        } else if(key==QLatin1String("visible")) {
            if(v.metaType().id()!=QMetaType::Bool)return reject(QStringLiteral("Series visible must be boolean"));
        } else if(key==QLatin1String("width") || key==QLatin1String("markerSize")) {
            bool ok=false;const double d=v.toDouble(&ok);
            const bool clearWidth=key==QLatin1String("width") && d==0.0;
            if(!ok || !std::isfinite(d) || (!clearWidth && (d<0.5 || d>12)))
                return reject(QStringLiteral("Series width must be 0 (inherit) or 0.5-12; markerSize must be 0.5-12"));
        } else if(key==QLatin1String("axis")) {
            if(v.toString()!=QLatin1String("left") && v.toString()!=QLatin1String("right"))
                return reject(QStringLiteral("Series axis must be left or right"));
        } else return reject(QStringLiteral("Unknown series option: %1").arg(key));
    }
    return true;
}
bool PlotBridge::setSeries(const QString& id,const QString& name,
                           const QVariantList& xx,const QVariantList& yy,
                           const QVariantMap& options) {
    if(!validPlot(id))return false;
    if(!validSeriesName(name))return reject(QStringLiteral("Series name must be 1-64 characters"));
    if(!validNewSeries(id,name))return false;
    if(xx.size()!=yy.size())return reject(QStringLiteral("X/Y series lengths must match"));
    if(!validSeriesOptions(options))return false;
    PlotCommand c;
    c.plotId=id;c.seriesName=name;c.options=options;
    if(!readNumbers(xx,&c.x) || !readNumbers(yy,&c.y))
        return reject(QStringLiteral("Series requires 1-250000 finite numeric X/Y pairs"));
    m_seriesNames[id].insert(name);
    emit commandReady(c);return true;
}
bool PlotBridge::appendSeries(const QString& id,const QString& name,
                              const QVariantList& xx,const QVariantList& yy,
                              const QVariantMap& options) {
    if(!validPlot(id))return false;
    if(!validSeriesName(name) || xx.size()!=yy.size())
        return reject(QStringLiteral("Invalid append series name or unequal X/Y lengths"));
    if(!validNewSeries(id,name))return false;
    if(!validSeriesOptions(options))return false;
    PlotCommand c;c.action=PlotCommand::Action::AppendSeries;
    c.plotId=id;c.seriesName=name;c.options=options;
    if(!readNumbers(xx,&c.x) || !readNumbers(yy,&c.y))
        return reject(QStringLiteral("Append requires 1-250000 finite numeric X/Y pairs"));
    m_seriesNames[id].insert(name);
    emit commandReady(c);return true;
}
bool PlotBridge::appendPoint(const QString& id,const QString& name,double x,double y) {
    if(!validPlot(id))return false;
    if(!validSeriesName(name) || !std::isfinite(x) || !std::isfinite(y))
        return reject(QStringLiteral("Invalid series name or nonfinite point"));
    if(!validNewSeries(id,name))return false;
    PlotCommand c;c.action=PlotCommand::Action::AppendSeries;
    c.plotId=id;c.seriesName=name;c.x={x};c.y={y};
    m_seriesNames[id].insert(name);
    emit commandReady(c);return true;
}
bool PlotBridge::setSamples(const QString& id,const QString& name,const QVariantList& yy,
                            double rate,double start,const QVariantMap& options) {
    if(!validPlot(id))return false;
    if(!validSeriesName(name))return reject(QStringLiteral("Series name must be 1-64 characters"));
    if(!validNewSeries(id,name))return false;
    if(!std::isfinite(rate) || rate<=0 || !std::isfinite(start))
        return reject(QStringLiteral("Sample rate must be positive and start time finite"));
    if(!validSeriesOptions(options))return false;
    PlotCommand c;c.plotId=id;c.seriesName=name;c.options=options;
    if(!readNumbers(yy,&c.y))return reject(QStringLiteral("Samples require 1-250000 finite values"));
    c.x.reserve(c.y.size());
    for(qsizetype i=0;i<c.y.size();++i) {
        const double x=start+double(i)/rate;
        if(!std::isfinite(x))return reject(QStringLiteral("Time-axis sample X exceeds finite range"));
        c.x.append(x);
    }
    m_seriesNames[id].insert(name);
    emit commandReady(c);return true;
}
bool PlotBridge::fromScope(const QString& id,int channel,const QString& name) {
    if(!validPlot(id))return false;
    if(channel!=1 && channel!=2)return reject(QStringLiteral("Scope channel must be 1 or 2"));
    if(!validSeriesName(name))return reject(QStringLiteral("Invalid scope series name"));
    if(!validNewSeries(id,name))return false;
    if(!m_scope)return reject(QStringLiteral("Scope interface unavailable"));
    const auto* sc=static_cast<ScopeChannel*>(channel==1?m_scope->channel1():m_scope->channel2());
    const QVector<double> values=sc->getData();
    const double rate=sc->sampleRateHz();
    if(values.isEmpty() || values.size()>kMaxPoints || !std::isfinite(rate) || rate<=0)
        return reject(QStringLiteral("Capture Scope1.wait() successfully before plots.fromScope()"));
    PlotCommand c;c.plotId=id;c.seriesName=name;
    c.y=values;c.x.reserve(values.size());
    for(qsizetype i=0;i<values.size();++i) {
        if(!std::isfinite(values.at(i)))return reject(QStringLiteral("Capture includes nonfinite samples"));
        c.x.append(double(i)/rate);
    }
    m_seriesNames[id].insert(name);
    emit commandReady(c);return true;
}
bool PlotBridge::configure(const QString& id,const QVariantMap& settings) {
    if(!validPlot(id))return false;
    if(settings.isEmpty())return reject(QStringLiteral("No plot options supplied"));
    for(auto it=settings.cbegin();it!=settings.cend();++it) {
        const QString k=it.key();const QVariant v=it.value();
        if(k==QLatin1String("title") || k==QLatin1String("xLabel") || k==QLatin1String("yLabel") || k==QLatin1String("y2Label")) {
            if(v.metaType().id()!=QMetaType::QString || v.toString().size()>100)
                return reject(QStringLiteral("Invalid plot label: %1").arg(k));
        } else if(k==QLatin1String("chartType")) {
            if(!QStringList({"Line","XY","Scatter","Step","Bar","Area","Digital"}).contains(v.toString()))
                return reject(QStringLiteral("chartType must be Line, XY, Scatter, Step, Bar, or Area"));
        } else if(k==QLatin1String("xAuto") || k==QLatin1String("yAuto") ||
                  k==QLatin1String("showGrid") || k==QLatin1String("showLegend") ||
                  k==QLatin1String("xLog") || k==QLatin1String("yLog") ||
                  k==QLatin1String("showCursors") || k==QLatin1String("showY2") ||
                  k==QLatin1String("showY2Labels") || k==QLatin1String("showXAxis") ||
                  k==QLatin1String("showYAxis") || k==QLatin1String("y2Auto")) {
            if(v.metaType().id()!=QMetaType::Bool)
                return reject(QStringLiteral("Option %1 must be boolean").arg(k));
        } else if(k==QLatin1String("xMin") || k==QLatin1String("xMax") ||
                  k==QLatin1String("yMin") || k==QLatin1String("yMax") ||
                  k==QLatin1String("lineWidth") || k==QLatin1String("cursorA") ||
                  k==QLatin1String("cursorB") || k==QLatin1String("y2Min") || k==QLatin1String("y2Max")) {
            bool ok=false;const double d=v.toDouble(&ok);
            if(!ok || !std::isfinite(d) ||
               (k==QLatin1String("lineWidth") && (d<0.5 || d>8)))
                return reject(QStringLiteral("Invalid numeric plot option: %1").arg(k));
        } else return reject(QStringLiteral("Unknown plot option: %1").arg(k));
    }
    PlotCommand c;c.action=PlotCommand::Action::Configure;c.plotId=id;c.options=settings;
    emit commandReady(c);return true;
}
bool PlotBridge::clear(const QString& id) {
    if(!validPlot(id))return false;
    PlotCommand c;c.action=PlotCommand::Action::Clear;c.plotId=id;
    m_seriesNames.remove(id);
    emit commandReady(c);return true;
}
bool PlotBridge::removeSeries(const QString& id,const QString& name) {
    if(!validPlot(id))return false;
    if(!validSeriesName(name))return reject(QStringLiteral("Invalid series name"));
    PlotCommand c;c.action=PlotCommand::Action::RemoveSeries;c.plotId=id;c.seriesName=name;
    m_seriesNames[id].remove(name);
    emit commandReady(c);return true;
}

bool PlotBridge::styleSeries(const QString& id,const QString& name,const QVariantMap& options) {
    if(!validPlot(id))return false;
    if(!validSeriesName(name) || !m_seriesNames.value(id).contains(name))
        return reject(QStringLiteral("Unknown series: %1").arg(name));
    if(options.isEmpty())return reject(QStringLiteral("No series styling supplied"));
    if(!validSeriesOptions(options))return false;
    PlotCommand c;c.action=PlotCommand::Action::StyleSeries;
    c.plotId=id;c.seriesName=name;c.options=options;
    emit commandReady(c);return true;
}
