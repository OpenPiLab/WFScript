#include "ScriptTimerManager.h"
#include <QJSEngine>
#include <QTimer>
#include <QtGlobal>
#include <cmath>
#include <limits>

ScriptTimerManager::ScriptTimerManager(QJSEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {}

ScriptTimerManager::~ScriptTimerManager() { shutdown(); }

int ScriptTimerManager::schedule(const QJSValue& callback, double milliseconds, bool repeating) {
    m_lastError.clear();
    if (m_stopped || !m_engine) {
        m_lastError = QStringLiteral("Panel timers are unavailable during shutdown");
        return 0;
    }
    if (!callback.isCallable()) {
        m_lastError = QStringLiteral("Timer callback must be a function");
        return 0;
    }
    if (!std::isfinite(milliseconds) || std::floor(milliseconds) != milliseconds ||
        milliseconds < kMinDelayMs || milliseconds > kMaxDelayMs) {
        m_lastError = QStringLiteral("Timer delay must be an integer from 10 to 86400000 milliseconds");
        return 0;
    }
    if (m_entries.size() >= kMaxActive) {
        m_lastError = QStringLiteral("Maximum of 256 active Panel timers exceeded");
        return 0;
    }
    // IDs are monotonically assigned for this Panel; none is reused until
    // the registry has exhausted its positive integer range.
    if (m_nextId == std::numeric_limits<int>::max()) {
        m_lastError = QStringLiteral("Timer ID range exhausted");
        return 0;
    }
    const int id = m_nextId++;
    auto* timer = new QTimer(this);
    timer->setTimerType(Qt::CoarseTimer); // supervisory/event timers, not hardware clocking
    timer->setSingleShot(!repeating);
    timer->setInterval(static_cast<int>(milliseconds));
    m_entries.insert(id, {timer, callback, repeating});
    connect(timer, &QTimer::timeout, this, [this, id] { fire(id); });
    timer->start();
    return id;
}

void ScriptTimerManager::cancel(int id) {
    auto it = m_entries.find(id);
    if (it == m_entries.end()) return; // browser-style tolerant clear
    QTimer* timer = it->timer;
    m_entries.erase(it); // immediately release QJSValue reference
    timer->stop();
    timer->deleteLater(); // safe even when called inside its callback
}

int ScriptTimerManager::activeCount() const { return m_entries.size(); }

void ScriptTimerManager::fire(int id) {
    auto it = m_entries.find(id);
    if (m_stopped || it == m_entries.end() || !m_engine) return;
    QJSValue callback = it->callback; // retain across a self-clear or shutdown
    if (!it->repeating) cancel(id); // one-shot must be absent during callback
    emit callbackStarted();
    const QJSValue result = callback.call();
    emit callbackFinished();
    if (!m_stopped && !m_engine->isInterrupted() && result.isError())
        emit callbackFailed(QStringLiteral("Panel JavaScript timer %1").arg(id), result);
}

void ScriptTimerManager::shutdown() {
    if (m_stopped) return;
    m_stopped = true;
    const auto ids = m_entries.keys();
    for (int id : ids) cancel(id);
}

QString ScriptTimerManager::bootstrapSource() {
    return QStringLiteral(R"JS(
        // Browser-style Panel timers backed by the native Qt event loop.
        function __schedulePanelTimer(callback, ms, repeat, extra) {
            if (typeof callback !== 'function') throw new TypeError('Timer callback must be a function');
            if (typeof ms !== 'number' || !Number.isFinite(ms) || !Number.isInteger(ms) ||
                ms < 10 || ms > 86400000)
                throw new RangeError('Timer delay must be an integer from 10 to 86400000 ms');
            var wrapped = extra.length ? function(){callback.apply(undefined, extra);} : callback;
            var id = __timerHost.schedule(wrapped, ms, repeat);
            if (!id) throw new Error(__timerHost.lastError);
            return id;
        }
        function setTimeout(callback, ms, ...args) { return __schedulePanelTimer(callback,ms,false,args); }
        function setInterval(callback, ms, ...args) { return __schedulePanelTimer(callback,ms,true,args); }
        function clearTimeout(id) { __timerHost.cancel(id); }
        function clearInterval(id) { __timerHost.cancel(id); }
    )JS");
}
