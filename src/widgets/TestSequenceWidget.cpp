#include "TestSequenceWidget.h"
#include "InstrumentVisuals.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaType>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QVBoxLayout>
#include <QtGlobal>
#include <cmath>
#include <utility>

namespace {
bool isDone(const QString& status) {
    return status==QStringLiteral("pass") || status==QStringLiteral("done") ||
           status==QStringLiteral("fail") ||
           status==QStringLiteral("error") || status==QStringLiteral("skipped") ||
           status==QStringLiteral("aborted");
}
bool validStatus(const QString& status) {
    return status==QStringLiteral("pending") || status==QStringLiteral("running") ||
           isDone(status);
}
QColor statusColor(const QString& status, bool light) {
    if(status==QStringLiteral("pass") || status==QStringLiteral("done")) return QColor(light?"#14754c":"#42dc9b");
    if(status==QStringLiteral("fail") || status==QStringLiteral("error"))
        return QColor(light?"#ae2639":"#ff777e");
    if(status==QStringLiteral("running")) return QColor(light?"#196eae":"#59bffe");
    if(status==QStringLiteral("skipped")) return QColor(light?"#7e6619":"#f2c968");
    if(status==QStringLiteral("aborted")) return QColor(light?"#a25526":"#f59d6e");
    return QColor(light?"#697e90":"#95a9bf");
}
QString timeText(qint64 ms) {
    if(ms<0)return {};
    if(ms<1000)return QStringLiteral("%1 ms").arg(ms);
    return QStringLiteral("%1 s").arg(double(ms)/1000.0,0,'f',2);
}
QString symbolFor(const QString& state) {
    if(state==QStringLiteral("pass") || state==QStringLiteral("done"))return QStringLiteral("✓");
    if(state==QStringLiteral("fail") || state==QStringLiteral("error"))return QStringLiteral("✕");
    if(state==QStringLiteral("running"))return QStringLiteral("▶");
    if(state==QStringLiteral("skipped"))return QStringLiteral("–");
    if(state==QStringLiteral("aborted"))return QStringLiteral("■");
    return QStringLiteral("○");
}
}

TestSequenceWidget::TestSequenceWidget(QWidget* parent):QWidget(parent) {
    setObjectName(QStringLiteral("wfTestSequenceRoot"));
    setAttribute(Qt::WA_StyledBackground,true);
    auto* outer=new QVBoxLayout(this);
    outer->setContentsMargins(10,8,10,8);
    outer->setSpacing(6);
    auto* header=new QHBoxLayout;
    m_heading=new QLabel(this);
    auto font=m_heading->font();font.setBold(true);m_heading->setFont(font);
    m_overview=new QLabel(this);
    m_overview->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    header->addWidget(m_heading,1);header->addWidget(m_overview);
    outer->addLayout(header);
    m_progress=new QProgressBar(this);
    m_progress->setRange(0,100);
    m_progress->setTextVisible(true);
    m_progress->setFixedHeight(18);
    outer->addWidget(m_progress);
    m_scroll=new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea {background:transparent;border:none;}"
       "QScrollArea > QWidget > QWidget {background:transparent;}") );
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(m_scroll,1);
    configure(m_title,true,true,true,true,false,64);
}

void TestSequenceWidget::configure(const QString& title,bool showProgress,bool showElapsed,
                                   bool showNumbers,bool autoScroll,bool compactMode,int maxSteps) {
    m_title=title.left(120);
    m_showProgress=showProgress;
    m_showElapsed=showElapsed;
    m_showNumbers=showNumbers;
    m_autoScroll=autoScroll;
    m_compactMode=compactMode;
    m_maxSteps=qBound(1,maxSteps,100);
    if(m_steps.size()>m_maxSteps)m_steps.resize(m_maxSteps);
    rebuild();
}

bool TestSequenceWidget::parseStep(const QVariantMap& input, Step* dest, bool partial) {
    if(!dest || input.isEmpty())return false;
    if(!partial && (!input.contains("id") || !input.contains("name")))return false;
    if(input.contains("id")) {
        if(input.value("id").metaType().id()!=QMetaType::QString)return false;
        const QString id=input.value("id").toString().trimmed();
        if(id.isEmpty() || id.size()>64)return false;
        dest->id=id;
    }
    if(input.contains("name")) {
        if(input.value("name").metaType().id()!=QMetaType::QString)return false;
        const QString name=input.value("name").toString().trimmed();
        if(name.isEmpty() || name.size()>120)return false;
        dest->name=name;
    }
    if(input.contains("description")) {
        if(input.value("description").metaType().id()!=QMetaType::QString ||
           input.value("description").toString().size()>320)return false;
        dest->description=input.value("description").toString();
    }
    if(input.contains("status")) {
        if(input.value("status").metaType().id()!=QMetaType::QString)return false;
        const QString status=input.value("status").toString().trimmed().toLower();
        if(!validStatus(status))return false;
        dest->status=status;
    }
    if(input.contains("message")) {
        if(input.value("message").metaType().id()!=QMetaType::QString ||
           input.value("message").toString().size()>320)return false;
        dest->message=input.value("message").toString();
    }
    if(input.contains("elapsedMs")) {
        const QVariant v=input.value("elapsedMs");
        if(v.isNull())dest->elapsedMs=-1;
        else {
            const int type=v.metaType().id();
            if(type!=QMetaType::Int && type!=QMetaType::LongLong &&
               type!=QMetaType::Double && type!=QMetaType::UInt)return false;
            const double number=v.toDouble();
            if(!std::isfinite(number) || number<0 || number>86400000 ||
               std::floor(number)!=number)return false;
            dest->elapsedMs=qint64(number);
        }
    }
    return !dest->id.isEmpty() && !dest->name.isEmpty();
}

bool TestSequenceWidget::replaceSteps(const QVariantList& input) {
    if(input.size()>m_maxSteps)return false;
    QVector<Step> next;
    next.reserve(input.size());
    QSet<QString> ids;
    for(const QVariant& element:input) {
        if(element.metaType().id()!=QMetaType::QVariantMap)return false;
        Step candidate;
        if(!parseStep(element.toMap(),&candidate) || ids.contains(candidate.id))return false;
        ids.insert(candidate.id);
        next.push_back(std::move(candidate));
    }
    m_steps=std::move(next);
    rebuild();
    return true;
}

bool TestSequenceWidget::updateStep(const QVariant& patch) {
    if(patch.metaType().id()!=QMetaType::QVariantMap)return false;
    const QVariantMap fields=patch.toMap();
    const QString id=fields.value("id").toString();
    if(id.isEmpty())return false;
    // Reject unknown keys and attempts to rename an established step.
    for(auto it=fields.cbegin();it!=fields.cend();++it)
        if(it.key()!=QStringLiteral("id") && it.key()!=QStringLiteral("name") &&
           it.key()!=QStringLiteral("status") && it.key()!=QStringLiteral("description") &&
           it.key()!=QStringLiteral("message") && it.key()!=QStringLiteral("elapsedMs"))return false;
    for(int i=0;i<m_steps.size();++i) {
        if(m_steps.at(i).id!=id)continue;
        Step next=m_steps.at(i);
        if(!parseStep(fields,&next,true) || next.id!=id)return false;
        m_steps[i]=std::move(next);
        rebuild();
        return true;
    }
    return false;
}

bool TestSequenceWidget::applySteps(const QVariant& data) {
    if(data.metaType().id()==QMetaType::QVariantList)return replaceSteps(data.toList());
    if(data.metaType().id()!=QMetaType::QVariantMap)return false;
    const QVariantMap operation=data.toMap();
    if(operation.size()==1 && operation.value("reset").metaType().id()==QMetaType::Bool &&
       operation.value("reset").toBool()) {
        for(auto& step:m_steps){step.status=QStringLiteral("pending");step.message.clear();step.elapsedMs=-1;}
        rebuild();return true;
    }
    if(operation.size()==1 && operation.value("clear").metaType().id()==QMetaType::Bool &&
       operation.value("clear").toBool()) return replaceSteps({});
    if(operation.size()==1 && operation.contains("update"))return updateStep(operation.value("update"));
    return false;
}

int TestSequenceWidget::completedSteps() const {
    int count=0;
    for(const auto& step:m_steps)if(isDone(step.status))++count;
    return count;
}
int TestSequenceWidget::percentComplete() const {
    return m_steps.isEmpty()?0:int(double(completedSteps())*100.0/double(m_steps.size())+0.5);
}
QString TestSequenceWidget::statusFor(const QString& id) const {
    for(const auto& step:m_steps)if(step.id==id)return step.status;
    return {};
}
QString TestSequenceWidget::overallState() const {
    bool running=false,hasFailed=false,hasError=false,hasAborted=false;
    for(const auto& step:m_steps) {
        if(step.status==QStringLiteral("error"))hasError=true;
        if(step.status==QStringLiteral("fail"))hasFailed=true;
        if(step.status==QStringLiteral("aborted"))hasAborted=true;
        if(step.status==QStringLiteral("running"))running=true;
    }
    if(hasError)return QStringLiteral("ERROR");
    if(hasFailed)return QStringLiteral("FAILED");
    if(hasAborted)return QStringLiteral("ABORTED");
    if(running)return QStringLiteral("RUNNING");
    if(!m_steps.isEmpty() && completedSteps()==m_steps.size())return QStringLiteral("COMPLETE");
    return QStringLiteral("PENDING");
}

void TestSequenceWidget::rebuild() {
    if(!m_heading || !m_scroll)return;
    const bool light=InstrumentVisuals::light(this);
    const QString fg=light?QStringLiteral("#23384c"):QStringLiteral("#e0ebf5");
    const QString muted=light?QStringLiteral("#647b8c"):QStringLiteral("#97afc4");
    const QString background=light?QStringLiteral("#edf4fa"):QStringLiteral("#101e2f");
    const QString rowBg=light?QStringLiteral("#e3edf5"):QStringLiteral("#172b3e");
    const QString border=light?QStringLiteral("#c1d0db"):QStringLiteral("#3a536b");
    m_heading->setText(m_title.isEmpty()?QStringLiteral("Test Sequence"):m_title);
    m_heading->setStyleSheet(QStringLiteral("color:%1;").arg(fg));
    m_overview->setText(QStringLiteral("%1 / %2  •  %3")
        .arg(completedSteps()).arg(m_steps.size()).arg(overallState()));
    m_overview->setStyleSheet(QStringLiteral("color:%1;").arg(
        statusColor(overallState()==QStringLiteral("FAILED")?QStringLiteral("fail"):
            overallState()==QStringLiteral("ERROR")?QStringLiteral("error"):
            overallState()==QStringLiteral("RUNNING")?QStringLiteral("running"):
            overallState()==QStringLiteral("COMPLETE")?QStringLiteral("pass"):
            QStringLiteral("pending"),light).name()));
    m_progress->setVisible(m_showProgress);
    m_progress->setValue(percentComplete());
    m_progress->setFormat(QStringLiteral("%p%"));
    m_progress->setStyleSheet(QStringLiteral(
        "QProgressBar{border:1px solid %1;border-radius:5px;background:%2;color:%3;text-align:center;}"
        "QProgressBar::chunk{background:#30b98b;border-radius:4px;}").arg(border,background,fg));
    setStyleSheet(QStringLiteral("QWidget#wfTestSequenceRoot{background:%1;border:1px solid %2;border-radius:9px;}")
        .arg(background,border));

    const int oldScroll=m_scroll->verticalScrollBar()->value();
    auto* content=new QWidget;
    content->setObjectName(QStringLiteral("sequenceRows"));
    content->setStyleSheet(QStringLiteral("background:transparent;"));
    auto* list=new QVBoxLayout(content);
    list->setContentsMargins(0,0,0,0);
    list->setSpacing(m_compactMode?3:6);
    int currentRow=-1;
    for(int i=0;i<m_steps.size();++i) {
        const Step& s=m_steps.at(i);
        if(s.status==QStringLiteral("running"))currentRow=i;
        auto* row=new QFrame(content);
        row->setObjectName(QStringLiteral("sequenceStep_%1").arg(s.id));
        row->setStyleSheet(QStringLiteral("QFrame{background:%1;border:1px solid %2;border-radius:5px;}")
            .arg(rowBg,border));
        auto* h=new QHBoxLayout(row);
        h->setContentsMargins(8,m_compactMode?4:7,8,m_compactMode?4:7);
        h->setSpacing(9);
        const QColor tint=statusColor(s.status,light);
        auto* symbol=new QLabel(symbolFor(s.status),row);
        symbol->setObjectName(QStringLiteral("sequenceIcon_%1").arg(s.id));
        symbol->setFixedWidth(21);symbol->setAlignment(Qt::AlignCenter);
        symbol->setStyleSheet(QStringLiteral("border:0;color:%1;font-weight:bold;font-size:15px;").arg(tint.name()));
        h->addWidget(symbol);
        if(m_showNumbers) {
            auto* order=new QLabel(QString::number(i+1),row);
            order->setFixedWidth(19);order->setAlignment(Qt::AlignCenter);
            order->setStyleSheet(QStringLiteral("border:0;color:%1;").arg(muted));
            h->addWidget(order);
        }
        auto* body=new QVBoxLayout;
        body->setSpacing(2);
        auto* title=new QLabel(s.name,row);
        title->setObjectName(QStringLiteral("sequenceName_%1").arg(s.id));
        title->setStyleSheet(QStringLiteral("border:0;color:%1;font-weight:600;").arg(fg));
        title->setWordWrap(true);
        body->addWidget(title);
        const QString notes=!s.message.isEmpty()?s.message:s.description;
        if(!notes.isEmpty() && !m_compactMode) {
            auto* detail=new QLabel(notes,row);
            detail->setWordWrap(true);
            detail->setStyleSheet(QStringLiteral("border:0;color:%1;font-size:11px;").arg(muted));
            body->addWidget(detail);
        }
        h->addLayout(body,1);
        if(m_showElapsed && s.elapsedMs>=0) {
            auto* elapsed=new QLabel(timeText(s.elapsedMs),row);
            elapsed->setStyleSheet(QStringLiteral("border:0;color:%1;").arg(muted));
            h->addWidget(elapsed);
        }
        auto* status=new QLabel(s.status.toUpper(),row);
        status->setObjectName(QStringLiteral("sequenceStatus_%1").arg(s.id));
        status->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        status->setMinimumWidth(61);
        status->setStyleSheet(QStringLiteral("border:0;color:%1;font-weight:bold;").arg(tint.name()));
        h->addWidget(status);
        list->addWidget(row);
    }
    if(m_steps.isEmpty()) {
        auto* hint=new QLabel(QStringLiteral("No test steps configured.\nSet steps through the Property Inspector or ui.set()."),content);
        hint->setAlignment(Qt::AlignCenter);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color:%1;").arg(muted));
        list->addWidget(hint);
    }
    list->addStretch(1);
    m_scroll->setWidget(content); // QScrollArea deletes its previous widget.
    if(m_autoScroll && currentRow>=0) {
        // ensureWidgetVisible acts after the layout has calculated the new geometry.
        m_scroll->ensureWidgetVisible(content->findChild<QWidget*>(
            QStringLiteral("sequenceStep_%1").arg(m_steps.at(currentRow).id)),0,20);
    } else m_scroll->verticalScrollBar()->setValue(oldScroll);
}
