#pragma once

// The original Slider's 0..1000 Qt input is retained as a child of a
// styled, persistent Panel widget.  Only the presentation has changed:
// value mapping, user event contract, ui.set(), and Panel Data remain native.
#include "WidgetDefinition.h"
#include <QWidget>

class QSlider;
class QResizeEvent;
class QPaintEvent;

class PanelSliderWidget final : public QWidget {
public:
    explicit PanelSliderWidget(QWidget* parent = nullptr);
    void configure(const PanelControl& definition);
    void setRuntimeValue(double value); // signal-blocked; never a user event
    double value() const;
    QSlider* slider() const { return m_slider; }
    QString title() const { return m_title; }
    bool showsValue() const { return m_showValue; }
    bool showsRange() const { return m_showRange; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void positionSlider();
    static QString formatNumber(double number);
    QSlider* m_slider = nullptr;
    QString m_title;
    double m_minimum = 0;
    double m_maximum = 100;
    bool m_showValue = true;
    bool m_showRange = true;
};
