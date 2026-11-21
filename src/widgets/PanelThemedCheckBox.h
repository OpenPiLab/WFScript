#pragma once
// Unified, contrast-tested Panel check indicator. Qt/Windows themes sometimes
// draw an almost invisible checked state over a Panel stylesheet. Paint the
// small indicator explicitly while leaving text, focus and layout to Qt.
#include "InstrumentVisuals.h"
#include <QCheckBox>
#include <QStyleOptionButton>
#include <QStyle>
#include <QPainter>
#include <QPainterPath>
class PanelThemedCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    void paintEvent(QPaintEvent* event) override {
        QCheckBox::paintEvent(event);
        QStyleOptionButton option;
        initStyleOption(&option);
        const QRect indicator=style()->subElementRect(QStyle::SE_CheckBoxIndicator,&option,this);
        if(!indicator.isValid() || indicator.width()<6 || indicator.height()<6)return;
        const bool light=InstrumentVisuals::light(this);
        const bool active=isEnabled();
        const QColor surface=active?(light?QColor("#ffffff"):QColor("#24384d"))
                                     :(light?QColor("#e5e9ee"):QColor("#263240"));
        const QColor selected=active?(light?QColor("#146b9c"):QColor("#64d6ed"))
                                      :(light?QColor("#7b98a7"):QColor("#607d8a"));
        const QColor outline=light?QColor("#506778"):QColor("#c6d7e5");
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing,true);
        const QRectF box=QRectF(indicator).adjusted(1,1,-1,-1);
        painter.setPen(QPen(outline,1.35));
        painter.setBrush(checkState()==Qt::Checked?selected:surface);
        painter.drawRoundedRect(box,3,3);
        if(checkState()==Qt::Checked) {
            QPainterPath tick;
            tick.moveTo(box.left()+box.width()*.21,box.top()+box.height()*.51);
            tick.lineTo(box.left()+box.width()*.43,box.top()+box.height()*.72);
            tick.lineTo(box.left()+box.width()*.79,box.top()+box.height()*.29);
            painter.setPen(QPen(light?Qt::white:QColor("#102638"),qMax(2.0,box.width()*.14),
                                Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(tick);
        } else if(checkState()==Qt::PartiallyChecked) {
            painter.setPen(QPen(light?QColor("#146b9c"):QColor("#64d6ed"),2));
            painter.drawLine(QPointF(box.left()+box.width()*.25,box.center().y()),
                             QPointF(box.right()-box.width()*.25,box.center().y()));
        }
    }
};
