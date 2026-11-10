#include "OptionSelectorWidget.h"
#include "InstrumentVisuals.h"
#include <QComboBox>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QVariant>
#include <utility>

OptionSelectorWidget::OptionSelectorWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
    m_combo=new QComboBox(this);
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    connect(m_combo,QOverload<int>::of(&QComboBox::activated),this,[this](int index){
        emit userIndexChanged(index);
        // Keep the script-visible ui.get() value as the selected string.
        emit userSelectionChanged(m_combo->itemText(index));
    });
    updateLayout();
}
void OptionSelectorWidget::configure(QString title,QString accent,QStringList items,int index) {
    m_title=std::move(title);m_accent=std::move(accent);
    if(items.isEmpty())items={"Option 1"};
    const QSignalBlocker blocker(m_combo);
    m_combo->clear();m_combo->addItems(items);
    m_combo->setCurrentIndex(qBound(0,index,items.size()-1));
    updateLayout();update();
}
void OptionSelectorWidget::setSelection(const QVariant& selection) {
    int idx=-1;
    if(selection.metaType().id()==QMetaType::QString)
        idx=m_combo->findText(selection.toString(),Qt::MatchFixedString);
    else {
        bool ok=false;const int i=selection.toInt(&ok);
        if(ok && i>=0 && i<m_combo->count())idx=i;
    }
    if(idx<0)return;
    const QSignalBlocker blocker(m_combo);
    m_combo->setCurrentIndex(idx);
}
int OptionSelectorWidget::currentIndex() const {return m_combo->currentIndex();}
QString OptionSelectorWidget::currentText() const {return m_combo->currentText();}
QStringList OptionSelectorWidget::options() const {
    QStringList v;for(int i=0;i<m_combo->count();++i)v.append(m_combo->itemText(i));return v;
}
void OptionSelectorWidget::updateLayout() {
    const int w=width(),h=height();
    const int pad=qMax(5,qRound(qMin(w/325.,h/112.)*13));
    const int top=qMax(19,qRound(h*.37));
    m_combo->setGeometry(pad,top,qMax(20,w-2*pad),qMax(22,h-top-pad));
    const int fontSize=qBound(9,qRound(h*.21),32);
    m_combo->setFont(InstrumentVisuals::labelFont(font(),fontSize,true));
    const QColor a=InstrumentVisuals::accent(m_accent);
    const bool light=InstrumentVisuals::light(this);
    const QString text=light?QStringLiteral("#19364a"):QStringLiteral("#eef7ff");
    const QString background=light?QStringLiteral("#ffffff"):QStringLiteral("#142b40");
    const QString popup=light?QStringLiteral("#f4f8fc"):QStringLiteral("#16283b");
    const QString selected=light?QStringLiteral("#bddbe7"):QStringLiteral("#355a72");
    m_combo->setStyleSheet(QStringLiteral(
        "QComboBox { color:%2; background:%3; border:1px solid %1;"
        "border-radius:8px; padding-left:12px; padding-right:8px; }"
        "QComboBox QAbstractItemView {color:%2;background:%4;"
        "selection-background-color:%5;}").arg(a.name(),text,background,popup,selected));
}
void OptionSelectorWidget::resizeEvent(QResizeEvent* e) {QWidget::resizeEvent(e);updateLayout();}
void OptionSelectorWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if(event->type()==QEvent::ThemeChange && m_combo)updateLayout();
}
void OptionSelectorWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing,true);
    InstrumentVisuals::background(p,QRectF(0,0,width(),height()),12,this);
    const int px=qBound(8,qRound(qMin(width()/325.,height()/112.)*15),26);
    const QFont f=InstrumentVisuals::labelFont(font(),px,true);
    p.setFont(f);p.setPen(InstrumentVisuals::tone(this, QColor(198,218,232)));
    p.drawText(QRectF(13,6,width()-26,qMax(16.0,height()*0.27)),Qt::AlignVCenter,
               InstrumentVisuals::elide(f,m_title,width()-26));
}
