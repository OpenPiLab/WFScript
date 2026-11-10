#pragma once

#include <QWidget>
#include <QVariant>
#include <QVector>
#include <QString>

class QLabel;
class QProgressBar;
class QScrollArea;

// Visual test-progress display. This is NOT a test scheduler: JavaScript owns
// test execution, timeouts, retries and any hardware control.
//
// Runtime payloads (converted from QJSValue to native QVariant on the script
// thread before GUI delivery):
//   ui.set(id, [{id:'power',name:'Check power',status:'pending'}, ...]);
//   ui.set(id, {update:{id:'power',status:'pass',elapsedMs:120,message:'3.301 V'}});
//   ui.set(id, {reset:true});            // retain planned steps; reset states
//   ui.set(id, {clear:true});            // remove every step
// Invalid data never changes existing visual state.
class TestSequenceWidget final : public QWidget {
public:
    struct Step {
        QString id;
        QString name;
        QString description;
        QString status = QStringLiteral("pending");
        QString message;
        qint64 elapsedMs = -1;
    };

    explicit TestSequenceWidget(QWidget* parent=nullptr);
    void configure(const QString& title, bool showProgress, bool showElapsed,
                   bool showNumbers, bool autoScroll, bool compactMode,
                   int maxSteps);
    bool applySteps(const QVariant& data);

    int stepCount() const { return m_steps.size(); }
    int completedSteps() const;
    int percentComplete() const;
    QString statusFor(const QString& id) const;
    QString overallState() const;
    const QVector<Step>& steps() const { return m_steps; }

private:
    static bool parseStep(const QVariantMap& map, Step* step, bool partial=false);
    bool replaceSteps(const QVariantList& data);
    bool updateStep(const QVariant& patch);
    void rebuild();

    QVector<Step> m_steps;
    QString m_title = QStringLiteral("Test Sequence");
    bool m_showProgress = true;
    bool m_showElapsed = true;
    bool m_showNumbers = true;
    bool m_autoScroll = true;
    bool m_compactMode = false;
    int m_maxSteps = 64;
    QLabel* m_heading = nullptr;
    QLabel* m_overview = nullptr;
    QProgressBar* m_progress = nullptr;
    QScrollArea* m_scroll = nullptr;
};
