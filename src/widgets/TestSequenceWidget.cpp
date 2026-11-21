#include "TestSequenceWidget.h"
#include "InstrumentVisuals.h"

#include <QColor>
#include <QFrame>
#include <QTableView>
#include <QAbstractTableModel>
#include <QHeaderView>
#include <QFont>
#include <QPushButton>
#include <QEvent>
#include <QBrush>
#include "../TestSequenceCapacity.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaType>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSet>
#include <QStringList>
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


// Native large sequences keep their entire step history in one model. A
// QTableView creates no QWidget per row, and scrolls to arbitrary run indices.
// Small script-owned sequences retain their existing card-style presentation.
class VirtualStepModel final : public QAbstractTableModel {
public:
    explicit VirtualStepModel(const QVector<TestSequenceWidget::Step>* steps,QObject* parent)
        :QAbstractTableModel(parent),m_steps(steps){}
    int rowCount(const QModelIndex& parent=QModelIndex()) const override {
        return parent.isValid()?0:m_steps->size();
    }
    int columnCount(const QModelIndex& parent=QModelIndex()) const override {
        return parent.isValid()?0:4;
    }
    QVariant headerData(int section,Qt::Orientation orientation,int role) const override {
        if(role!=Qt::DisplayRole||orientation!=Qt::Horizontal)return {};
        return QStringList{QStringLiteral("#"),QStringLiteral("Test step"),
                           QStringLiteral("Elapsed"),QStringLiteral("Result")}.value(section);
    }
    QVariant data(const QModelIndex& idx,int role) const override {
        if(!idx.isValid()||idx.row()<0||idx.row()>=m_steps->size())return {};
        const auto& step=m_steps->at(idx.row());
        if(role==Qt::DisplayRole) {
            switch(idx.column()) {
            case 0:return idx.row()+1;
            case 1:return step.name;
            case 2:return timeText(step.elapsedMs);
            default:return step.status.toUpper();
            }
        }
        if(role==Qt::ToolTipRole)return step.description+QStringLiteral("\n")+step.message;
        if(role==Qt::TextAlignmentRole)return int(idx.column()==1?Qt::AlignLeft|Qt::AlignVCenter:Qt::AlignCenter);
        if(role==Qt::ForegroundRole && idx.column()==3)
            return QBrush(statusColor(step.status,m_light));
        if(role==Qt::FontRole && (idx.column()==1||idx.column()==3)) {
            QFont f;f.setBold(true);return f;
        }
        if(role==Qt::BackgroundRole && step.status==QLatin1String("running"))
            return QBrush(QColor(m_light?"#d2ebf5":"#1b4359"));
        return {};
    }
    void setLight(bool light) {
        if(m_light==light)return;
        m_light=light;
        if(rowCount()>0)
            emit dataChanged(index(0,3),index(rowCount()-1,3),
                             {Qt::ForegroundRole,Qt::BackgroundRole});
    }
    void resetAll() {beginResetModel();endResetModel();}
    void changedRow(int row) {if(row>=0&&row<m_steps->size())emit dataChanged(index(row,0),index(row,3));}
private:
    const QVector<TestSequenceWidget::Step>* m_steps;
    bool m_light=false;
};

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
    m_virtualView=new QTableView(this);
    m_virtualView->setObjectName(QStringLiteral("virtualSequenceTable"));
    m_virtualModel=new VirtualStepModel(&m_steps,m_virtualView);
    m_virtualView->setModel(m_virtualModel);
    m_virtualView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_virtualView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_virtualView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_virtualView->setAlternatingRowColors(true);
    m_virtualView->setShowGrid(false);
    m_virtualView->verticalHeader()->hide();
    m_virtualView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_virtualView->verticalHeader()->setDefaultSectionSize(30);
    m_virtualView->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_virtualView->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    m_virtualView->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    m_virtualView->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    m_virtualView->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    m_virtualView->viewport()->installEventFilter(this);
    m_virtualView->installEventFilter(this);
    m_virtualView->verticalScrollBar()->installEventFilter(this);
    connect(m_virtualView->verticalScrollBar(),&QScrollBar::sliderPressed,this,[this]{
        m_followLive=false;m_followButton->setText(QStringLiteral("Follow live"));
    });
    outer->addWidget(m_virtualView,1);
    m_virtualView->hide();
    m_followButton=new QPushButton(QStringLiteral("Following live"),this);
    m_followButton->setToolTip(QStringLiteral("Scroll to the active step; browsing earlier rows pauses automatic scrolling"));
    connect(m_followButton,&QPushButton::clicked,this,[this]{m_followLive=true;followCurrent();});
    outer->addWidget(m_followButton,0,Qt::AlignRight);
    m_followButton->hide();
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

bool TestSequenceWidget::replaceSteps(const QVariantList& input,bool nativeWindow) {
    // Native Test Executive windows are bounded to 100 regardless of design-time maxSteps.
    if(input.size()>(nativeWindow?TestSequenceCapacity::visibleSequenceSteps:
                     m_maxSteps))return false;
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
    m_stepIndex.clear();m_localCompleted=0;
    m_runningIndex=-1;m_lastCompletedIndex=-1;
    for(int i=0;i<m_steps.size();++i) {
        m_stepIndex.insert(m_steps.at(i).id,i);
        if(isDone(m_steps.at(i).status)) {++m_localCompleted;m_lastCompletedIndex=i;}
        if(m_steps.at(i).status==QLatin1String("running"))m_runningIndex=i;
    }
    m_virtualMode=false;
    m_hasExternalProgress=false;m_windowStart=0;
    m_globalTotal=0;m_globalCompleted=0;m_globalState.clear();
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
    const int i=m_stepIndex.value(id,-1);
    if(i>=0 && i<m_steps.size()) {
        Step next=m_steps.at(i);
        if(!parseStep(fields,&next,true) || next.id!=id)return false;
        const Step& before=m_steps.at(i);
        if(before.name==next.name && before.description==next.description &&
           before.status==next.status && before.message==next.message &&
           before.elapsedMs==next.elapsedMs)return true;
        const bool becameRunning=before.status!=QStringLiteral("running") &&
                                  next.status==QStringLiteral("running");
        if(isDone(before.status))--m_localCompleted;
        if(isDone(next.status))++m_localCompleted;
        m_steps[i]=std::move(next);
        if(m_steps.at(i).status==QLatin1String("running"))m_runningIndex=i;
        else if(m_runningIndex==i)m_runningIndex=-1;
        if(isDone(m_steps.at(i).status))m_lastCompletedIndex=i;
        if(m_virtualMode) {
            static_cast<VirtualStepModel*>(m_virtualModel)->changedRow(i);
            refreshOverview();
            if(m_followLive && becameRunning)followCurrent();
            return true;
        }
        if(!isVisible()) {m_rowsDirty=true;return true;}
        refreshRow(i);
        refreshOverview();
        if(m_autoScroll && becameRunning && i<m_rows.size() && m_rows.at(i).frame)
            m_scroll->ensureWidgetVisible(m_rows.at(i).frame,0,20);
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
        m_localCompleted=0;m_runningIndex=-1;m_lastCompletedIndex=-1;
        if(m_virtualMode) {static_cast<VirtualStepModel*>(m_virtualModel)->resetAll();refreshOverview();}
        else if(!isVisible())m_rowsDirty=true;
        else {for(int i=0;i<m_steps.size();++i)refreshRow(i);refreshOverview();}
        return true;
    }
    if(operation.size()==1 && operation.value("clear").metaType().id()==QMetaType::Bool &&
       operation.value("clear").toBool()) return replaceSteps({});
    if(operation.size()==1 && operation.contains("virtualSteps")) {
        const QVariantList incoming=operation.value(QStringLiteral("virtualSteps")).toList();
        if(incoming.isEmpty()||incoming.size()>TestSequenceCapacity::maxExpandedSteps)return false;
        QVector<Step> next;next.reserve(incoming.size());
        QHash<QString,int> ids;
        int done=0;
        int running=-1,lastCompleted=-1;
        for(const QVariant& v:incoming) {
            if(v.metaType().id()!=QMetaType::QVariantMap)return false;
            Step step;
            if(!parseStep(v.toMap(),&step)||ids.contains(step.id))return false;
            ids.insert(step.id,next.size());
            if(isDone(step.status)) {++done;lastCompleted=next.size();}
            if(step.status==QLatin1String("running"))running=next.size();
            next.push_back(std::move(step));
        }
        m_steps=std::move(next);
        m_stepIndex=std::move(ids);m_localCompleted=done;
        m_runningIndex=running;m_lastCompletedIndex=lastCompleted;
        m_virtualMode=m_steps.size()>TestSequenceCapacity::visibleSequenceSteps;
        m_followLive=m_autoScroll;m_windowStart=0;
        m_hasExternalProgress=m_virtualMode;m_globalTotal=m_steps.size();
        m_globalCompleted=done;m_globalState=QStringLiteral("idle");
        rebuild();return true;
    }
    if(operation.size()==1 && operation.contains("window")) {
        const QVariantMap window=operation.value("window").toMap();
        const QVariantList steps=window.value(QStringLiteral("steps")).toList();
        const int start=window.contains(QStringLiteral("start"))?
            window.value(QStringLiteral("start")).toInt():-1;
        const int total=window.contains(QStringLiteral("total"))?
            window.value(QStringLiteral("total")).toInt():-1;
        if(start<0 || total<=0 || start>=total || steps.isEmpty() ||
           start+steps.size()>total || steps.size()>100)return false;
        if(!replaceSteps(steps,true))return false;
        m_windowStart=start;m_hasExternalProgress=true;m_globalTotal=total;
        m_globalCompleted=qBound(0,m_globalCompleted,total);
        rebuild(); // update visible global row numbers when the window advances
        return true;
    }
    if(operation.size()==1 && operation.contains("progress")) {
        const QVariantMap progress=operation.value("progress").toMap();
        const int total=progress.contains(QStringLiteral("total"))?
            progress.value(QStringLiteral("total")).toInt():-1;
        const int completed=progress.contains(QStringLiteral("completed"))?
            progress.value(QStringLiteral("completed")).toInt():-1;
        const QString state=progress.value(QStringLiteral("state")).toString();
        if(total<0 || completed<0 || completed>total ||
           !QStringList{QStringLiteral("idle"),QStringLiteral("running"),
               QStringLiteral("passed"),QStringLiteral("failed"),
               QStringLiteral("error"),QStringLiteral("aborted")}.contains(state))return false;
        if(total>m_maxSteps) {
            m_hasExternalProgress=true;m_globalTotal=total;
            m_globalCompleted=completed;m_globalState=state;
        } else {m_hasExternalProgress=false;m_windowStart=0;}
        refreshOverview();return true;
    }
    if(operation.size()==1 && operation.contains("update"))return updateStep(operation.value("update"));
    return false;
}

int TestSequenceWidget::completedSteps() const {
    return m_localCompleted;
}
int TestSequenceWidget::percentComplete() const {
    const int total=m_hasExternalProgress?m_globalTotal:m_steps.size();
    const int completed=m_hasExternalProgress?m_globalCompleted:completedSteps();
    return total<=0?0:int(double(completed)*100.0/double(total)+0.5);
}
QString TestSequenceWidget::statusFor(const QString& id) const {
    const int i=m_stepIndex.value(id,-1);
    return i>=0 && i<m_steps.size()?m_steps.at(i).status:QString();
}
QString TestSequenceWidget::overallState() const {
    if(m_hasExternalProgress && !m_globalState.isEmpty()) {
        if(m_globalState==QStringLiteral("passed"))return QStringLiteral("COMPLETE");
        if(m_globalState==QStringLiteral("failed"))return QStringLiteral("FAILED");
        if(m_globalState==QStringLiteral("error"))return QStringLiteral("ERROR");
        if(m_globalState==QStringLiteral("aborted"))return QStringLiteral("ABORTED");
        if(m_globalState==QStringLiteral("running"))return QStringLiteral("RUNNING");
        return QStringLiteral("PENDING");
    }
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

void TestSequenceWidget::refreshOverview() {
    if(!m_heading)return;
    const bool light=InstrumentVisuals::light(this);
    const QString state=overallState();
    const int total=m_hasExternalProgress?m_globalTotal:m_steps.size();
    const int completed=m_hasExternalProgress?m_globalCompleted:completedSteps();
    const QString window=m_hasExternalProgress && !m_virtualMode?
        QStringLiteral(" · showing %1–%2").arg(m_windowStart+1).arg(m_windowStart+m_steps.size()):QString();
    const QString overview=QStringLiteral("%1 / %2  •  %3%4")
       .arg(completed).arg(total).arg(state,window);
    if(m_overview->text()!=overview)m_overview->setText(overview);
    const QString tint=statusColor(state==QStringLiteral("FAILED")?QStringLiteral("fail"):
        state==QStringLiteral("ERROR")?QStringLiteral("error"):
        state==QStringLiteral("RUNNING")?QStringLiteral("running"):
        state==QStringLiteral("COMPLETE")?QStringLiteral("pass"):
        QStringLiteral("pending"),light).name();
    const QString style=QStringLiteral("color:%1;").arg(tint);
    if(m_overview->styleSheet()!=style)m_overview->setStyleSheet(style);
    const int percent=percentComplete();
    if(m_progress->value()!=percent)m_progress->setValue(percent);
}

void TestSequenceWidget::refreshRow(int i) {
    if(i<0 || i>=m_rows.size() || i>=m_steps.size())return;
    const Step& s=m_steps.at(i);
    const RowWidgets& w=m_rows.at(i);
    const bool light=InstrumentVisuals::light(this);
    const QString muted=light?QStringLiteral("#647b8c"):QStringLiteral("#97afc4");
    const QString fg=light?QStringLiteral("#23384c"):QStringLiteral("#e0ebf5");
    const QString tint=statusColor(s.status,light).name();
    const QString icon=symbolFor(s.status);
    if(w.icon->text()!=icon)w.icon->setText(icon);
    const QString iconStyle=QStringLiteral("border:0;color:%1;font-weight:bold;font-size:15px;").arg(tint);
    if(w.icon->styleSheet()!=iconStyle)w.icon->setStyleSheet(iconStyle);
    if(w.name->text()!=s.name)w.name->setText(s.name);
    const QString nameStyle=QStringLiteral("border:0;color:%1;font-weight:600;").arg(fg);
    if(w.name->styleSheet()!=nameStyle)w.name->setStyleSheet(nameStyle);
    const QString notes=!s.message.isEmpty()?s.message:s.description;
    if(w.detail->text()!=notes)w.detail->setText(notes);
    w.detail->setVisible(!m_compactMode && !notes.isEmpty());
    const QString detailStyle=QStringLiteral("border:0;color:%1;font-size:11px;").arg(muted);
    if(w.detail->styleSheet()!=detailStyle)w.detail->setStyleSheet(detailStyle);
    const QString elapsed=timeText(s.elapsedMs);
    if(w.elapsed->text()!=elapsed)w.elapsed->setText(elapsed);
    w.elapsed->setVisible(m_showElapsed && s.elapsedMs>=0);
    const QString status=s.status.toUpper();
    if(w.status->text()!=status)w.status->setText(status);
    const QString statusStyle=QStringLiteral("border:0;color:%1;font-weight:bold;").arg(tint);
    if(w.status->styleSheet()!=statusStyle)w.status->setStyleSheet(statusStyle);
}

void TestSequenceWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if(m_virtualMode) {m_rowsDirty=false;refreshOverview();return;}
    if(!m_rowsDirty)return;
    m_rowsDirty=false;
    for(int i=0;i<m_steps.size();++i)refreshRow(i);
    refreshOverview();
    if(m_autoScroll) {
        for(int i=0;i<m_steps.size();++i)
            if(m_steps.at(i).status==QStringLiteral("running") && i<m_rows.size()) {
                m_scroll->ensureWidgetVisible(m_rows.at(i).frame,0,20);
                break;
            }
    }
}

void TestSequenceWidget::applyTheme() {
    const bool light=InstrumentVisuals::light(this);
    const QString fg=light?QStringLiteral("#23384c"):QStringLiteral("#e0ebf5");
    const QString muted=light?QStringLiteral("#647b8c"):QStringLiteral("#97afc4");
    const QString background=light?QStringLiteral("#edf4fa"):QStringLiteral("#101e2f");
    const QString rowBg=light?QStringLiteral("#e3edf5"):QStringLiteral("#172b3e");
    const QString border=light?QStringLiteral("#c1d0db"):QStringLiteral("#3a536b");
    m_heading->setText(m_title.isEmpty()?QStringLiteral("Test Sequence"):m_title);
    m_heading->setStyleSheet(QStringLiteral("color:%1;").arg(fg));
    m_progress->setVisible(m_showProgress);
    m_progress->setFormat(QStringLiteral("%p%"));
    m_progress->setStyleSheet(QStringLiteral(
        "QProgressBar{border:1px solid %1;border-radius:5px;background:%2;color:%3;text-align:center;}"
        "QProgressBar::chunk{background:#30b98b;border-radius:4px;}").arg(border,background,fg));
    setStyleSheet(QStringLiteral("QWidget#wfTestSequenceRoot{background:%1;border:1px solid %2;border-radius:9px;}")
        .arg(background,border));
    refreshOverview();
    m_virtualView->setStyleSheet(light?QStringLiteral(
        "QTableView{background:#f3f8fc;color:#253b50;alternate-background-color:#e4edf5;border:1px solid #b8cbd9;}"
        "QHeaderView::section{background:#d8e7f1;color:#253b50;border:0;padding:5px;}"):
        QStringLiteral(
        "QTableView{background:#101e2f;color:#e0ebf5;alternate-background-color:#172b3e;border:1px solid #3a536b;}"
        "QHeaderView::section{background:#243a4e;color:#d5e4f2;border:0;padding:5px;}"));
    static_cast<VirtualStepModel*>(m_virtualModel)->setLight(light);
    // Up to 100 small card rows; never rebuild or reallocate them for a theme.
    if(!m_virtualMode)for(int i=0;i<m_rows.size();++i) {
        const auto& row=m_rows.at(i);
        if(row.frame)row.frame->setStyleSheet(QStringLiteral(
            "QFrame{background:%1;border:1px solid %2;border-radius:5px;}").arg(rowBg,border));
        if(row.order)row.order->setStyleSheet(QStringLiteral("border:0;color:%1;").arg(muted));
        if(row.elapsed)row.elapsed->setStyleSheet(QStringLiteral("border:0;color:%1;").arg(muted));
        refreshRow(i);
    }
    if(m_virtualView)m_virtualView->viewport()->update();
}
void TestSequenceWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if(event->type()==QEvent::ThemeChange)applyTheme();
}

void TestSequenceWidget::rebuild() {
    if(!m_heading || !m_scroll)return;
    applyTheme();
    // Small-card layout is rebuilt only when its sequence definition changes.
    // The same appearance colors are refreshed in-place by applyTheme().
    const bool light=InstrumentVisuals::light(this);
    const QString rowBg=light?QStringLiteral("#e3edf5"):QStringLiteral("#172b3e");
    const QString border=light?QStringLiteral("#c1d0db"):QStringLiteral("#3a536b");
    const QString muted=light?QStringLiteral("#647b8c"):QStringLiteral("#97afc4");

    m_scroll->setVisible(!m_virtualMode);
    m_virtualView->setVisible(m_virtualMode);
    m_followButton->setVisible(m_virtualMode);
    if(m_virtualMode) {
        static_cast<VirtualStepModel*>(m_virtualModel)->resetAll();
        m_followButton->setText(m_followLive?QStringLiteral("Following live"):QStringLiteral("Follow live"));
        if(m_followLive)followCurrent();
        return;
    }
    const int oldScroll=m_scroll->verticalScrollBar()->value();
    auto* content=new QWidget;
    content->setObjectName(QStringLiteral("sequenceRows"));
    content->setStyleSheet(QStringLiteral("background:transparent;"));
    auto* list=new QVBoxLayout(content);
    list->setContentsMargins(0,0,0,0);
    list->setSpacing(m_compactMode?3:6);
    QVector<RowWidgets> rows;
    rows.reserve(m_steps.size());
    int currentRow=-1;
    for(int i=0;i<m_steps.size();++i) {
        const Step& s=m_steps.at(i);
        if(s.status==QStringLiteral("running"))currentRow=i;
        RowWidgets w;
        w.frame=new QFrame(content);
        w.frame->setObjectName(QStringLiteral("sequenceStep_%1").arg(s.id));
        w.frame->setStyleSheet(QStringLiteral("QFrame{background:%1;border:1px solid %2;border-radius:5px;}")
            .arg(rowBg,border));
        auto* h=new QHBoxLayout(w.frame);
        h->setContentsMargins(8,m_compactMode?4:7,8,m_compactMode?4:7);
        h->setSpacing(9);
        w.icon=new QLabel(w.frame);
        w.icon->setObjectName(QStringLiteral("sequenceIcon_%1").arg(s.id));
        w.icon->setFixedWidth(21);w.icon->setAlignment(Qt::AlignCenter);
        h->addWidget(w.icon);
        w.order=new QLabel(QString::number(i+1+m_windowStart),w.frame);
        w.order->setFixedWidth(19);w.order->setAlignment(Qt::AlignCenter);
        w.order->setStyleSheet(QStringLiteral("border:0;color:%1;").arg(muted));
        h->addWidget(w.order);
        w.order->setVisible(m_showNumbers);
        auto* body=new QVBoxLayout;
        body->setSpacing(2);
        w.name=new QLabel(w.frame);
        w.name->setObjectName(QStringLiteral("sequenceName_%1").arg(s.id));
        w.name->setWordWrap(true);
        body->addWidget(w.name);
        w.detail=new QLabel(w.frame);
        w.detail->setWordWrap(true);
        body->addWidget(w.detail);
        h->addLayout(body,1);
        w.elapsed=new QLabel(w.frame);
        w.elapsed->setStyleSheet(QStringLiteral("border:0;color:%1;").arg(muted));
        h->addWidget(w.elapsed);
        w.status=new QLabel(w.frame);
        w.status->setObjectName(QStringLiteral("sequenceStatus_%1").arg(s.id));
        w.status->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        w.status->setMinimumWidth(61);
        h->addWidget(w.status);
        list->addWidget(w.frame);
        rows.push_back(w);
    }
    if(m_steps.isEmpty()) {
        auto* hint=new QLabel(QStringLiteral("No test steps configured.\nSet steps through the Property Inspector or ui.set()."),content);
        hint->setAlignment(Qt::AlignCenter);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color:%1;").arg(muted));
        list->addWidget(hint);
    }
    list->addStretch(1);
    m_rows.clear(); // previous row pointers are invalid after setWidget()
    m_scroll->setWidget(content); // full replace only for a new definition/configuration
    m_rows=std::move(rows);
    for(int i=0;i<m_steps.size();++i)refreshRow(i);
    m_rowsDirty=false;
    if(m_autoScroll && currentRow>=0 && isVisible())
        m_scroll->ensureWidgetVisible(m_rows.at(currentRow).frame,0,20);
    else m_scroll->verticalScrollBar()->setValue(oldScroll);
}

void TestSequenceWidget::followCurrent() {
    if(!m_virtualMode||m_steps.isEmpty())return;
    const int row=m_runningIndex>=0?m_runningIndex:
        (m_lastCompletedIndex>=0?m_lastCompletedIndex:0);
    m_programScroll=true;
    m_virtualView->scrollTo(m_virtualModel->index(row,0),QAbstractItemView::PositionAtCenter);
    m_programScroll=false;
    m_followButton->setText(QStringLiteral("Following live"));
}

bool TestSequenceWidget::eventFilter(QObject* watched,QEvent* event) {
    if(m_virtualMode && !m_programScroll &&
       (watched==m_virtualView->viewport() || watched==m_virtualView ||
        watched==m_virtualView->verticalScrollBar()) &&
       (event->type()==QEvent::Wheel || event->type()==QEvent::KeyPress)) {
        m_followLive=false;
        m_followButton->setText(QStringLiteral("Follow live"));
    }
    return QWidget::eventFilter(watched,event);
}
