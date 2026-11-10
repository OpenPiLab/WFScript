#include "WaveformPlot.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QFontMetrics>
#include <QPen>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const QColor bg(14, 23, 35);
const QColor grid(42, 58, 75);
const QColor labelColor(164, 184, 204);
const QColor trace1(245, 211, 61);
const QColor trace2(75, 207, 234);
const QColor cursorAColor(115, 245, 170);
const QColor cursorBColor(250, 132, 183);
QString formatTime(double seconds) {
    if (std::abs(seconds) >= 1) return QStringLiteral("%1 s").arg(seconds,0,'f',3);
    if (std::abs(seconds) >= 1e-3) return QStringLiteral("%1 ms").arg(seconds*1e3,0,'f',3);
    return QStringLiteral("%1 us").arg(seconds*1e6,0,'f',2);
}
}

WaveformPlot::WaveformPlot(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(260);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(QStringLiteral("Left click: cursor A | Right click: cursor B | Left drag: pan | Wheel: zoom | Double-click: fit"));
}

void WaveformPlot::setFrame(const ScopeFrame& frame) {
    m_rawFrame = frame;
    regenerateDisplayFrame();
}
void WaveformPlot::setSoftwareAlignmentEnabled(bool enabled) {
    if (m_softwareAlignment == enabled) return;
    m_softwareAlignment = enabled;
    regenerateDisplayFrame();
}
void WaveformPlot::regenerateDisplayFrame() {
    m_frame = m_rawFrame;  // raw measurement/CSV buffers are never modified
    if (m_softwareAlignment && m_frame.hasMatchingEdge &&
        std::isfinite(m_frame.nearestEdgeOffsetSamples)) {
        const double shift = m_frame.nearestEdgeOffsetSamples;
        const auto shiftChannel = [shift](QVector<double>& samples) {
            const QVector<double> original = samples;
            const int count = static_cast<int>(original.size());
            for (int i=0;i<count;++i) {
                const double index = i + shift;
                const int lo = static_cast<int>(std::floor(index));
                if (lo < 0 || lo+1 >= count) {
                    samples[i] = std::numeric_limits<double>::quiet_NaN();
                } else {
                    const double frac = index-lo;
                    samples[i] = original[lo]*(1.0-frac) + original[lo+1]*frac;
                }
            }
        };
        shiftChannel(m_frame.ch1);
        shiftChannel(m_frame.ch2);
    }
    update();
    publishCursors();
}
void WaveformPlot::setChannelsVisible(bool ch1, bool ch2) {
    m_show1=ch1; m_show2=ch2; update();
}
void WaveformPlot::setVoltageScales(bool automatic, double ch1VoltsPerDiv, double ch2VoltsPerDiv,
                                    double ch1CenterV, double ch2CenterV) {
    m_autoScale = automatic;
    m_voltsPerDiv[0] = std::max(0.001, ch1VoltsPerDiv);
    m_voltsPerDiv[1] = std::max(0.001, ch2VoltsPerDiv);
    m_centerVolts[0] = ch1CenterV;
    m_centerVolts[1] = ch2CenterV;
    update();
}
void WaveformPlot::resetView() { m_start=0; m_span=1; update(); publishCursors(); }
QRectF WaveformPlot::plotBounds() const {
    return QRectF(rect()).adjusted(69, 26, -70, -43);
}
double WaveformPlot::fractionAt(double x) const {
    const QRectF p=plotBounds();
    if (p.width()<1) return m_start;
    return qBound(0.0,m_start+(x-p.left())/p.width()*m_span,1.0);
}
double WaveformPlot::sampleAt(const QVector<double>& data, double fraction) const {
    if(data.isEmpty()) return 0;
    const int index=qBound(0,int(std::round(fraction*(data.size()-1))),static_cast<int>(data.size()-1));
    return data.at(index);
}
void WaveformPlot::publishCursors() {
    if (!hasData() || m_frame.sampleRateHz<=0) {
        emit cursorChanged(QStringLiteral("A/B cursors: no data")); return;
    }
    const double duration=double(m_frame.ch1.size()-1)/m_frame.sampleRateHz;
    const double dTime=(m_cursorB-m_cursorA)*duration;
    const double a=sampleAt(m_frame.ch1,m_cursorA);
    const double b=sampleAt(m_frame.ch1,m_cursorB);
    emit cursorChanged(QStringLiteral("Cursors (CH1)   A: %1, %2 V    B: %3, %4 V    Δt: %5    ΔV: %6 V")
        .arg(formatTime(m_cursorA*duration))
        .arg(a,0,'f',4)
        .arg(formatTime(m_cursorB*duration))
        .arg(b,0,'f',4)
        .arg(formatTime(dTime))
        .arg(b-a,0,'f',4));
}
void WaveformPlot::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), bg);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QRectF p=plotBounds();
    if (p.width()<30 || p.height()<20) return;
    painter.setPen(QPen(grid,1));
    for(int x=0;x<=10;++x) {
        const double px=p.left()+p.width()*x/10.0;
        painter.drawLine(QPointF(px,p.top()),QPointF(px,p.bottom()));
    }
    for(int y=0;y<=8;++y) {
        const double py=p.top()+p.height()*y/8.0;
        painter.drawLine(QPointF(p.left(),py),QPointF(p.right(),py));
    }
    painter.setPen(labelColor);
    painter.drawText(QRectF(p.left(),1,p.width(),20),Qt::AlignCenter,
                     QStringLiteral("Oscilloscope  |  %1 MS/s  |  %2 samples  |  frame %3%4")
                         .arg(m_frame.sampleRateHz/1e6,0,'f',3)
                         .arg(m_frame.ch1.size()).arg(m_frame.sequence)
                         .arg(m_frame.simulated ? QStringLiteral("  [SIMULATED]") :
                              (m_softwareAlignment ? QStringLiteral("  [DISPLAY ALIGNED ONLY]") : QString())));
    if (!hasData() || m_frame.sampleRateHz <= 0) {
        painter.drawText(p,Qt::AlignCenter,QStringLiteral("No captured samples yet — click Single or Start Live"));
        return;
    }

    const int n = static_cast<int>(m_frame.ch1.size());
    double automaticLow = 0, automaticHigh = 0;
    bool first = true;
    const auto findLimits = [&](const QVector<double>& data) {
        for (double v : data) {
            if (!std::isfinite(v)) continue;
            if (first) { automaticLow=automaticHigh=v; first=false; }
            automaticLow=std::min(automaticLow,v);
            automaticHigh=std::max(automaticHigh,v);
        }
    };
    if (m_autoScale) {
        if (m_show1) findLimits(m_frame.ch1);
        if (m_show2) findLimits(m_frame.ch2);
        if (first) { automaticLow=-1; automaticHigh=1; }
        const double center = (automaticLow+automaticHigh)/2.0;
        const double span = std::max(0.10,(automaticHigh-automaticLow)*1.20);
        automaticLow=center-span/2.0;
        automaticHigh=center+span/2.0;
    }
    const auto voltsAtGrid = [&](int channel, int y) {
        if (m_autoScale)
            return automaticHigh - (automaticHigh-automaticLow)*double(y)/8.0;
        return m_centerVolts[channel] + (4.0 - y)*m_voltsPerDiv[channel];
    };
    const auto yPixel = [&](int channel, double volts) {
        if (m_autoScale)
            return p.bottom()-(volts-automaticLow)/(automaticHigh-automaticLow)*p.height();
        return p.top() + (4.0 - (volts - m_centerVolts[channel]) / m_voltsPerDiv[channel])
            * p.height() / 8.0;
    };
    for(int y=0;y<=8;++y) {
        const double py=p.top()+p.height()*y/8.0;
        if (m_show1) {
            painter.setPen(trace1);
            painter.drawText(QRectF(1,py-10,63,20),Qt::AlignRight|Qt::AlignVCenter,
                             QString::number(voltsAtGrid(0,y),'f',2));
        }
        if (m_show2 && !m_autoScale) {
            painter.setPen(trace2);
            painter.drawText(QRectF(p.right()+6,py-10,63,20),Qt::AlignLeft|Qt::AlignVCenter,
                             QString::number(voltsAtGrid(1,y),'f',2));
        }
    }
    const double captureSeconds=double(n-1)/m_frame.sampleRateHz;
    for(int x=0;x<=10;++x) {
        const double t=(m_start+m_span*x/10.0)*captureSeconds;
        painter.drawText(QRectF(p.left()+p.width()*x/10-36,p.bottom()+7,72,22),Qt::AlignHCenter,
                         formatTime(t));
    }
    painter.save();
    painter.setClipRect(p.adjusted(-1,-1,1,1));
    const auto drawChannel=[&](const QVector<double>& data, const QColor& color, int channel) {
        if(data.isEmpty())return;
        painter.setPen(QPen(color,1.3));
        const int firstIndex=qBound(0,int(std::floor(m_start*(data.size()-1))),static_cast<int>(data.size()-1));
        const int lastIndex=qBound(firstIndex+1,int(std::ceil((m_start+m_span)*(data.size()-1)))+1,static_cast<int>(data.size()));
        const int count=lastIndex-firstIndex;
        if(count<=0) return;
        // Switch to min/max envelope when many points map to one pixel.
        // Unlike simple decimation, this keeps narrow voltage spikes visible.
        if(count<=p.width()*1.5) {
            QPolygonF poly;
            poly.reserve(count);
            for(int i=firstIndex;i<lastIndex;++i) {
                const double v=data[i];
                if(!std::isfinite(v))continue;
                const double f=double(i)/double(data.size()-1);
                const double x=p.left()+(f-m_start)/m_span*p.width();
                poly.append(QPointF(x,yPixel(channel,v)));
            }
            painter.drawPolyline(poly);
        } else {
            const int columns=std::max(1,int(p.width()));
            for(int col=0;col<columns;++col) {
                const double f0=m_start+m_span*double(col)/columns;
                const double f1=m_start+m_span*double(col+1)/columns;
                const int a=qBound(0,int(std::floor(f0*(data.size()-1))),static_cast<int>(data.size()-1));
                const int b=qBound(a+1,int(std::ceil(f1*(data.size()-1)))+1,static_cast<int>(data.size()));
                bool valid=false;double mn=0,mx=0;
                for(int i=a;i<b;++i) {
                    const double v=data[i]; if(!std::isfinite(v))continue;
                    if(!valid){mn=mx=v;valid=true;}else{mn=std::min(mn,v);mx=std::max(mx,v);}
                }
                if(valid) painter.drawLine(QPointF(p.left()+col,yPixel(channel,mx)),
                                           QPointF(p.left()+col,yPixel(channel,mn)));
            }
        }
    };
    if(m_show1)drawChannel(m_frame.ch1,trace1,0);
    if(m_show2)drawChannel(m_frame.ch2,trace2,1);
    if (m_frame.triggerEnabled) {
        const double f = m_frame.trigger.positionPercent / 100.0;
        const double posX = p.left() + (f-m_start)/m_span*p.width();
        const QColor color = m_frame.trigger.channel == 0 ? trace1 : trace2;
        painter.setPen(QPen(QColor(248,167,73),1.0,Qt::DashLine));
        if (posX >= p.left() && posX <= p.right()) {
            painter.drawLine(QPointF(posX,p.top()),QPointF(posX,p.bottom()));
            painter.setPen(QColor(248,167,73));
            painter.drawText(QRectF(posX+4,p.bottom()-20,62,16),QStringLiteral("TRIG"));
        }
        const double posY = yPixel(m_frame.trigger.channel,m_frame.trigger.levelVolts);
        if (posY >= p.top() && posY <= p.bottom()) {
            painter.setPen(QPen(color,1.0,Qt::DashLine));
            painter.drawLine(QPointF(p.left(),posY),QPointF(p.right(),posY));
        }
    }
    const auto drawCursor=[&](double fraction,const QColor& c,const QString& tag) {
        if(fraction<m_start || fraction>m_start+m_span)return;
        const double x=p.left()+(fraction-m_start)/m_span*p.width();
        painter.setPen(QPen(c,1,Qt::DashLine));
        painter.drawLine(QPointF(x,p.top()),QPointF(x,p.bottom()));
        painter.setPen(c);
        painter.drawText(QRectF(x+4,p.top()+3,22,17),tag);
    };
    drawCursor(m_cursorA,cursorAColor,QStringLiteral("A"));
    drawCursor(m_cursorB,cursorBColor,QStringLiteral("B"));
    painter.restore();
    painter.setPen(trace1);
    painter.drawText(QRectF(p.left()+10,p.top()+6,110,18),Qt::AlignLeft|Qt::AlignVCenter,
                     m_autoScale ? QStringLiteral("CH1 Auto") : QStringLiteral("CH1 %1 V/div").arg(m_voltsPerDiv[0]));
    painter.setPen(trace2);
    painter.drawText(QRectF(p.left()+135,p.top()+6,110,18),Qt::AlignLeft|Qt::AlignVCenter,
                     m_autoScale ? QStringLiteral("CH2 Auto") : QStringLiteral("CH2 %1 V/div").arg(m_voltsPerDiv[1]));
}
void WaveformPlot::mousePressEvent(QMouseEvent* ev) {
    if (!plotBounds().contains(ev->position())) return;
    if(ev->button()==Qt::LeftButton) {
        m_downPoint=ev->position().toPoint(); m_lastPoint=m_downPoint; m_dragging=false;
    } else if(ev->button()==Qt::RightButton) {
        m_cursorB=fractionAt(ev->position().x());update();publishCursors();
    }
}
void WaveformPlot::mouseMoveEvent(QMouseEvent* ev) {
    if (!(ev->buttons() & Qt::LeftButton)) return;
    const QPoint now=ev->position().toPoint();
    if((now-m_downPoint).manhattanLength()>4) m_dragging=true;
    if(m_dragging) {
        const double dx=double(now.x()-m_lastPoint.x());
        const double shift=-dx/std::max(1.0,plotBounds().width())*m_span;
        m_start=qBound(0.0,m_start+shift,1.0-m_span);
        update();
    }
    m_lastPoint=now;
}
void WaveformPlot::mouseReleaseEvent(QMouseEvent* ev) {
    if(ev->button()==Qt::LeftButton && !m_dragging && plotBounds().contains(ev->position())) {
        m_cursorA=fractionAt(ev->position().x()); update(); publishCursors();
    }
    m_dragging=false;
}
void WaveformPlot::mouseDoubleClickEvent(QMouseEvent*) { resetView(); }
void WaveformPlot::wheelEvent(QWheelEvent* ev) {
    if (!plotBounds().contains(ev->position()) || !hasData()) return;
    const double factor=std::pow(0.85,double(ev->angleDelta().y())/120.0);
    const double minSpan=std::min(1.0,32.0/double(std::max<qsizetype>(1,m_frame.ch1.size())));
    const double span=qBound(minSpan,m_span*factor,1.0);
    const double anchor=qBound(0.0,(ev->position().x()-plotBounds().left())/plotBounds().width(),1.0);
    const double dataAnchor=m_start+m_span*anchor;
    m_start=qBound(0.0,dataAnchor-span*anchor,1.0-span);
    m_span=span;
    update();ev->accept();
}
