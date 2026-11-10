#pragma once

#include "ScopeFrame.h"
#include <QPoint>
#include <QRectF>
#include <QWidget>

class QPaintEvent;
class QMouseEvent;
class QWheelEvent;

// QPainter-based oscilloscope trace viewer. No Qt Charts, Qwt or OpenGL required.
// Data arrive only via setFrame() on the UI thread. Acquisition never draws.
class WaveformPlot final : public QWidget {
    Q_OBJECT
public:
    explicit WaveformPlot(QWidget* parent = nullptr);
    void setFrame(const ScopeFrame& frame);
    void setSoftwareAlignmentEnabled(bool enabled);
    const ScopeFrame& frame() const { return m_frame; }
    bool hasData() const { return !m_frame.ch1.isEmpty(); }
    void setChannelsVisible(bool ch1, bool ch2);
    void setVoltageScales(bool automatic, double ch1VoltsPerDiv, double ch2VoltsPerDiv,
                          double ch1CenterV, double ch2CenterV);
    void resetView();
signals:
    void cursorChanged(const QString& description);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
private:
    ScopeFrame m_frame;
    ScopeFrame m_rawFrame;
    bool m_softwareAlignment = false;
    bool m_show1 = true;
    bool m_show2 = true;
    bool m_autoScale = false; // fixed scale prevents frame-to-frame vertical jumping
    double m_voltsPerDiv[2] = {0.5, 0.5};
    double m_centerVolts[2] = {0.0, 0.0};
    double m_start = 0.0;  // normalized fraction of total capture
    double m_span = 1.0;
    double m_cursorA = 0.25;
    double m_cursorB = 0.75;
    bool m_dragging = false;
    QPoint m_downPoint;
    QPoint m_lastPoint;
    QRectF plotBounds() const;
    double fractionAt(double x) const;
    double sampleAt(const QVector<double>& data, double fraction) const;
    void publishCursors();
    void regenerateDisplayFrame();
};
