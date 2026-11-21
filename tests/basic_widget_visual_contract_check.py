#!/usr/bin/env python3
"""Offline invariants for v0.2-dev.8.3 when Qt6 cannot compile locally.

Source checks supplement, but do not replace, WFScriptBasicWidgetVisualSmoke.
"""
from pathlib import Path
from zipfile import ZipFile
import json

root=Path(__file__).resolve().parents[1]
assert 'WFScriptBasicWidgetVisualSmoke' in (root/'CMakeLists.txt').read_text()
builtin=(root/'src/widgets/BuiltinWidgets.cpp').read_text()
label=(root/'src/widgets/PanelLabelWidget.cpp').read_text()
gauge=(root/'src/widgets/PanelGauge.cpp').read_text()
assert 'ResponsiveNumericReadout' in builtin
slider=(root/'src/widgets/PanelSliderWidget.cpp').read_text()
slider_h=(root/'src/widgets/PanelSliderWidget.h').read_text()
assert 'class PanelSliderWidget final : public QWidget' in slider_h
assert 'new PanelSliderWidget(parent)' in builtin
assert 'qobject_cast<QSlider*>(w)' not in builtin
assert 'm_title = definition.text' in slider
assert 'm_showRange = definition.properties.value' in slider
assert 'm_showValue = definition.properties.value' in slider
assert 'QSignalBlocker block(m_slider)' in slider
assert 'InstrumentVisuals::background(painter' in slider
assert 'slider->configure(d);' in builtin
assert 'slider->setRuntimeValue(value.toDouble());' in builtin
assert 'new QPushButton(d.text,parent)' in builtin
assert 'QFrame::StyledPanel' not in builtin
assert 'InstrumentVisuals::background(painter,QRectF(rect()),9,this)' in builtin
assert 'class ResponsivePushButton' not in builtin and 'class ResponsiveToggle' in builtin
assert 'new QPushButton(d.text,parent)' in builtin
assert 'new ResponsiveNumericReadout' in builtin
assert 'void PanelLabelWidget::resizeEvent' not in label
assert 'void PanelLabelWidget::refreshTypography' not in label
assert 'BasicWidgetVisuals::fit(' not in label
assert 'setTextFormat(qtFormat)' in label and 'setWordWrap(wrap)' in label
assert 'QLabel::paintEvent(event)' in label
assert 'InstrumentVisuals::light(this)' in label
assert 'd.properties.insert("cornerRadius",8)' not in builtin
assert 'InstrumentVisuals::background(p,QRectF(rect()),10,this)' in gauge
assert 'QPalette::WindowText' not in gauge
assert 'InstrumentVisuals::tone(this,QColor(232,245,252))' in gauge
qa=json.loads((root/'examples/qa/WidgetVisualReview.wfpanel').read_text())
assert qa['format']=='WFScriptPanel' and len(qa['pages'])==1
c={c['id']:c for c in qa['pages'][0]['controls']}
for kind in ('Button','Toggle','Label','Numeric','Gauge'):
    assert 'small'+kind in c and 'large'+kind in c
    assert c['small'+kind]['width']<c['large'+kind]['width']
assert not qa['panelData'] and not qa['libraries']
print('PASS: native smoke registered, Button/Label native typography + rich text, Toggle/Numeric/Gauge v8.2 styling preserved, one-Page QA')
