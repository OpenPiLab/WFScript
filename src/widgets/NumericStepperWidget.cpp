#include "NumericStepperWidget.h"
#include "InstrumentVisuals.h"
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QFontMetricsF>
#include <cmath>
#include <utility>

NumericStepperWidget::NumericStepperWidget(QWidget* parent):QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent,false);
    setAutoFillBackground(false);
    m_spin=new QDoubleSpinBox(this);
    m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spin->setKeyboardTracking(false);
    m_spin->setAlignment(Qt::AlignRight);
    m_spin->setRange(0,100);
    m_spin->setDecimals(2);
    m_minus=new QPushButton(QString::fromUtf8("−"),this);
    m_plus=new QPushButton("+",this);
    m_minus->setFocusPolicy(Qt::NoFocus);
    m_plus->setFocusPolicy(Qt::NoFocus);
    m_minus->setAutoRepeat(true);m_plus->setAutoRepeat(true);
    m_minus->setAutoRepeatDelay(400);m_plus->setAutoRepeatDelay(400);
    m_minus->setAutoRepeatInterval(80);m_plus->setAutoRepeatInterval(80);
    connect(m_minus,&QPushButton::clicked,m_spin,[this]{m_spin->stepDown();});
    connect(m_plus,&QPushButton::clicked,m_spin,[this]{m_spin->stepUp();});
    connect(m_spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double v){
        if(!m_programmatic)emit userValueChanged(v);
    });
    layoutChildren();
}
void NumericStepperWidget::configure(QString title,QString unit,QString accent,
      double v,double min,double max,double step,int decimals) {
    if(!std::isfinite(v)||!std::isfinite(min)||!std::isfinite(max)||
       !std::isfinite(step)||max<=min||step<=0)return;
    m_title=std::move(title);m_unit=std::move(unit);m_accent=std::move(accent);
    m_programmatic=true;
    const QSignalBlocker blocker(m_spin);
    m_spin->setDecimals(qBound(0,decimals,8));
    m_spin->setRange(min,max);
    m_spin->setSingleStep(step);
    m_spin->setValue(v);
    m_programmatic=false;
    layoutChildren();update();
}
void NumericStepperWidget::setValue(double v) {
    if(!std::isfinite(v))return;
    const QSignalBlocker blocker(m_spin);
    m_spin->setValue(v);
}
double NumericStepperWidget::value() const {return m_spin->value();}
double NumericStepperWidget::singleStep() const {return m_spin->singleStep();}
void NumericStepperWidget::applyPalette() {
    const QString accent=InstrumentVisuals::accent(m_accent).name();
    // Explicit font sizes in the stylesheet prevent Qt's style re-polish
    // during hover/focus from resetting the spin editor to a tiny default.
    const bool light=InstrumentVisuals::light(this);
    const QString foreground=light?QStringLiteral("#203d56"):QStringLiteral("#eef7ff");
    const QString editorBg=light?QStringLiteral("#ffffff"):QStringLiteral("#112538");
    const QString buttonBg=light?QStringLiteral("#d9e6f1"):QStringLiteral("#29445b");
    const QString buttonBorder=light?QStringLiteral("#8eafc6"):QStringLiteral("#54728a");
    const QString style=QStringLiteral(
        "QDoubleSpinBox { font-family: 'Consolas'; font-size:%2px; font-weight:600; "
        "color:%3; background:%4; border:1px solid %1; "
        "border-radius:7px; padding:2px 8px; selection-background-color:%1; }"
        "QDoubleSpinBox QLineEdit { font-family: 'Consolas'; font-size:%2px; "
        "font-weight:600; background:transparent; border:none; }"
        "QPushButton {color:%3; background:%5; border:1px solid %6; "
        "border-radius:8px;} QPushButton:pressed {background:%1;color:#081721;}"
    ).arg(accent).arg(m_editorFontPx).arg(foreground,editorBg,buttonBg,buttonBorder);
    m_spin->setStyleSheet(style);
    m_minus->setStyleSheet(style);
    m_plus->setStyleSheet(style);
    m_styledFontPx=m_editorFontPx;
    m_styledAccent=m_accent;
}
void NumericStepperWidget::applyEditorFont() {
    // Size for the *widest possible* formatted value, not the current text.
    // This avoids the number jumping in size during editing or value changes.
    const int decimals=m_spin->decimals();
    const QString minimum=QString::number(m_spin->minimum(),'f',decimals);
    const QString maximum=QString::number(m_spin->maximum(),'f',decimals);
    const QString widest=minimum.size()>=maximum.size()?minimum:maximum;
    const int available=qMax(20,m_spin->width()-22);
    const int ideal=qBound(12,qRound(m_spin->height()*0.57),54);
    QFont editor=InstrumentVisuals::labelFont(font(),ideal,true);
    editor.setFamily(QStringLiteral("Consolas"));
    editor.setStyleHint(QFont::Monospace);
    for(int px=ideal;px>=9;--px) {
        editor.setPixelSize(px);
        if(QFontMetricsF(editor).horizontalAdvance(widest)<=available)break;
    }
    m_editorFontPx=editor.pixelSize();
    if(m_styledFontPx!=m_editorFontPx || m_styledAccent!=m_accent)
        applyPalette();
    // QDoubleSpinBox embeds a QLineEdit. Set both fonts so entering/leaving
    // the child editor never changes the readout typography.
    m_spin->setFont(editor);
    if(auto* edit=m_spin->findChild<QLineEdit*>())edit->setFont(editor);
}
void NumericStepperWidget::layoutChildren() {
    const int w=width(),h=height();
    const int pad=qMax(3,qRound(qMin(w/320.0,h/118.0)*12));
    const int top=qMax(15,qRound(h*0.34));
    const int fieldHeight=qMax(22,h-top-pad);
    const int buttonWidth=qBound(25,qRound(w*.145),qMax(25,w/3));
    const int gap=qMax(3,pad/2);
    const int spinWidth=qMax(30,w-2*pad-2*buttonWidth-2*gap);
    m_spin->setGeometry(pad,top,spinWidth,fieldHeight);
    m_minus->setGeometry(pad+spinWidth+gap,top,buttonWidth,fieldHeight);
    m_plus->setGeometry(pad+spinWidth+gap+buttonWidth+gap,top,buttonWidth,fieldHeight);
    applyEditorFont();
    const int buttonPx=qBound(12,qRound(fieldHeight*0.5),42);
    m_minus->setFont(InstrumentVisuals::labelFont(font(),buttonPx,true));
    m_plus->setFont(m_minus->font());
}
void NumericStepperWidget::resizeEvent(QResizeEvent* e) {QWidget::resizeEvent(e);layoutChildren();}
void NumericStepperWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if(event->type()==QEvent::ThemeChange && m_spin)applyPalette();
}
void NumericStepperWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing,true);
    InstrumentVisuals::background(p,QRectF(0,0,width(),height()),12,this);
    const qreal factor=qMin(width()/320.,height()/118.);
    const int labelPx=qBound(7,qRound(factor*15),25);
    const QFont label=InstrumentVisuals::labelFont(font(),labelPx,true);
    p.setFont(label);p.setPen(InstrumentVisuals::tone(this, QColor(195,216,229)));
    const QRectF heading(12,6,qMax(20,width()-24),qMax(12.0,height()*0.25));
    const QString title=m_title+(m_unit.isEmpty()?QString():QStringLiteral("   [%1]").arg(m_unit));
    p.drawText(heading,Qt::AlignVCenter|Qt::AlignLeft,
               InstrumentVisuals::elide(label,title,heading.width()));
}
