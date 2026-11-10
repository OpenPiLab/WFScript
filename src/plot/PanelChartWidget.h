#pragma once
#include "PlotCommand.h"
#include "../widgets/WidgetDefinition.h"
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QPainter;
class QPaintEvent;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

// Panel-only independent X/Y chart. The main Oscilloscope workspace continues
// to use WaveformPlot. All series/view state belongs to this native QWidget.
class PanelChartWidget final : public QWidget {
    Q_OBJECT
public:
    explicit PanelChartWidget(QWidget* parent=nullptr);
    void applyDefinition(const PanelControl& definition);
    void applyCommand(const PlotCommand& command);
    int seriesCount() const { return m_series.size(); }
    qsizetype pointCount(const QString& series) const;
    QString chartType() const { return m_settings.value("chartType").toString(); }
    bool hasSeries(const QString& name) const;
    void resetView();
    QVariantMap seriesStyle(const QString& name) const;
    QVariantMap resolvedRanges() const; // effective axis limits for diagnostics/tests
    double effectiveSeriesWidth(const QString& name) const;
    bool cursorsEnabled() const { return m_settings.value("showCursors").toBool() && !m_inspectorDismissed; }
    bool measurementPaneVisible() const { return cursorsEnabled(); }
    QVariantMap cursorMeasurements() const; // samples only; no extrapolation
    QRectF measurementPaneRect() const; // draggable overlay for tests
protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    struct Series {
        QString name;
        QVector<double> x,y;
        bool xAscending=true; // cached for efficient nearest-sample snapping
        QColor color;
        double width=0.0; // 0: follow chart lineWidth
        double markerSize=2.6;
        bool visible=true;
        QString axis=QStringLiteral("left");
    };
    QVector<Series> m_series;
    QVariantMap m_settings;
    bool m_viewActive=false;
    double m_viewX0=0, m_viewX1=1, m_viewY0=0, m_viewY1=1;
    bool m_panning=false;
    bool m_draggingInspector=false;
    bool m_inspectorDismissed=false; // X hides the overlay, not the underlying chart setting
    bool m_cursorsCleared=false; // after X, script defaults cannot resurrect old markers
    QPointF m_paneTopLeft{0.0,0.0}; // chart-relative; runtime only
    bool m_paneMoved=false;
    QPointF m_paneGrabOffset;
    QPointF m_last;
    double m_cursorA=0.0, m_cursorB=0.0;
    bool m_haveCursorA=false, m_haveCursorB=false;
    QRectF chartRect() const;
    QRectF legendRow(int index) const;
    void secondaryRanges(double& ymin,double& ymax) const;
    bool sampleDomain(double& lo,double& hi) const;
    double mouseXValue(double px) const;
    bool nearestSampleX(double requested,double& snapped) const;
    void placeMouseCursor(Qt::KeyboardModifiers modifiers,double x);
    QRectF paneCloseRect() const;
    void dismissMeasurements();
    static bool sampleAt(const Series& series,double x,bool digital,double& y,bool exactOnly=false);
    void drawMeasurementPane(QPainter& painter,const QRectF& plot,bool light,bool digital);
    void drawDigital(QPainter& painter, const QRectF& plot, bool light);
    void ranges(double& xmin,double& xmax,double& ymin,double& ymax) const;
    static QVariantMap defaults();
    void updateSeries(const PlotCommand& command);
};
