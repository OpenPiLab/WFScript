#include "PanelChartWidget.h"
#include "PlotAxisLabels.h"
#include <QFont>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QCursor>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QWheelEvent>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const QColor seriesColors[] = {QColor("#47cce4"),QColor("#f3c34e"),QColor("#49d2a1"),
                               QColor("#ec82bb"),QColor("#b199f5"),QColor("#fa9160")};
constexpr int kMaxSeries=12;
QString axisNumber(double v) {
    if(!std::isfinite(v)) return QStringLiteral("—");
    if(v == 0.0 || std::abs(v)<1e-14) return QStringLiteral("0");
    // Common engineering prefixes improve time/frequency ticks.
    const double a=std::abs(v);
    if(a>=1e9) return QString::number(v/1e9,'g',4)+"G";
    if(a>=1e6) return QString::number(v/1e6,'g',4)+"M";
    if(a>=1e3) return QString::number(v/1e3,'g',4)+"k";
    if(a<1e-6) return QString::number(v*1e9,'g',4)+"n";
    if(a<1e-3) return QString::number(v*1e6,'g',4)+"µ";
    if(a<0.1) return QString::number(v*1e3,'g',4)+"m";
    return QString::number(v,'g',5);
}
// Pick decimal decade subdivisions 1, 2, 5 where space permits.
QVector<double> ticks(double low,double high,bool logarithmic,int desired) {
    QVector<double> out;
    if(!std::isfinite(low) || !std::isfinite(high) || low>=high)return out;
    if(logarithmic) {
        if(low<=0)return out;
        const double decades=std::log10(high/low);
        const int skip=std::max(1,int(std::ceil(decades/std::max(1,desired-1))));
        const int minExp=std::max(-300,int(std::floor(std::log10(low))));
        const int maxExp=std::min(300,int(std::ceil(std::log10(high))));
        const bool fine=decades<=2.2;
        for(int e=minExp;e<=maxExp;e+=skip) {
            const double decade=std::pow(10.0,e);
            for(double multiple : {1.0,2.0,5.0}) {
                if(!fine && multiple!=1.0)continue;
                const double v=decade*multiple;
                if(v>=low*(1-1e-12) && v<=high*(1+1e-12))out.push_back(v);
            }
        }
    } else {
        const double rough=(high-low)/std::max(1,desired);
        const double decade=std::pow(10.0,std::floor(std::log10(rough)));
        double step=decade;
        for(double mul : {1.0,2.0,5.0,10.0}) {step=decade*mul;if(step>=rough)break;}
        const double begin=std::ceil(low/step)*step;
        for(int i=0;i<200;++i) {
            double v=begin+i*step;
            if(v>high+step*1e-9)break;
            if(std::abs(v)<=step*1e-8)v=0.0;
            out.push_back(v);
        }
    }
    return out;
}
}
QVariantMap PanelChartWidget::defaults() {
    return {{"chartType","Line"},{"title","Plot"},{"xLabel","X"},{"yLabel","Y"},
            {"xAuto",true},{"yAuto",true},{"xMin",0.0},{"xMax",1.0},
            {"yMin",-1.0},{"yMax",1.0},{"showGrid",true},{"showLegend",true},{"showXAxis",true},{"showYAxis",true},{"showY2Labels",true},
            {"lineWidth",1.8},{"showY2",false},{"y2Auto",true},{"y2Min",0.0},{"y2Max",100.0},{"y2Label","Right Y"},{"xLog",false},{"yLog",false},
            {"showCursors",false},{"cursorA",0.0},{"cursorB",1.0},
            {"nativeEnvelopeOpacity",0.32},{"legacyEnvelopeOpacity",0.32},
            {"digitalPerfOverlay",false}};
}
PanelChartWidget::PanelChartWidget(QWidget* parent):QWidget(parent),m_settings(defaults()) {
    setAutoFillBackground(false);
    setMinimumSize(120,85);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(QStringLiteral("Shift+mouse: snap cursor A | Ctrl+mouse: snap cursor B | Measurement X: clear cursors | Drag: pan | Legend: toggle trace | Wheel: zoom"));
    // Interactive events must not flood the script worker with viewChanged/UI
    // round-trips. Publish the latest viewport after a short idle interval.
    m_viewNotificationTimer.setSingleShot(true);
    m_viewNotificationTimer.setInterval(40);
    connect(&m_viewNotificationTimer,&QTimer::timeout,this,&PanelChartWidget::publishView);
    // While dragging or scrolling, paint a cheaper pixel-column preview.
    // Once input settles, redraw the exact per-pixel envelope/full samples.
    m_renderIdleTimer.setSingleShot(true);
    m_renderIdleTimer.setInterval(85);
    connect(&m_renderIdleTimer,&QTimer::timeout,this,[this]{
        m_fastInteraction=false;
        update();
    });
}
qsizetype PanelChartWidget::pointCount(const QString& name) const {
    for(const Series& s:m_series)if(s.name==name)return s.nativeSamples?s.y.size():s.x.size();
    return 0;
}
bool PanelChartWidget::hasSeries(const QString& name) const {
    for(const Series& s:m_series)if(s.name==name)return true;
    return false;
}
void PanelChartWidget::applyDefinition(const PanelControl& def) {
    QVariantMap properties=defaults();
    properties.insert("title",def.text.isEmpty()?QStringLiteral("Plot"):def.text);
    for(auto it=def.properties.begin();it!=def.properties.end();++it)
        properties.insert(it.key(),it.value().toVariant());
    const bool wasDigital=m_settings.value("chartType").toString()==QLatin1String("Digital");
    m_settings=properties;
    const bool isDigital=m_settings.value("chartType").toString()==QLatin1String("Digital");
    if(wasDigital!=isDigital)for(Series& s:m_series){
        if(!s.nativeSamples){
            if(isDigital)s.digitalIndex.build(s.x,s.y);
            else s.digitalIndex=DigitalEdgeIndex{};
        }
        s.digitalGeometry.valid=false;
    }
    m_haveCursorA=m_haveCursorB=false;
    m_inspectorDismissed=false;
    m_cursorsCleared=false;
    m_paneMoved=false;
    m_viewActive=false; // settings changed; series always remain owned here
    update();
}
void PanelChartWidget::updateSeries(const PlotCommand& cmd) {
    int found=-1;
    for(int i=0;i<m_series.size();++i)if(m_series[i].name==cmd.seriesName){found=i;break;}
    if(found<0 && m_series.size()>=kMaxSeries)return;
    Series s;
    s.name=cmd.seriesName;s.x=cmd.x;s.y=cmd.y;
    s.nativeSamples=cmd.nativeSamples;s.nativeStart=cmd.nativeStart;s.nativeStep=cmd.nativeStep;
    if(s.nativeSamples)s.nativeIndex.build(s.y);
    else {
        s.legacyIndex.build(s.x,s.y);
        if(m_settings.value("chartType").toString()==QLatin1String("Digital"))
            s.digitalIndex.build(s.x,s.y);
    }
    if(found>=0){
        s.color=m_series.at(found).color;
        s.width=m_series.at(found).width;
        s.markerSize=m_series.at(found).markerSize;
        s.visible=m_series.at(found).visible;
        s.axis=m_series.at(found).axis;
    }
    else s.color=seriesColors[m_series.size()%int(sizeof(seriesColors)/sizeof(seriesColors[0]))];
    if(cmd.options.contains("color"))s.color=QColor(cmd.options.value("color").toString());
    if(cmd.options.contains("width"))s.width=cmd.options.value("width").toDouble();
    if(cmd.options.contains("markerSize"))s.markerSize=cmd.options.value("markerSize").toDouble();
    if(cmd.options.contains("visible"))s.visible=cmd.options.value("visible").toBool();
    if(cmd.options.contains("axis"))s.axis=cmd.options.value("axis").toString();
    s.xAscending=s.nativeSamples?true:s.legacyIndex.ascending();
    if(found>=0)m_series[found]=std::move(s);
    else m_series.append(std::move(s));
}
void PanelChartWidget::applyCommand(const PlotCommand& cmd) {
    switch(cmd.action) {
    case PlotCommand::Action::ReplaceSeries:updateSeries(cmd);break;
    case PlotCommand::Action::AppendSeries: {
        int found=-1;
        for(int i=0;i<m_series.size();++i)
            if(m_series.at(i).name==cmd.seriesName){found=i;break;}
        if(found<0)updateSeries(cmd);
        else {
            Series& s=m_series[found];
            if(s.nativeSamples){
                // Appending arbitrary XY data changes the series model; switch
                // deliberately to the existing ordinary-array representation.
                s.x.reserve(s.y.size()+cmd.x.size());
                for(qsizetype i=0;i<s.y.size();++i)
                    s.x.append(s.nativeStart+double(i)*s.nativeStep);
                s.nativeSamples=false;
                s.nativeIndex=NativeSampleIndex{};
            }
            const std::size_t oldCount=std::size_t(s.y.size());
            s.x+=cmd.x;s.y+=cmd.y;
            constexpr qsizetype maxPoints=250000;
            if(s.x.size()>maxPoints){
                const qsizetype drop=s.x.size()-maxPoints;
                s.x.remove(0,drop);s.y.remove(0,drop);
                s.legacyIndex.build(s.x,s.y); // ring front-trim changes all index positions
                if(m_settings.value("chartType").toString()==QLatin1String("Digital"))
                    s.digitalIndex.build(s.x,s.y);
            } else {
                s.legacyIndex.append(s.x,s.y,oldCount);
                if(m_settings.value("chartType").toString()==QLatin1String("Digital"))
                    s.digitalIndex.append(s.x,s.y,oldCount);
            }
            s.legacyGeometry.valid=false;
            s.digitalGeometry.valid=false;
            s.xAscending=s.legacyIndex.ascending();
            if(cmd.options.contains("color") || cmd.options.contains("width"))
                s.digitalGeometry.valid=false; // raster contains the stroke color and width
            if(cmd.options.contains("color"))s.color=QColor(cmd.options.value("color").toString());
            if(cmd.options.contains("width"))s.width=cmd.options.value("width").toDouble();
            if(cmd.options.contains("markerSize"))s.markerSize=cmd.options.value("markerSize").toDouble();
            if(cmd.options.contains("visible"))s.visible=cmd.options.value("visible").toBool();
            if(cmd.options.contains("axis"))s.axis=cmd.options.value("axis").toString();
        }
        break;
    }
    case PlotCommand::Action::RemoveSeries:
        for(int i=m_series.size()-1;i>=0;--i)
            if(m_series.at(i).name==cmd.seriesName)m_series.removeAt(i);
        break;
    case PlotCommand::Action::Clear:m_series.clear();m_viewActive=false;m_haveCursorA=m_haveCursorB=false;break;
    case PlotCommand::Action::StyleSeries:
        for(Series& s:m_series)if(s.name==cmd.seriesName) {
            if((cmd.options.contains("color") && QColor(cmd.options.value("color").toString())!=s.color) ||
               (cmd.options.contains("width") && cmd.options.value("width").toDouble()!=s.width))
                s.digitalGeometry.valid=false; // color/width are baked into cached raster
            if(cmd.options.contains("color"))s.color=QColor(cmd.options.value("color").toString());
            if(cmd.options.contains("width"))s.width=cmd.options.value("width").toDouble();
            if(cmd.options.contains("markerSize"))s.markerSize=cmd.options.value("markerSize").toDouble();
            if(cmd.options.contains("visible"))s.visible=cmd.options.value("visible").toBool();
            if(cmd.options.contains("axis"))s.axis=cmd.options.value("axis").toString();
            break;
        }
        break;
    case PlotCommand::Action::SetView:
        m_viewX0=cmd.options.value("xMin").toDouble();
        m_viewX1=cmd.options.value("xMax").toDouble();
        m_viewY0=cmd.options.value("yMin").toDouble();
        m_viewY1=cmd.options.value("yMax").toDouble();
        m_viewActive=true;publishView();break;
    case PlotCommand::Action::Configure:
        if(cmd.options.contains("cursorA") || cmd.options.contains("cursorB") ||
           (cmd.options.contains("showCursors") && cmd.options.value("showCursors").toBool())) {
            m_cursorsCleared=false;
            m_inspectorDismissed=false;
        }
        // Appearance/configuration operations such as envelope opacity, grid,
        // title, and cursors never discard an active zoom. Only an actual axis
        // domain change releases the view, allowing new manual axes to apply.
        bool axisChanged=false;
        const bool strokeChanged=cmd.options.contains("lineWidth") &&
            m_settings.value("lineWidth")!=cmd.options.value("lineWidth");
        const bool wasDigital=m_settings.value("chartType").toString()==QLatin1String("Digital");
        static const QStringList axisKeys={
            QStringLiteral("chartType"),QStringLiteral("xAuto"),QStringLiteral("yAuto"),
            QStringLiteral("xMin"),QStringLiteral("xMax"),
            QStringLiteral("yMin"),QStringLiteral("yMax"),
            QStringLiteral("xLog"),QStringLiteral("yLog"),
            QStringLiteral("showY2"),QStringLiteral("y2Auto"),
            QStringLiteral("y2Min"),QStringLiteral("y2Max")};
        for(auto it=cmd.options.cbegin();it!=cmd.options.cend();++it){
            if(axisKeys.contains(it.key()) && m_settings.value(it.key())!=it.value())
                axisChanged=true;
            m_settings.insert(it.key(),it.value());
        }
        if(axisChanged)m_viewActive=false;
        if(strokeChanged)for(Series& s:m_series)s.digitalGeometry.valid=false;
        const bool isDigital=m_settings.value("chartType").toString()==QLatin1String("Digital");
        if(wasDigital!=isDigital)for(Series& s:m_series){
            if(!s.nativeSamples){
                if(isDigital)s.digitalIndex.build(s.x,s.y);
                else s.digitalIndex=DigitalEdgeIndex{};
            }
            s.digitalGeometry.valid=false;
        }
        break;
    }
    update();
}
void PanelChartWidget::resetView() {
    m_viewNotificationTimer.stop();m_renderIdleTimer.stop();m_fastInteraction=false;
    m_viewActive=false;update();publishView();
}
void PanelChartWidget::publishView(){
    const QVariantMap bounds=resolvedRanges();
    QVariantMap state=bounds;
    state.insert(QStringLiteral("isZoomed"),m_viewActive);
    state.insert(QStringLiteral("seriesCount"),m_series.size());
    emit viewChanged(state);
}
QVariantMap PanelChartWidget::nativeRenderDiagnostics() const {
    return {{QStringLiteral("lastNativePrimitives"),qlonglong(m_lastNativePrimitives)},
            {QStringLiteral("geometryBuilds"),qlonglong(m_nativeGeometryBuilds)},
            {QStringLiteral("cacheHits"),qlonglong(m_nativeCacheHits)},
            {QStringLiteral("fastPreview"),m_fastInteraction}};
}
QVariantMap PanelChartWidget::legacyRenderDiagnostics() const {
    return {{QStringLiteral("lastLegacyPrimitives"),qlonglong(m_lastLegacyPrimitives)},
            {QStringLiteral("geometryBuilds"),qlonglong(m_legacyGeometryBuilds)},
            {QStringLiteral("cacheHits"),qlonglong(m_legacyCacheHits)},
            {QStringLiteral("fastPreview"),m_fastInteraction}};
}
QVariantMap PanelChartWidget::digitalRenderDiagnostics() const {
    return {{QStringLiteral("lastDigitalPrimitives"),qlonglong(m_lastDigitalPrimitives)},
            {QStringLiteral("geometryBuilds"),qlonglong(m_digitalGeometryBuilds)},
            {QStringLiteral("cacheHits"),qlonglong(m_digitalCacheHits)},
            {QStringLiteral("rasterBuilds"),qlonglong(m_digitalRasterBuilds)},
            {QStringLiteral("previewBlits"),qlonglong(m_digitalPreviewBlits)},
            {QStringLiteral("lastFrameMs"),m_lastDigitalFrameMs},
            {QStringLiteral("lastQueryMs"),m_lastDigitalQueryMs},
            {QStringLiteral("lastGeometryMs"),m_lastDigitalGeometryMs},
            {QStringLiteral("lastPaintMs"),m_lastDigitalPaintMs},
            {QStringLiteral("maxFrameMs"),m_maxDigitalFrameMs}};
}
QVariantMap PanelChartWidget::seriesStyle(const QString& name) const {
    for(const Series& s:m_series)if(s.name==name)
        return {{"color",s.color.name()},{"width",s.width},
                {"markerSize",s.markerSize},{"visible",s.visible},{"axis",s.axis}};
    return {};
}
QVariantMap PanelChartWidget::resolvedRanges() const {
    double xmin,xmax,ymin,ymax;
    ranges(xmin,xmax,ymin,ymax);
    return {{"xMin",xmin},{"xMax",xmax},{"yMin",ymin},{"yMax",ymax}};
}
double PanelChartWidget::effectiveSeriesWidth(const QString& name) const {
    const double base=qBound(0.5,m_settings.value("lineWidth").toDouble(),8.0);
    for(const Series& series:m_series)if(series.name==name)
        return series.width>0?series.width:base;
    return base;
}
QRectF PanelChartWidget::chartRect() const {
    const double scale=qBound(0.65,height()/300.0,1.4);
    const bool digital=m_settings.value("chartType").toString()==QLatin1String("Digital");
    return QRectF(rect()).adjusted((digital?110:65)*scale,42*scale,
       (m_settings.value("showY2").toBool()?-66:-23)*scale,-52*scale);
}
void PanelChartWidget::ranges(double& xmin,double& xmax,double& ymin,double& ymax) const {
    const bool xl=m_settings.value("xLog").toBool(),yl=m_settings.value("yLog").toBool();
    xmin=xl?1.0:0.0;xmax=xl?10.0:1.0;
    ymin=yl?1.0:-1.0;ymax=yl?10.0:1.0;
    bool anyX=false,anyY=false;
    for(const Series& s:m_series) {
        if(!s.visible)continue;
        const bool right=m_settings.value("showY2").toBool() && s.axis==QLatin1String("right");
        if(s.nativeSamples){
            if(!s.nativeIndex.valid()||s.y.isEmpty())continue;
            const double a=s.nativeStart,b=a+double(s.y.size()-1)*s.nativeStep;
            if(!xl || b>0){
                const double xlow=xl?std::max(a,std::numeric_limits<double>::min()):a;
                if(!anyX){xmin=xlow;xmax=b;anyX=true;}
                else {xmin=std::min(xmin,xlow);xmax=std::max(xmax,b);}
                if(!right){
                    const double low=yl?s.nativeIndex.minimumPositive():s.nativeIndex.minimum();
                    const double high=s.nativeIndex.maximum();
                    if(std::isfinite(low) && high>=low){
                        if(!anyY){ymin=low;ymax=high;anyY=true;}
                        else {ymin=std::min(ymin,low);ymax=std::max(ymax,high);}
                    }
                }
            }
            continue;
        }
        // For ordinary XY arrays all four combinations of linear/log valid
        // extents were calculated when the data arrived (not per paint/wheel).
        const LegacyXYExtent& bounds=s.legacyIndex.range(xl,!right && yl);
        if(!bounds.valid)continue;
        if(!anyX){xmin=bounds.xmin;xmax=bounds.xmax;anyX=true;}
        else {xmin=std::min(xmin,bounds.xmin);xmax=std::max(xmax,bounds.xmax);}
        if(!right){
            if(!anyY){ymin=bounds.ymin;ymax=bounds.ymax;anyY=true;}
            else {ymin=std::min(ymin,bounds.ymin);ymax=std::max(ymax,bounds.ymax);}
        }
    }
    if(!anyX){xmin=xl?1.0:0.0;xmax=xl?10.0:1.0;}
    if(!anyY){ymin=yl?1.0:-1.0;ymax=yl?10.0:1.0;}
    if(xmin>=xmax) {if(xl){xmin/=1.5;xmax*=1.5;}else{xmin-=0.5;xmax+=0.5;}}
    if(ymin>=ymax) {if(yl){ymin/=1.5;ymax*=1.5;}
        else {const double m=std::max(0.5,std::abs(ymin)*.1);ymin-=m;ymax+=m;}}
    // Manual limits are real axis bounds, not data filters. On a log axis
    // zero/negative endpoints cannot be transformed; repair only that end
    // using a three-decade window rather than silently reverting to auto.
    // All values remain in the user's original units (not log10 units).
    const auto manualBounds=[](double a,double b,bool log,double& lo,double& hi) {
        if(!std::isfinite(a) || !std::isfinite(b))return;
        if(!log) {
            if(a<b){lo=a;hi=b;}
            return;
        }
        if(b<=0) {
            if(a<=0)return; // no positive endpoint: keep valid auto view
            b=a*1000.0;
        }
        if(a<=0)a=b/1000.0;
        else if(a>=b)b=a*1000.0;
        if(a>0 && b>a && std::isfinite(a) && std::isfinite(b)) {
            lo=a;hi=b;
        }
    };
    if(m_settings.value("xAuto").toBool()) {
        if(xl){xmin/=1.06;xmax*=1.06;}
        else {const double pad=(xmax-xmin)*0.025;xmin-=pad;xmax+=pad;}
    } else manualBounds(m_settings.value("xMin").toDouble(),
                        m_settings.value("xMax").toDouble(),xl,xmin,xmax);
    if(m_settings.value("yAuto").toBool()) {
        if(yl){ymin/=1.12;ymax*=1.12;}
        else {const double pad=(ymax-ymin)*0.08;ymin-=pad;ymax+=pad;}
    } else manualBounds(m_settings.value("yMin").toDouble(),
                        m_settings.value("yMax").toDouble(),yl,ymin,ymax);
    if(m_viewActive && (!xl || m_viewX0>0) && (!yl || m_viewY0>0)) {
        xmin=m_viewX0;xmax=m_viewX1;ymin=m_viewY0;ymax=m_viewY1;
    }
}
void PanelChartWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.setRenderHint(QPainter::TextAntialiasing,true);
    bool light=false;
    for(const QWidget* w=this;w;w=w->parentWidget())
        if(w->property("wfPanelLight").isValid()) {light=w->property("wfPanelLight").toBool();break;}
    const QColor surface=light?QColor("#ffffff"):QColor("#152334");
    const QColor foreground=light?QColor("#23374a"):QColor("#e4eef8");
    const QColor muted=light?QColor("#637a91"):QColor("#94b2ca");
    const QColor lines=light?QColor("#dce6ef"):QColor("#304458");
    painter.setPen(QPen(light?QColor("#bdcfdf"):QColor("#435c74"),1));
    painter.setBrush(surface);
    painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),10,10);
    const QRectF plot=chartRect();
    if(plot.width()<30 || plot.height()<24)return;
    QFont font=painter.font();font.setPixelSize(qBound(9,int(height()/23),15));
    font.setWeight(QFont::DemiBold);painter.setFont(font);
    painter.setPen(foreground);
    painter.drawText(QRectF(14,6,width()-28,28),Qt::AlignHCenter|Qt::AlignVCenter,
                     m_settings.value("title").toString());
    font.setWeight(QFont::Normal);font.setPixelSize(qBound(8,int(height()/32),12));
    painter.setFont(font);
    if(m_settings.value("chartType").toString()==QLatin1String("Digital")) {
        drawDigital(painter,plot,light);
        if(cursorsEnabled())drawMeasurementPane(painter,plot,light,true);
        return;
    }
    double xmin,xmax,ymin,ymax;ranges(xmin,xmax,ymin,ymax);
    const bool xl=m_settings.value("xLog").toBool(), yl=m_settings.value("yLog").toBool();
    const auto tx=[&](double x){return xl?std::log10(x):x;};
    const auto ty=[&](double y){return yl?std::log10(y):y;};
    const double ax=tx(xmin),bx=tx(xmax),ay=ty(ymin),by=ty(ymax);
    const auto xCoord=[&](double x){return plot.left()+(tx(x)-ax)/(bx-ax)*plot.width();};
    const auto yCoord=[&](double y){return plot.bottom()-(ty(y)-ay)/(by-ay)*plot.height();};
    const auto drawXGrid=[&]() {
        const QVector<double> xt=ticks(xmin,xmax,xl,8);
        const QVector<QString> xlabels=xl ? QVector<QString>{} : PlotAxisLabels::linearX(xt);
        double prevLabel=-1e6;
        for(int i=0;i<xt.size();++i) {
            const double x=xt[i];
            const double px=xCoord(x);
            if(m_settings.value("showGrid").toBool()){
                painter.setPen(QPen(lines,1));painter.drawLine(QPointF(px,plot.top()),QPointF(px,plot.bottom()));
            }
            if(m_settings.value("showXAxis",true).toBool() && (px-prevLabel>=43 || prevLabel<-1e5)) {
                painter.setPen(muted);
                painter.drawText(QRectF(px-39,plot.bottom()+6,78,18),Qt::AlignCenter,
                                 xl ? axisNumber(x) : xlabels[i]);
                prevLabel=px;
            }
        }
    };
    drawXGrid();
    for(double y:ticks(ymin,ymax,yl,6)) {
        const double py=yCoord(y);
        if(m_settings.value("showGrid").toBool()){
            painter.setPen(QPen(lines,1));painter.drawLine(QPointF(plot.left(),py),QPointF(plot.right(),py));
        }
        if(m_settings.value("showYAxis",true).toBool()) {
            painter.setPen(muted);
            painter.drawText(QRectF(3,py-10,plot.left()-9,20),Qt::AlignRight|Qt::AlignVCenter,axisNumber(y));
        }
    }
    double y2min=0,y2max=1;
    const bool showY2=m_settings.value("showY2").toBool();
    if(showY2){
        secondaryRanges(y2min,y2max);
        for(double v:ticks(y2min,y2max,false,6)) {
            const double py=plot.bottom()-(v-y2min)/(y2max-y2min)*plot.height();
            painter.setPen(muted);
            if(m_settings.value("showY2Labels",true).toBool())
            painter.drawText(QRectF(plot.right()+5,py-10,width()-plot.right()-7,20),
                             Qt::AlignLeft|Qt::AlignVCenter,axisNumber(v));
        }
    }
    if(showY2 && m_settings.value("showY2Labels",true).toBool()){
        painter.setPen(muted);
        QFont axisFont=painter.font();axisFont.setPixelSize(std::max(8,axisFont.pixelSize()-1));
        painter.save();painter.setFont(axisFont);
        painter.drawText(QRectF(plot.right()+3,plot.top()-22,width()-plot.right()-3,18),
                         Qt::AlignHCenter|Qt::AlignVCenter,m_settings.value("y2Label").toString());
        painter.restore();
    }
    painter.setPen(foreground);
    if(m_settings.value("showXAxis",true).toBool())
    painter.drawText(QRectF(plot.left(),height()-26,plot.width(),19),Qt::AlignCenter,
                     m_settings.value("xLabel").toString());
    if(m_settings.value("showYAxis",true).toBool()){
        painter.save();painter.translate(12,plot.center().y());painter.rotate(-90);
        painter.drawText(QRectF(-plot.height()/2,-10,plot.height(),18),Qt::AlignCenter,
                         m_settings.value("yLabel").toString());
        painter.restore();
    }
    painter.save();painter.setClipRect(plot.adjusted(-1,-1,1,1));
    m_lastNativePrimitives=0;
    m_lastLegacyPrimitives=0;
    const QString mode=m_settings.value("chartType").toString();
    const double lineWidth=qBound(.5,m_settings.value("lineWidth").toDouble(),8.0);
    for(const Series& s:m_series) {
        const qsizetype count=s.nativeSamples?s.y.size():s.x.size();
        if(count<=0 || !s.visible)continue;
        // Isolate per-series QPainter state. The rounded chart background above
        // uses a solid brush; inheriting it here would fill open line/XY/step
        // paths (implicitly closed by QPainter), obscuring the plot beneath.
        painter.save();
        const bool rightAxis=showY2 && s.axis==QLatin1String("right");
        const auto yCoord=[&](double y) {
            return rightAxis ? plot.bottom()-(y-y2min)/(y2max-y2min)*plot.height()
                             : plot.bottom()-(ty(y)-ay)/(by-ay)*plot.height();
        };
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(s.color,s.width>0?s.width:lineWidth));
        if(s.nativeSamples){
            // RC1.5: a million original samples never become a million Qt
            // drawing primitives. The existing 64-sample index reduces each
            // visible X-column to its real extrema; dense waveforms use one
            // un-antialiased vertical stroke plus at most one connector per
            // column. Close zoom draws the original individual samples.
            const double viewFirst=(xmin-s.nativeStart)/s.nativeStep;
            const double viewEnd=(xmax-s.nativeStart)/s.nativeStep;
            const double n=double(s.y.size());
            if(std::isfinite(viewFirst)&&std::isfinite(viewEnd)&&viewEnd>=0&&viewFirst<n){
                const std::size_t first=std::size_t(std::max(0.0,std::floor(viewFirst)-1.0));
                const std::size_t last=std::size_t(std::min(n,std::ceil(viewEnd)+2.0));
                NativeGeometry& geometry=s.nativeGeometry;
                const bool yLog=yl&&!rightAxis;
                if(!geometry.valid || geometry.xmin!=xmin || geometry.xmax!=xmax ||
                   geometry.ymin!=(rightAxis?y2min:ymin) ||
                   geometry.ymax!=(rightAxis?y2max:ymax) ||
                   geometry.plot!=plot || geometry.xLog!=xl ||
                   geometry.yLog!=yLog || geometry.preview!=m_fastInteraction){
                    geometry=NativeGeometry{};
                    geometry.valid=true;geometry.xmin=xmin;geometry.xmax=xmax;
                    geometry.ymin=rightAxis?y2min:ymin;
                    geometry.ymax=rightAxis?y2max:ymax;
                    geometry.plot=plot;geometry.xLog=xl;geometry.yLog=yLog;
                    geometry.preview=m_fastInteraction;
                    ++m_nativeGeometryBuilds;
                    const std::size_t widthPixels=std::size_t(std::max(1.0,std::ceil(plot.width())));
                    geometry.dense=last>first && last-first>widthPixels*2;
                    if(geometry.dense){
                        // During continuous wheel/drag input, reduce the number
                        // of columns temporarily, but keep true extrema even
                        // in that preview. Idle redraw restores every column.
                        const std::size_t columns=m_fastInteraction?
                            std::max(std::size_t(1),widthPixels/2):widthPixels;
                        const auto envelopes=s.nativeIndex.columnsByView(
                            s.y,s.nativeStart,s.nativeStep,xmin,xmax,columns,xl);
                        geometry.lines.reserve(qsizetype(envelopes.size()));
                        geometry.centerLines.reserve(qsizetype(envelopes.size()));
                        QPointF previousCenter;
                        bool havePrevious=false;
                        for(const NativePixelColumn& column:envelopes){
                            const NativeSampleExtrema& envelope=column.envelope;
                            if(!envelope.valid || !envelope.count || (yLog&&!envelope.hasPositive)){
                                havePrevious=false;continue;
                            }
                            const NativeSamplePoint& low=yLog?envelope.positiveLow:envelope.low;
                            const NativeSamplePoint& high=envelope.high;
                            const double x=column.x;
                            if(xl&&x<=0){havePrevious=false;continue;}
                            const double px=xCoord(x);
                            const double pyLow=yCoord(low.value),pyHigh=yCoord(high.value);
                            // The signal itself is an exact *column mean*.
                            // The separate extrema stroke still shows spikes/noise;
                            // neither representation changes the original data.
                            const double mean=envelope.sum/double(envelope.count);
                            const bool meanValid=std::isfinite(mean)&&(!yLog||mean>0);
                            const double pyMean=meanValid?yCoord(mean):0;
                            if(!std::isfinite(px)||!std::isfinite(pyLow)||!std::isfinite(pyHigh)){
                                havePrevious=false;continue;
                            }
                            geometry.lines.append(QLineF(px,pyLow,px,pyHigh));
                            if(meanValid&&std::isfinite(pyMean)){
                                const QPointF center(px,pyMean);
                                if(havePrevious)geometry.centerLines.append(QLineF(previousCenter,center));
                                previousCenter=center;
                                havePrevious=true;
                            } else havePrevious=false;
                        }
                        geometry.primitives=geometry.lines.size()+geometry.centerLines.size();
                    } else if(last>first){
                        // Fine zoom: the original samples are still available.
                        const auto points=s.nativeIndex.visible(s.y,first,last,widthPixels);
                        bool started=false;
                        for(const NativeSamplePoint& item:points){
                            const double x=s.nativeStart+double(item.index)*s.nativeStep;
                            if((xl&&x<=0)||(yLog&&item.value<=0)){started=false;continue;}
                            const QPointF pt(xCoord(x),yCoord(item.value));
                            if(!std::isfinite(pt.x())||!std::isfinite(pt.y())){
                                started=false;continue;
                            }
                            if(!started){geometry.finePath.moveTo(pt);started=true;}
                            else geometry.finePath.lineTo(pt);
                            ++geometry.primitives;
                        }
                    }
                } else {
                    ++m_nativeCacheHits;
                }
                m_lastNativePrimitives+=geometry.primitives;
                if(geometry.dense){
                    // Two screen-bounded drawing layers, never per-sample Qt
                    // primitives: subdued true min/max envelope plus the
                    // smooth antialiased mean trace. This is a *display* choice
                    // only; cursor/statistics/exports retain every sample.
                    const double opacity=qBound(0.0,m_settings.value("nativeEnvelopeOpacity",QVariant(0.32)).toDouble(),1.0);
                    if(opacity>0 && !geometry.lines.isEmpty()){
                        QColor envelopeColor=s.color;
                        envelopeColor.setAlphaF(opacity);
                        painter.setRenderHint(QPainter::Antialiasing,false);
                        painter.setPen(QPen(envelopeColor,1.0,Qt::SolidLine,Qt::FlatCap));
                        painter.drawLines(geometry.lines.constData(),int(geometry.lines.size()));
                    }
                    if(!geometry.centerLines.isEmpty()){
                        painter.setRenderHint(QPainter::Antialiasing,true);
                        painter.setPen(QPen(s.color,s.width>0?s.width:lineWidth,
                                            Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                        painter.drawLines(geometry.centerLines.constData(),int(geometry.centerLines.size()));
                    }
                } else {
                    painter.setRenderHint(QPainter::Antialiasing,true);
                    painter.drawPath(geometry.finePath);
                }
            }
            painter.restore();continue;
        }
        const int maxDraw=qMax(2000,int(plot.width()*5));
        const qsizetype step=qMax(qsizetype(1),(count+maxDraw-1)/maxDraw);
        const auto valid=[&](qsizetype i){return std::isfinite(s.x[i]) && std::isfinite(s.y[i]) &&
                (!xl || s.x[i]>0) && (rightAxis || !yl || s.y[i]>0);};
        if(mode==QLatin1String("Scatter")) {
            painter.setBrush(s.color);
            for(qsizetype i=0;i<count;i+=step)if(valid(i))
                painter.drawEllipse(QPointF(xCoord(s.x[i]),yCoord(s.y[i])),s.markerSize,s.markerSize);
            painter.setBrush(Qt::NoBrush);
        } else if(mode==QLatin1String("Bar")) {
            const double barWidth=qMax(1.0,plot.width()/qMax(1.0,double(count))*0.76);
            const double zero=yCoord(yl?ymin:qBound(ymin,0.0,ymax));
            painter.setBrush(s.color);
            for(qsizetype i=0;i<count;i+=step) {
                if(!valid(i))continue;
                const double px=xCoord(s.x[i]);
                painter.drawRect(QRectF(px-barWidth/2,std::min(zero,yCoord(s.y[i])),
                                        barWidth,std::abs(zero-yCoord(s.y[i]))));
            }
            painter.setBrush(Qt::NoBrush);
        } else if((mode==QLatin1String("Line") || mode==QLatin1String("XY")) &&
                  s.legacyIndex.ascending() && s.legacyIndex.allFinite() &&
                  (!xl || s.legacyIndex.allXPositive()) &&
                  (rightAxis || !yl || s.legacyIndex.allYPositive())) {
            // Legacy ordinary XY charts share the RC1.5.1 pixel-bounded
            // renderer, but bin by TRUE X coordinates (including irregular
            // intervals) instead of assuming native uniform sample spacing.
            // Index and axis summaries are built on set/append, not on repaint.
            const auto span=s.legacyIndex.visibleSpan(s.x,xmin,xmax);
            const std::size_t widthPixels=std::size_t(std::max(1.0,std::ceil(plot.width())));
            NativeGeometry& geometry=s.legacyGeometry;
            const bool yLog=yl&&!rightAxis;
            if(!geometry.valid || geometry.xmin!=xmin || geometry.xmax!=xmax ||
               geometry.ymin!=(rightAxis?y2min:ymin) ||
               geometry.ymax!=(rightAxis?y2max:ymax) ||
               geometry.plot!=plot || geometry.xLog!=xl ||
               geometry.yLog!=yLog || geometry.preview!=m_fastInteraction){
                geometry=NativeGeometry{};
                geometry.valid=true;geometry.xmin=xmin;geometry.xmax=xmax;
                geometry.ymin=rightAxis?y2min:ymin;
                geometry.ymax=rightAxis?y2max:ymax;
                geometry.plot=plot;geometry.xLog=xl;geometry.yLog=yLog;
                geometry.preview=m_fastInteraction;
                ++m_legacyGeometryBuilds;
                // Keep legacy per-sample antialiased appearance for medium
                // (e.g. Rigol 1700-point) previews, but cache that path.
                // Use the indexed envelope only for genuinely dense input.
                geometry.dense=count>qsizetype(plot.width()*4) &&
                               span.second>span.first &&
                               span.second-span.first>widthPixels*2;
                if(geometry.dense){
                    const std::size_t columns=m_fastInteraction?
                        std::max(std::size_t(1),widthPixels/2):widthPixels;
                    const auto envelopes=s.legacyIndex.columns(s.x,s.y,xmin,xmax,columns,xl);
                    geometry.lines.reserve(qsizetype(envelopes.size()));
                    geometry.centerLines.reserve(qsizetype(envelopes.size()));
                    QPointF previousCenter;
                    bool havePrevious=false;
                    for(const LegacyXYColumn& column:envelopes){
                        const NativeSampleExtrema& e=column.envelope;
                        if(!e.valid||!e.count){havePrevious=false;continue;}
                        const double px=xCoord(column.x);
                        const double yLo=yCoord(e.low.value),yHi=yCoord(e.high.value);
                        const double mean=e.sum/double(e.count);
                        if(!std::isfinite(px)||!std::isfinite(yLo)||!std::isfinite(yHi)){
                            havePrevious=false;continue;
                        }
                        geometry.lines.append(QLineF(px,yLo,px,yHi));
                        if(std::isfinite(mean) && (!yLog||mean>0)){
                            const double py=yCoord(mean);
                            if(std::isfinite(py)){
                                const QPointF center(px,py);
                                if(havePrevious)geometry.centerLines.append(QLineF(previousCenter,center));
                                previousCenter=center;havePrevious=true;
                            }else havePrevious=false;
                        }else havePrevious=false;
                    }
                    geometry.primitives=geometry.lines.size()+geometry.centerLines.size();
                }else{
                    // At close zoom show original points directly (plus
                    // immediate neighbors for continuity at plot boundaries).
                    const std::size_t from=span.first>0?span.first-1:span.first;
                    const std::size_t to=std::min(std::size_t(count),span.second+1);
                    bool started=false;
                    for(std::size_t i=from;i<to;++i){
                        const double xx=s.x[qsizetype(i)],yy=s.y[qsizetype(i)];
                        if((xl&&xx<=0)||(yLog&&yy<=0)){started=false;continue;}
                        const QPointF pt(xCoord(xx),yCoord(yy));
                        if(!std::isfinite(pt.x())||!std::isfinite(pt.y())){
                            started=false;continue;
                        }
                        if(!started){geometry.finePath.moveTo(pt);started=true;}
                        else geometry.finePath.lineTo(pt);
                        ++geometry.primitives;
                    }
                }
            }else ++m_legacyCacheHits;
            m_lastLegacyPrimitives+=geometry.primitives;
            if(geometry.dense){
                const double opacity=qBound(0.0,
                    m_settings.value("legacyEnvelopeOpacity",QVariant(0.32)).toDouble(),1.0);
                if(opacity>0 && !geometry.lines.isEmpty()){
                    QColor envelope=s.color;envelope.setAlphaF(opacity);
                    painter.setRenderHint(QPainter::Antialiasing,false);
                    painter.setPen(QPen(envelope,1.0,Qt::SolidLine,Qt::FlatCap));
                    painter.drawLines(geometry.lines.constData(),int(geometry.lines.size()));
                }
                if(!geometry.centerLines.isEmpty()){
                    painter.setRenderHint(QPainter::Antialiasing,true);
                    painter.setPen(QPen(s.color,s.width>0?s.width:lineWidth,
                                        Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                    painter.drawLines(geometry.centerLines.constData(),int(geometry.centerLines.size()));
                }
            }else{
                painter.setRenderHint(QPainter::Antialiasing,true);
                painter.drawPath(geometry.finePath);
            }
        } else {
            QPainterPath path;
            QPainterPath segment;
            bool started=false;
            qsizetype previousValid=-1;
            qsizetype segmentFirst=-1;
            qsizetype segmentLast=-1;
            const bool area=mode==QLatin1String("Area");
            const double base=yl?ymin:qBound(ymin,0.0,ymax);
            auto finishAreaSegment=[&]() {
                if(!area || segmentFirst<0 || segmentLast<0)return;
                QPainterPath fill=segment;
                fill.lineTo(xCoord(s.x[segmentLast]),yCoord(base));
                fill.lineTo(xCoord(s.x[segmentFirst]),yCoord(base));
                fill.closeSubpath();
                QColor color=s.color;color.setAlpha(52);
                painter.fillPath(fill,color);
                segment=QPainterPath();
                segmentFirst=segmentLast=-1;
            };
            for(qsizetype i=0;i<count;i+=step) {
                // A nonpositive log coordinate or a nonfinite sample breaks
                // the path. Area fills must also stop at that gap.
                if(!valid(i)) {
                    finishAreaSegment();
                    started=false;previousValid=-1;
                    continue;
                }
                const QPointF pt(xCoord(s.x[i]),yCoord(s.y[i]));
                if(!started){
                    path.moveTo(pt);
                    if(area){segment.moveTo(pt);segmentFirst=i;}
                    started=true;
                } else if(mode==QLatin1String("Step")) {
                    path.lineTo(pt.x(),yCoord(s.y[previousValid]));
                    path.lineTo(pt);
                } else {
                    path.lineTo(pt);
                    if(area)segment.lineTo(pt);
                }
                if(area)segmentLast=i;
                previousValid=i;
            }
            finishAreaSegment();
            // Only Area uses fillPath() above. Stroke-only chart types must
            // never inherit an active brush from other rendering branches.
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
        painter.restore();
    }
    painter.restore();
    if(m_series.isEmpty()){
        painter.setPen(muted);
        painter.drawText(plot,Qt::AlignCenter,
            QStringLiteral("No data — send X/Y series with plots.setSeries()"));
    }
    if(cursorsEnabled()) {
        const double cursorA=m_haveCursorA?m_cursorA:(m_cursorsCleared?std::numeric_limits<double>::quiet_NaN():m_settings.value("cursorA").toDouble());
        const double cursorB=m_haveCursorB?m_cursorB:(m_cursorsCleared?std::numeric_limits<double>::quiet_NaN():m_settings.value("cursorB").toDouble());
        double dataLo=0,dataHi=0;
        const bool hasData=sampleDomain(dataLo,dataHi);
        const bool aValid=hasData && std::isfinite(cursorA) && cursorA>=dataLo && cursorA<=dataHi && (!xl || cursorA>0);
        const bool bValid=hasData && std::isfinite(cursorB) && cursorB>=dataLo && cursorB<=dataHi && (!xl || cursorB>0);
        painter.save();painter.setClipRect(plot);
        painter.setPen(QPen(QColor("#f3c34e"),1,Qt::DashLine));
        if(aValid)painter.drawLine(QPointF(xCoord(cursorA),plot.top()),QPointF(xCoord(cursorA),plot.bottom()));
        painter.setPen(QPen(QColor("#49d2a1"),1,Qt::DashLine));
        if(bValid)painter.drawLine(QPointF(xCoord(cursorB),plot.top()),QPointF(xCoord(cursorB),plot.bottom()));
        painter.restore();
        drawMeasurementPane(painter,plot,light,false);
    }
    if(m_settings.value("showLegend").toBool() && !m_series.isEmpty()) {
        for(int i=0;i<m_series.size() && i<8;++i) {
            const Series& series=m_series.at(i);
            const QRectF r=legendRow(i);
            QColor color=series.color;
            if(!series.visible)color.setAlpha(85);
            painter.setPen(QPen(color,2));
            painter.drawLine(QPointF(r.left()+2,r.center().y()),QPointF(r.left()+18,r.center().y()));
            painter.setPen(series.visible?foreground:muted);
            painter.drawText(r.adjusted(23,0,-2,0),Qt::AlignVCenter|Qt::AlignLeft,series.name);
        }
    }
}
void PanelChartWidget::wheelEvent(QWheelEvent* ev) {
    if(!chartRect().contains(ev->position()) || m_series.isEmpty())return;
    const double delta=ev->angleDelta().y()!=0?ev->angleDelta().y()/120.0:ev->pixelDelta().y()/60.0;
    if(delta==0)return;
    double xmin,xmax,ymin,ymax;ranges(xmin,xmax,ymin,ymax);
    const double factor=std::pow(.86,delta);
    const bool xl=m_settings.value("xLog").toBool(),yl=m_settings.value("yLog").toBool();
    const bool zoomY=ev->modifiers()&Qt::ShiftModifier;
    if(zoomY){
        const double a=qBound(0.0,(chartRect().bottom()-ev->position().y())/chartRect().height(),1.0);
        const double lo=yl?std::log10(ymin):ymin,hi=yl?std::log10(ymax):ymax;
        const double center=lo+(hi-lo)*a;
        const double nlo=center-(center-lo)*factor,nhi=center+(hi-center)*factor;
        ymin=yl?std::pow(10.0,nlo):nlo;ymax=yl?std::pow(10.0,nhi):nhi;
    } else {
        const double a=qBound(0.0,(ev->position().x()-chartRect().left())/chartRect().width(),1.0);
        const double lo=xl?std::log10(xmin):xmin,hi=xl?std::log10(xmax):xmax;
        const double center=lo+(hi-lo)*a;
        const double nlo=center-(center-lo)*factor,nhi=center+(hi-center)*factor;
        xmin=xl?std::pow(10.0,nlo):nlo;xmax=xl?std::pow(10.0,nhi):nhi;
    }
    if(std::isfinite(xmin) && std::isfinite(xmax) && std::isfinite(ymin) &&
       std::isfinite(ymax) && xmin<xmax && ymin<ymax) {
        m_viewX0=xmin;m_viewX1=xmax;m_viewY0=ymin;m_viewY1=ymax;
        m_viewActive=true;m_fastInteraction=true;
        m_renderIdleTimer.start();m_viewNotificationTimer.start();update();
    }
    ev->accept();
}
// Snap to an actual X value present in a visible series, never a generated
// interpolation point. Sorted sampled traces use binary search; unsorted XY
// traces take the (less common) linear path.
bool PanelChartWidget::nearestSampleX(double requested,double& snapped) const {
    if(!std::isfinite(requested))return false;
    bool found=false;
    double best=std::numeric_limits<double>::infinity();
    const bool logX=m_settings.value("xLog").toBool() &&
        m_settings.value("chartType").toString()!=QLatin1String("Digital");
    const double target=logX?std::log10(std::max(requested,std::numeric_limits<double>::min())):requested;
    for(const Series& s:m_series) {
        if(!s.visible)continue;
        if(s.nativeSamples && !s.y.isEmpty()){
            const double pos=(requested-s.nativeStart)/s.nativeStep;
            if(!std::isfinite(pos))continue;
            for(double candidate:{std::floor(pos),std::ceil(pos)}){
                if(candidate<0||candidate>=s.y.size())continue;
                const auto i=qsizetype(candidate);
                const double x=s.nativeStart+double(i)*s.nativeStep;
                const double y=s.y.at(i);
                if(!std::isfinite(y)||(logX&&x<=0))continue;
                const double score=std::abs((logX?std::log10(x):x)-target);
                if(score<best){best=score;snapped=x;found=true;}
            }
            continue;
        }
        const qsizetype n=std::min(s.x.size(),s.y.size());
        const auto accept=[&](qsizetype i) {
            if(i<0 || i>=n)return;
            const double x=s.x.at(i),y=s.y.at(i);
            if(!std::isfinite(x)||!std::isfinite(y)||(logX&&x<=0))return;
            const double score=std::abs((logX?std::log10(x):x)-target);
            if(score<best){best=score;snapped=x;found=true;}
        };
        if(s.xAscending && n>0){
            const auto end=s.x.cbegin()+n;
            const auto it=std::lower_bound(s.x.cbegin(),end,requested);
            const qsizetype mid=it-s.x.cbegin();
            // Examine the closest valid point on each side of the insertion;
            // either side can win after the log transform.
            for(qsizetype i=mid-1;i>=0;--i) {
                const double x=s.x.at(i),y=s.y.at(i);
                if(std::isfinite(x)&&std::isfinite(y)&&(!logX||x>0)) {accept(i);break;}
            }
            for(qsizetype i=mid;i<n;++i) {
                const double x=s.x.at(i),y=s.y.at(i);
                if(std::isfinite(x)&&std::isfinite(y)&&(!logX||x>0)) {accept(i);break;}
            }
        } else {
            for(qsizetype i=0;i<n;++i)accept(i);
        }
    }
    return found;
}
void PanelChartWidget::placeMouseCursor(Qt::KeyboardModifiers modifiers,double x) {
    if(!(modifiers & (Qt::ShiftModifier|Qt::ControlModifier)))return;
    double snapped=0;
    if(!nearestSampleX(x,snapped))return;
    if(modifiers&Qt::ControlModifier){m_cursorB=snapped;m_haveCursorB=true;}
    else {m_cursorA=snapped;m_haveCursorA=true;}
    m_settings.insert("showCursors",true);
    m_inspectorDismissed=false;
    update();
}
QRectF PanelChartWidget::paneCloseRect() const {
    const QRectF p=measurementPaneRect();
    return QRectF(p.right()-28,p.top()+3,24,20);
}
void PanelChartWidget::dismissMeasurements() {
    m_inspectorDismissed=true;
    m_cursorsCleared=true;
    m_haveCursorA=m_haveCursorB=false;
    m_draggingInspector=false;
    update();
}
void PanelChartWidget::keyPressEvent(QKeyEvent* ev) {
    if(ev->key()==Qt::Key_Shift || ev->key()==Qt::Key_Control){
        const Qt::KeyboardModifiers mod=ev->key()==Qt::Key_Control?Qt::ControlModifier:Qt::ShiftModifier;
        const QPointF point=mapFromGlobal(QCursor::pos());
        if(chartRect().contains(point))placeMouseCursor(mod,mouseXValue(point.x()));
        ev->accept();return;
    }
    QWidget::keyPressEvent(ev);
}
void PanelChartWidget::mousePressEvent(QMouseEvent* ev) {
    if(ev->button()==Qt::LeftButton && cursorsEnabled()) {
        if(paneCloseRect().contains(ev->position())) {
            dismissMeasurements();ev->accept();return;
        }
        const QRectF pane=measurementPaneRect();
        if(QRectF(pane.left(),pane.top(),pane.width(),24).contains(ev->position())) {
            m_draggingInspector=true;
            m_paneGrabOffset=ev->position()-pane.topLeft();
            ev->accept();return;
        }
    }
    if(ev->button()==Qt::LeftButton && !(ev->modifiers() & (Qt::ShiftModifier|Qt::ControlModifier))) {
        const QRectF plot=chartRect();
        // Small clickable axis-label zones; toggling only affects labels,
        // not numerical coordinate mappings or the underlying samples.
        if(QRectF(0,plot.top(),plot.left()-3,plot.height()).contains(ev->position())){
            m_settings.insert("showYAxis",!m_settings.value("showYAxis",true).toBool());
            update();ev->accept();return;
        }
        if(QRectF(plot.left(),plot.bottom()+3,plot.width(),height()-plot.bottom()-3).contains(ev->position())){
            m_settings.insert("showXAxis",!m_settings.value("showXAxis",true).toBool());
            update();ev->accept();return;
        }
        if(m_settings.value("showY2").toBool() &&
           QRectF(plot.right()+2,plot.top(),width()-plot.right()-2,plot.height()).contains(ev->position())){
            m_settings.insert("showY2Labels",!m_settings.value("showY2Labels",true).toBool());
            update();ev->accept();return;
        }
    }
    if(ev->button()==Qt::LeftButton && m_settings.value("showLegend").toBool()) {
        for(int i=0;i<m_series.size() && i<8;++i) {
            if(legendRow(i).contains(ev->position())) {
                m_series[i].visible=!m_series[i].visible;
                // Visibility changes never discard a zoomed viewport.
                // Fit-to-series extents remains an explicit double-click.
                update();ev->accept();return;
            }
        }
    }
    if(ev->button()==Qt::LeftButton && chartRect().contains(ev->position()) &&
        (ev->modifiers() & (Qt::ShiftModifier|Qt::ControlModifier))) {
        placeMouseCursor(ev->modifiers(),mouseXValue(ev->position().x()));
        ev->accept();return;
    }
    if(ev->button()==Qt::LeftButton && chartRect().contains(ev->position())){
        m_panning=true;m_last=ev->position();ev->accept();
    }
}
void PanelChartWidget::mouseMoveEvent(QMouseEvent* ev) {
    if(m_draggingInspector && (ev->buttons()&Qt::LeftButton)) {
        const QRectF bounds=rect().adjusted(3,3,-3,-3);
        const QRectF pane=measurementPaneRect();
        const QPointF desired=ev->position()-m_paneGrabOffset;
        m_paneTopLeft=QPointF(qBound(bounds.left(),desired.x(),std::max(bounds.left(),bounds.right()-pane.width())),
                             qBound(bounds.top(),desired.y(),std::max(bounds.top(),bounds.bottom()-pane.height())));
        m_paneMoved=true;update();ev->accept();return;
    }
    // Cursor updates require the corresponding modifier; ordinary hover never
    // changes an established measurement. Ctrl wins if both are held.
    if(chartRect().contains(ev->position()) &&
       (ev->modifiers() & (Qt::ShiftModifier|Qt::ControlModifier)) &&
       !m_panning && !(ev->buttons() & Qt::LeftButton)) {
        placeMouseCursor(ev->modifiers(),mouseXValue(ev->position().x()));
        ev->accept();return;
    }
    if(!m_panning || !(ev->buttons()&Qt::LeftButton))return;
    double xmin,xmax,ymin,ymax;ranges(xmin,xmax,ymin,ymax);
    const QPointF delta=ev->position()-m_last;m_last=ev->position();
    const bool xl=m_settings.value("xLog").toBool(),yl=m_settings.value("yLog").toBool();
    const double lx=xl?std::log10(xmin):xmin,hx=xl?std::log10(xmax):xmax;
    const double ly=yl?std::log10(ymin):ymin,hy=yl?std::log10(ymax):ymax;
    const double dx=-delta.x()*(hx-lx)/qMax(1.0,chartRect().width());
    const double dy=delta.y()*(hy-ly)/qMax(1.0,chartRect().height());
    m_viewX0=xl?std::pow(10.0,lx+dx):xmin+dx;
    m_viewX1=xl?std::pow(10.0,hx+dx):xmax+dx;
    m_viewY0=yl?std::pow(10.0,ly+dy):ymin+dy;
    m_viewY1=yl?std::pow(10.0,hy+dy):ymax+dy;
    m_viewActive=true;m_fastInteraction=true;
    m_renderIdleTimer.start();m_viewNotificationTimer.start();update();ev->accept();
}
void PanelChartWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if(ev->button()==Qt::LeftButton){
        if(m_panning){
            // Deliver the actual final viewport once even when the operator
            // releases the mouse before the coalescing timer expires.
            m_viewNotificationTimer.stop();publishView();
            m_renderIdleTimer.stop();m_fastInteraction=false;update();
        }
        m_panning=false;m_draggingInspector=false;
    }
}
void PanelChartWidget::mouseDoubleClickEvent(QMouseEvent* ev) {
    resetView();ev->accept();
}

QRectF PanelChartWidget::legendRow(int index) const {
    const QRectF plot=chartRect();
    const double left=std::max(plot.left()+2,plot.right()-145);
    return QRectF(left,plot.top()+7+index*19,143,18);
}

void PanelChartWidget::secondaryRanges(double& ymin,double& ymax) const {
    bool any=false;
    ymin=0;ymax=1;
    for(const Series& s:m_series) {
        if(!s.visible || s.axis!=QLatin1String("right"))continue;
        if(s.nativeSamples){
            if(s.nativeIndex.valid()){
                if(!any){ymin=s.nativeIndex.minimum();ymax=s.nativeIndex.maximum();any=true;}
                else {ymin=std::min(ymin,s.nativeIndex.minimum());ymax=std::max(ymax,s.nativeIndex.maximum());}
            }
            continue;
        }
        double low=0,high=0;
        if(s.legacyIndex.rightRange(low,high)){
            if(!any){ymin=low;ymax=high;any=true;}
            else {ymin=std::min(ymin,low);ymax=std::max(ymax,high);}
        }
    }
    if(!any){ymin=0;ymax=1;}
    if(ymin>=ymax){const double pad=std::max(0.5,std::abs(ymin)*0.1);ymin-=pad;ymax+=pad;}
    if(m_settings.value("y2Auto",true).toBool()) {
        const double pad=(ymax-ymin)*0.08;ymin-=pad;ymax+=pad;
    } else {
        const double a=m_settings.value("y2Min").toDouble();
        const double b=m_settings.value("y2Max").toDouble();
        if(std::isfinite(a) && std::isfinite(b) && b>a){ymin=a;ymax=b;}
    }
}

void PanelChartWidget::drawDigital(QPainter& painter,const QRectF& plot,bool light) {
    QElapsedTimer frameClock;frameClock.start();
    m_lastDigitalQueryMs=0;m_lastDigitalGeometryMs=0;m_lastDigitalPaintMs=0;
    const QColor grid=light?QColor("#dce6ef"):QColor("#304458");
    const QColor fg=light?QColor("#23374a"):QColor("#e4eef8");
    const QColor muted=light?QColor("#637a91"):QColor("#94b2ca");
    // A dedicated lane chart uses binary values (>=0.5 = HIGH). Each named
    // series owns a lane; names are pinned to the left, not the legend.
    QVector<const Series*> lanes;
    for(const Series& s:m_series)if(s.visible)lanes.push_back(&s);
    if(lanes.isEmpty()) {
        painter.setPen(muted);
        painter.drawText(plot,Qt::AlignCenter,QStringLiteral("No digital lanes — plots.setSeries()"));
        return;
    }
    double xmin=std::numeric_limits<double>::infinity();
    double xmax=-std::numeric_limits<double>::infinity();
    for(const Series* s:lanes){
        if(s->digitalIndex.valid()){
            xmin=std::min(xmin,s->digitalIndex.firstX());
            xmax=std::max(xmax,s->digitalIndex.lastX());
        } else {
            // Unsupported unsorted/invalid digital series keep the old path.
            for(double x:s->x)if(std::isfinite(x)){
                xmin=std::min(xmin,x);xmax=std::max(xmax,x);
            }
        }
    }
    if(!std::isfinite(xmin)||xmin>=xmax){xmin=0;xmax=1;}
    if(!m_settings.value("xAuto",true).toBool()) {
        const double a=m_settings.value("xMin").toDouble(),b=m_settings.value("xMax").toDouble();
        if(std::isfinite(a)&&std::isfinite(b)&&a<b){xmin=a;xmax=b;}
    }
    if(m_viewActive && m_viewX0<m_viewX1){xmin=m_viewX0;xmax=m_viewX1;}
    const auto px=[&](double x){return plot.left()+(x-xmin)/(xmax-xmin)*plot.width();};
    const int count=std::min(int(lanes.size()),16);
    const double h=plot.height()/count;
    const QVector<double> xticks=ticks(xmin,xmax,false,8);
    const QVector<QString> xlabels=PlotAxisLabels::linearX(xticks);
    double previousLabelX=-1e6;
    for(int i=0;i<xticks.size();++i){
        const double x=xticks[i];
        const double xx=px(x);
        if(m_settings.value("showGrid").toBool()){
            painter.setPen(QPen(grid,1));painter.drawLine(QPointF(xx,plot.top()),QPointF(xx,plot.bottom()));
        }
        if(m_settings.value("showXAxis",true).toBool() &&
           (xx-previousLabelX>=70 || previousLabelX<-1e5)){
            painter.setPen(muted);
            painter.drawText(QRectF(xx-35,plot.bottom()+4,70,18),Qt::AlignCenter,xlabels[i]);
            previousLabelX=xx;
        }
    }
    painter.save();painter.setClipRect(plot);
    m_lastDigitalPrimitives=0;
    for(int n=0;n<count;++n){
        const Series& s=*lanes[n];
        const double top=plot.top()+h*n;
        const double high=top+h*.23,low=top+h*.77;
        painter.setPen(QPen(grid,1));
        painter.drawLine(QPointF(plot.left(),top+h),QPointF(plot.right(),top+h));
        painter.setPen(QPen(s.color,std::max(1.3,s.width)));
        if(s.digitalIndex.valid()){
            DigitalGeometry& geometry=s.digitalGeometry;
            // During wheel/drag interaction we can transform an existing
            // raster instead of rebuilding transitions and painting thousands
            // of Qt primitives for every mouse event. Exact data is restored
            // after the 85 ms idle timer, or immediately on a programmatic view.
            const bool canPreview=m_fastInteraction && geometry.valid &&
                !geometry.raster.isNull() && geometry.plot==plot &&
                geometry.hi==high && geometry.lo==low &&
                std::isfinite(xmin) && std::isfinite(xmax) && xmin<xmax &&
                geometry.xmin<geometry.xmax &&
                (geometry.xmax-geometry.xmin)/(xmax-xmin)>=0.20 &&
                (geometry.xmax-geometry.xmin)/(xmax-xmin)<=5.0 &&
                std::min(xmax,geometry.xmax)>std::max(xmin,geometry.xmin);
            if(!canPreview && (!geometry.valid || geometry.xmin!=xmin ||
               geometry.xmax!=xmax || geometry.hi!=high ||
               geometry.lo!=low || geometry.plot!=plot ||
               geometry.preview!=m_fastInteraction)){
                QElapsedTimer buildClock;buildClock.start();
                geometry=DigitalGeometry{};
                geometry.valid=true;geometry.xmin=xmin;geometry.xmax=xmax;
                geometry.hi=high;geometry.lo=low;geometry.plot=plot;
                geometry.preview=m_fastInteraction;
                ++m_digitalGeometryBuilds;
                const std::size_t pixels=std::size_t(std::max(1.0,std::ceil(plot.width())));
                QElapsedTimer queryClock;queryClock.start();
                const std::size_t visibleEdges=s.digitalIndex.transitionsIn(xmin,xmax);
                m_lastDigitalQueryMs+=double(queryClock.nsecsElapsed())*1.e-6;
                geometry.dense=visibleEdges>pixels*2;
                if(geometry.dense){
                    // Dense views have a bounded number of column strokes.
                    // This intermediate representation preserves both levels
                    // when even a one-sample pulse falls inside one pixel.
                    const std::size_t columns=m_fastInteraction?
                        std::max(std::size_t(1),pixels/2):pixels;
                    queryClock.restart();
                    const auto bins=s.digitalIndex.columns(xmin,xmax,columns);
                    m_lastDigitalQueryMs+=double(queryClock.nsecsElapsed())*1.e-6;
                    geometry.lines.reserve(qsizetype(bins.size()*2));
                    for(const DigitalPixelColumn& bin:bins){
                        const double x0=px(bin.x0),x1=px(bin.x1);
                        const double mid=(x0+x1)*.5;
                        if(bin.low && bin.high)
                            geometry.lines.append(QLineF(mid,high,mid,low));
                        else if(bin.high || bin.low){
                            const double y=bin.high?high:low;
                            geometry.lines.append(QLineF(x0,y,x1,y));
                        }
                    }
                    geometry.primitives=geometry.lines.size();
                } else {
                    const double begin=std::max(xmin,s.digitalIndex.firstX());
                    const double finish=std::min(xmax,s.digitalIndex.lastX());
                    if(begin<=finish){
                        bool state=s.digitalIndex.stateAt(begin);
                        const auto& edges=s.digitalIndex.transitions();
                        auto it=std::upper_bound(edges.begin(),edges.end(),begin,
                            [](double xx,const DigitalTransition& e){return xx<e.x;});
                        geometry.path.moveTo(px(begin),state?high:low);
                        for(;it!=edges.end() && it->x<=finish;++it){
                            const double xx=px(it->x);
                            geometry.path.lineTo(xx,state?high:low);
                            geometry.path.lineTo(xx,it->high?high:low);
                            state=it->high;geometry.primitives+=2;
                        }
                        geometry.path.lineTo(px(finish),state?high:low);
                        ++geometry.primitives;
                    }
                }
                // Rasterize just the digital waveform layer, not the grid,
                // labels or measurement cursors; those stay at their exact
                // viewport position during every pan and zoom.
                const double dpr=std::max(1.0,devicePixelRatioF());
                const int rw=std::max(1,int(std::ceil(plot.width()*dpr)));
                const int rh=std::max(1,int(std::ceil(h*dpr)));
                geometry.raster=QImage(rw,rh,QImage::Format_ARGB32_Premultiplied);
                geometry.raster.fill(Qt::transparent);
                {
                    QPainter rasterPainter(&geometry.raster);
                    rasterPainter.setRenderHint(QPainter::Antialiasing,!geometry.dense);
                    rasterPainter.scale(dpr,dpr);
                    rasterPainter.translate(-plot.left(),-top);
                    rasterPainter.setPen(QPen(s.color,std::max(1.3,s.width)));
                    if(geometry.dense && !geometry.lines.isEmpty())
                        rasterPainter.drawLines(geometry.lines.constData(),int(geometry.lines.size()));
                    else if(!geometry.dense)rasterPainter.drawPath(geometry.path);
                }
                ++m_digitalRasterBuilds;
                m_lastDigitalGeometryMs+=double(buildClock.nsecsElapsed())*1.e-6;
            } else ++m_digitalCacheHits;
            m_lastDigitalPrimitives+=geometry.primitives;
            QElapsedTimer blitClock;blitClock.start();
            if(!geometry.raster.isNull()){
                double scale=1.0,shift=0.0;
                if(canPreview){
                    scale=(geometry.xmax-geometry.xmin)/(xmax-xmin);
                    shift=(geometry.xmin-xmin)*plot.width()/(xmax-xmin);
                    ++m_digitalPreviewBlits;
                }
                const QRectF target(plot.left()+shift,top,plot.width()*scale,h);
                painter.drawImage(target,geometry.raster,
                    QRectF(0,0,geometry.raster.width(),geometry.raster.height()));
            }
            m_lastDigitalPaintMs+=double(blitClock.nsecsElapsed())*1.e-6;
        }else{
            // Unsorted/NaN input retains legacy segment and gap behaviour.
            QPointF previous;bool have=false;
            for(qsizetype i=0;i<std::min(s.x.size(),s.y.size());++i){
                if(!std::isfinite(s.x[i])||!std::isfinite(s.y[i])){have=false;continue;}
                const QPointF pt(px(s.x[i]),s.y[i]>=0.5?high:low);
                if(have){painter.drawLine(previous,QPointF(pt.x(),previous.y()));
                         painter.drawLine(QPointF(pt.x(),previous.y()),pt);}
                previous=pt;have=true;
            }
        }
    }
    painter.restore();
    for(int n=0;n<count;++n){
        const Series& s=*lanes[n];
        const double top=plot.top()+h*n;
        painter.setPen(s.color);
        painter.drawText(QRectF(3,top,plot.left()-7,h),Qt::AlignRight|Qt::AlignVCenter,s.name);
    }
    painter.setPen(fg);
    if(m_settings.value("showXAxis",true).toBool())
    painter.drawText(QRectF(plot.left(),height()-25,plot.width(),19),Qt::AlignCenter,
                     m_settings.value("xLabel").toString());
    if(cursorsEnabled()){
        const double a=m_haveCursorA?m_cursorA:(m_cursorsCleared?std::numeric_limits<double>::quiet_NaN():m_settings.value("cursorA").toDouble());
        const double b=m_haveCursorB?m_cursorB:(m_cursorsCleared?std::numeric_limits<double>::quiet_NaN():m_settings.value("cursorB").toDouble());
        painter.save();painter.setClipRect(plot);
        double dataLo=0,dataHi=0;
        if(sampleDomain(dataLo,dataHi)) {
            painter.setPen(QPen(QColor("#f3c34e"),1,Qt::DashLine));
            if(std::isfinite(a) && a>=dataLo && a<=dataHi)
                painter.drawLine(QPointF(px(a),plot.top()),QPointF(px(a),plot.bottom()));
            painter.setPen(QPen(QColor("#49d2a1"),1,Qt::DashLine));
            if(std::isfinite(b) && b>=dataLo && b<=dataHi)
                painter.drawLine(QPointF(px(b),plot.top()),QPointF(px(b),plot.bottom()));
        }
        painter.restore();
        // Detailed measurements are in the movable inspection overlay.
    }
    m_lastDigitalFrameMs=double(frameClock.nsecsElapsed())*1.e-6;
    m_maxDigitalFrameMs=std::max(m_maxDigitalFrameMs,m_lastDigitalFrameMs);
    if(m_settings.value("digitalPerfOverlay",false).toBool()){
        const QRectF hud(plot.right()-355,plot.top()+7,348,48);
        painter.save();painter.setPen(Qt::NoPen);
        painter.setBrush(light?QColor(250,252,255,225):QColor(9,17,29,225));
        painter.drawRoundedRect(hud,6,6);
        QFont info=painter.font();info.setPixelSize(11);painter.setFont(info);
        painter.setPen(fg);
        painter.drawText(hud.adjusted(6,2,-6,-2),Qt::AlignLeft|Qt::AlignVCenter,
           QStringLiteral("Frame %1 ms | Build %2 ms | Blit %3 ms\nQuery %4 ms | Raster %5 | Preview %6")
             .arg(m_lastDigitalFrameMs,0,'f',2)
             .arg(m_lastDigitalGeometryMs,0,'f',2)
             .arg(m_lastDigitalPaintMs,0,'f',2)
             .arg(m_lastDigitalQueryMs,0,'f',2)
             .arg(m_digitalRasterBuilds)
             .arg(m_digitalPreviewBlits));
        painter.restore();
    }

}

// Measurements are calculated directly from the series arrays, never from
// interpolated screen pixels or extrapolated values outside sample coverage.
bool PanelChartWidget::sampleDomain(double& lo,double& hi) const {
    bool found=false;
    const bool logX=m_settings.value("xLog").toBool();
    for(const Series& s:m_series) {
        if(!s.visible)continue;
        if(s.nativeSamples){
            if(s.nativeIndex.valid()&&!s.y.isEmpty()){
                const double a=s.nativeStart,b=a+double(s.y.size()-1)*s.nativeStep;
                if(!logX||b>0){
                    if(!found){lo=a;hi=b;found=true;}
                    else {lo=std::min(lo,a);hi=std::max(hi,b);}
                }
            }
            continue;
        }
        const LegacyXYExtent& bounds=s.legacyIndex.range(logX,false);
        if(!bounds.valid)continue;
        if(!found){lo=bounds.xmin;hi=bounds.xmax;found=true;}
        else {lo=std::min(lo,bounds.xmin);hi=std::max(hi,bounds.xmax);}
    }
    return found;
}

double PanelChartWidget::mouseXValue(double px) const {
    double xmin,xmax,ymin,ymax;ranges(xmin,xmax,ymin,ymax);
    if(m_settings.value("chartType").toString()==QLatin1String("Digital")){
        double low=0,high=0;
        if(sampleDomain(low,high) && low<high){xmin=low;xmax=high;}
        if(!m_settings.value("xAuto",true).toBool()){
            const double a=m_settings.value("xMin").toDouble(),b=m_settings.value("xMax").toDouble();
            if(std::isfinite(a)&&std::isfinite(b)&&a<b){xmin=a;xmax=b;}
        }
        if(m_viewActive && m_viewX0<m_viewX1){xmin=m_viewX0;xmax=m_viewX1;}
    }
    const double t=qBound(0.0,(px-chartRect().left())/std::max(1.0,chartRect().width()),1.0);
    const bool log=m_settings.value("xLog").toBool() && xmin>0 && xmax>0 &&
                   m_settings.value("chartType").toString()!=QLatin1String("Digital");
    return log?std::pow(10.0,std::log10(xmin)+t*(std::log10(xmax)-std::log10(xmin)))
              :xmin+t*(xmax-xmin);
}

bool PanelChartWidget::sampleAt(const Series& s,double x,bool digital,double& y,bool exactOnly) {
    if(s.nativeSamples){
        if(s.y.isEmpty()||!std::isfinite(x))return false;
        const double p=(x-s.nativeStart)/s.nativeStep;
        if(!std::isfinite(p)||p<0||p>double(s.y.size()-1))return false;
        const double nearest=std::round(p);
        if(exactOnly && std::abs(p-nearest)>1e-6)return false;
        const qsizetype a=qsizetype(std::floor(p)),b=qsizetype(std::ceil(p));
        const double t=p-double(a);
        y=s.y.at(a)*(1-t)+s.y.at(b)*t;
        if(digital)y=y>=.5?1.0:0.0;
        return std::isfinite(y);
    }
    const qsizetype n=std::min(s.x.size(),s.y.size());
    if(n==0 || !std::isfinite(x))return false;
    // Mouse-snapped cursors report only measured sample values. Explicit
    // programmatic cursor positions retain the previous interpolating API.
    if(exactOnly) {
        for(qsizetype i=0;i<n;++i)if(s.x.at(i)==x && std::isfinite(s.y.at(i))) {
            qsizetype last=i;
            while(last+1<n && s.x[last+1]==x && std::isfinite(s.y[last+1]))++last;
            y=digital?(s.y[last]>=0.5?1.0:0.0):s.y[last];return true;
        }
        return false;
    }
    // Step traces use the previous state up to the next transition. Analog
    // traces interpolate only between adjacent valid points in their domain.
    for(qsizetype i=0;i<n;++i) {
        const double xi=s.x.at(i),yi=s.y.at(i);
        if(!std::isfinite(xi)||!std::isfinite(yi))continue;
        if(x==xi) {
            // At coincident step timestamps, show the most recent state.
            qsizetype last=i;
            while(last+1<n && s.x[last+1]==x && std::isfinite(s.y[last+1]))++last;
            y=digital?(s.y[last]>=0.5?1.0:0.0):s.y[last];return true;
        }
        if(i==0)continue;
        const double xa=s.x.at(i-1),ya=s.y.at(i-1);
        if(!std::isfinite(xa)||!std::isfinite(ya)||xa==xi)continue;
        if(x>=std::min(xa,xi) && x<=std::max(xa,xi)) {
            y=digital?(ya>=0.5?1.0:0.0):ya+(yi-ya)*(x-xa)/(xi-xa);
            return true;
        }
    }
    return false;
}

QVariantMap PanelChartWidget::cursorMeasurements() const {
    QVariantMap out;
    const bool digital=m_settings.value("chartType").toString()==QLatin1String("Digital");
    double lo=0,hi=0;
    if(!sampleDomain(lo,hi)){out.insert("hasData",false);return out;}
    out.insert("hasData",true);
    out.insert("dataMin",lo);out.insert("dataMax",hi);
    const double a=m_haveCursorA?m_cursorA:(m_cursorsCleared?std::numeric_limits<double>::quiet_NaN():m_settings.value("cursorA").toDouble());
    const double b=m_haveCursorB?m_cursorB:(m_cursorsCleared?std::numeric_limits<double>::quiet_NaN():m_settings.value("cursorB").toDouble());
    const bool va=std::isfinite(a) && a>=lo && a<=hi;
    const bool vb=std::isfinite(b) && b>=lo && b<=hi;
    if(va)out.insert("cursorA",a);
    if(vb)out.insert("cursorB",b);
    if(va && vb)out.insert("deltaX",b-a);
    QVariantList rows;
    for(const Series& s:m_series) {
        if(!s.visible)continue;
        QVariantMap r{{"name",s.name},{"axis",s.axis}};
        double ya=0,yb=0;
        if(va && sampleAt(s,a,digital,ya,m_haveCursorA))r.insert("a",ya);
        if(vb && sampleAt(s,b,digital,yb,m_haveCursorB))r.insert("b",yb);
        if(r.contains("a")&&r.contains("b"))r.insert("deltaY",yb-ya);
        if(digital) {
            // Calculate positive-pulse metrics from actual binary transitions.
            // Only complete rise->fall pulses are measurable; no fabricated
            // duration for a capture ending in the HIGH state.
            QVector<double> rises,falls;
            const qsizetype n=std::min(s.x.size(),s.y.size());
            for(qsizetype i=1;i<n;++i){
                const double x0=s.x[i-1],x1=s.x[i],v0=s.y[i-1],v1=s.y[i];
                if(!std::isfinite(x0)||!std::isfinite(x1)||!std::isfinite(v0)||!std::isfinite(v1)||x1<=x0)continue;
                if(v0<0.5 && v1>=0.5)rises.push_back(x1);
                if(v0>=0.5 && v1<0.5)falls.push_back(x1);
            }
            if(rises.size()>=2){
                const double period=rises[1]-rises[0];
                if(period>0){
                    r.insert("period",period);r.insert("frequency",1.0/period);
                    for(double fall:falls)if(fall>rises[0] && fall<=rises[1]){
                        const double pulse=fall-rises[0];
                        r.insert("highWidth",pulse);r.insert("dutyPercent",100.0*pulse/period);break;
                    }
                }
            } else if(!rises.isEmpty()) {
                for(double fall:falls)if(fall>rises[0]){r.insert("highWidth",fall-rises[0]);break;}
            }
        }
        rows.push_back(r);
    }
    out.insert("series",rows);
    return out;
}

QRectF PanelChartWidget::measurementPaneRect() const {
    const QVariantMap m=cursorMeasurements();
    const QVariantList series=m.value("series").toList();
    const int rows=std::min(int(series.size()),5);
    const bool digital=m_settings.value("chartType").toString()==QLatin1String("Digital");
    const double w=std::min(300.0,std::max(185.0,width()*.48));
    const double h=66+rows*(digital?30:19);
    QPointF pos=m_paneMoved?m_paneTopLeft:QPointF(12,47);
    pos.setX(qBound(3.0,pos.x(),std::max(3.0,double(width())-w-3)));
    pos.setY(qBound(3.0,pos.y(),std::max(3.0,double(height())-h-3)));
    return QRectF(pos,QSizeF(w,h));
}

void PanelChartWidget::drawMeasurementPane(QPainter& painter,const QRectF&,bool light,bool digital) {
    const QRectF pane=measurementPaneRect();
    const QColor bg=light?QColor(250,253,255,244):QColor(12,24,36,244);
    const QColor fg=light?QColor("#183046"):QColor("#e5f2fc");
    const QColor muted=light?QColor("#526579"):QColor("#9bb5c9");
    painter.save();
    painter.setBrush(bg);painter.setPen(QPen(light?QColor("#9ab2c8"):QColor("#58748c"),1));
    painter.drawRoundedRect(pane,6,6);
    painter.setPen(fg);
    QFont f=painter.font();f.setPixelSize(11);f.setWeight(QFont::DemiBold);painter.setFont(f);
    painter.drawText(pane.adjusted(9,4,-34,-pane.height()+23),Qt::AlignVCenter,
                     QStringLiteral("MEASUREMENTS  ·  drag here"));
    const QRectF close=paneCloseRect();
    painter.setPen(QPen(muted,1.6));
    painter.drawLine(close.adjusted(6,6,-6,-6).topLeft(),close.adjusted(6,6,-6,-6).bottomRight());
    painter.drawLine(close.adjusted(6,6,-6,-6).bottomLeft(),close.adjusted(6,6,-6,-6).topRight());
    painter.setPen(fg);
    f.setWeight(QFont::Normal);f.setPixelSize(10);painter.setFont(f);
    const QVariantMap m=cursorMeasurements();
    if(!m.value("hasData").toBool()) {
        painter.drawText(pane.adjusted(9,27,-9,-5),Qt::AlignTop,QStringLiteral("No valid samples"));
        painter.restore();return;
    }
    const auto val=[&](const QString& key){return m.contains(key)?axisNumber(m.value(key).toDouble()):QStringLiteral("—");};
    painter.setPen(muted);
    painter.drawText(QRectF(pane.left()+9,pane.top()+25,pane.width()-18,16),Qt::AlignLeft|Qt::AlignVCenter,
                     QStringLiteral("A %1    B %2    ΔX %3").arg(val("cursorA"),val("cursorB"),val("deltaX")));
    const QVariantList series=m.value("series").toList();
    int i=0;
    for(const QVariant& v:series){
        if(i>=5)break;
        const QVariantMap row=v.toMap();
        const auto rnum=[&](const QString& k){return row.contains(k)?axisNumber(row.value(k).toDouble()):QStringLiteral("—");};
        painter.setPen(fg);
        const double yy=pane.top()+45+i*(digital?30:19);
        const QString label=row.value("name").toString().left(18);
        painter.drawText(QRectF(pane.left()+9,yy,pane.width()-18,15),Qt::AlignLeft|Qt::AlignVCenter,
                         QStringLiteral("%1   A:%2   B:%3   Δ:%4")
                           .arg(label,rnum("a"),rnum("b"),rnum("deltaY")));
        if(digital){
            painter.setPen(muted);
            // Frequency is reciprocal of the series X units (Hz when X is seconds).
            const QString metrics=QStringLiteral("Width %1  T %2  f %3  %4%")
                .arg(rnum("highWidth"),rnum("period"),rnum("frequency"),rnum("dutyPercent"));
            painter.drawText(QRectF(pane.left()+9,yy+15,pane.width()-18,14),Qt::AlignLeft|Qt::AlignVCenter,metrics);
        }
        ++i;
    }
    painter.restore();
}
