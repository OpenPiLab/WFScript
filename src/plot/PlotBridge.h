#pragma once
#include "PlotCommand.h"
#include <QObject>
#include <QSet>
#include <QHash>
#include <QVariantList>

class ScopeBridge;

// Lives exclusively on the PanelRuntime/QJSEngine thread. Validates bounded
// script values before sending immutable commands to the GUI via Qt signals.
class PlotBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError)
public:
    PlotBridge(QSet<QString> plotIds, ScopeBridge* scope, QObject* parent=nullptr);
    QString lastError() const { return m_lastError; }
    Q_INVOKABLE bool setSeries(const QString& plotId, const QString& name,
                               const QVariantList& x, const QVariantList& y,
                               const QVariantMap& options);
    Q_INVOKABLE bool appendSeries(const QString& plotId, const QString& name,
                                  const QVariantList& x, const QVariantList& y,
                                  const QVariantMap& options);
    Q_INVOKABLE bool appendPoint(const QString& plotId,const QString& name,double x,double y);
    Q_INVOKABLE bool setSamples(const QString& plotId, const QString& name,
                                const QVariantList& y, double sampleRateHz,
                                double startSeconds, const QVariantMap& options);
    Q_INVOKABLE bool fromScope(const QString& plotId, int channel, const QString& name);
    Q_INVOKABLE bool configure(const QString& plotId, const QVariantMap& settings);
    Q_INVOKABLE bool clear(const QString& plotId);
    Q_INVOKABLE bool styleSeries(const QString& plotId,const QString& name,const QVariantMap& options);
    Q_INVOKABLE bool removeSeries(const QString& plotId, const QString& name);
signals:
    void commandReady(const PlotCommand& command);
private:
    QSet<QString> m_plotIds;
    ScopeBridge* m_scope=nullptr; // worker-thread only; owned by PanelRuntime
    QString m_lastError;
    // Names only for synchronous series-capacity validation; no sample cache.
    QHash<QString,QSet<QString>> m_seriesNames;
    bool validPlot(const QString& id);
    bool validSeriesOptions(const QVariantMap& options);
    bool validNewSeries(const QString& id,const QString& name);
    bool reject(const QString& message);
    static bool readNumbers(const QVariantList& input, QVector<double>* output);
};
