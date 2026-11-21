#!/usr/bin/env python3
"""Source and .wfpanel invariants for native Slider v0.2-dev.8.7.

Requires no Qt SDK; the actual QWidget paint/events are covered by the expanded
WFScriptBasicWidgetVisualSmoke CTest (Windows Qt 6.9.2).
"""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
source = (root/'src/widgets/BuiltinWidgets.cpp').read_text()
header = (root/'src/widgets/PanelSliderWidget.h').read_text()
widget = (root/'src/widgets/PanelSliderWidget.cpp').read_text()
canvas = (root/'src/PanelCanvas.cpp').read_text()
cmake = (root/'CMakeLists.txt').read_text()
assert 'class PanelSliderWidget final : public QWidget' in header
assert 'new PanelSliderWidget(parent)' in source
assert 'new QSlider(Qt::Horizontal,parent)' not in source
assert 'src/widgets/PanelSliderWidget.cpp' in cmake
assert 'QSlider::valueChanged' in source and 'sink("valueChanged",wrapped->value())' in source
assert 'm_slider->setRange(0, 1000)' in widget
assert 'QSignalBlocker block(m_slider)' in widget
assert 'm_title = definition.text' in widget
assert 'showValue' in widget and 'showRange' in widget
assert 'InstrumentVisuals::background(painter' in widget
assert 'InstrumentVisuals::tone(this' in widget
assert 'slider->setRuntimeValue(value.toDouble())' in source
assert 'slider->configure(d)' in source
assert 'slider->value()' in source
assert 'setProperty("wfPanelLight",light)' in canvas
assert 'findChildren<QWidget*>()' in canvas # designer child event handling
assert 'new QPushButton(d.text,parent)' in source
assert 'new PanelLabelWidget(d.text,parent)' in source
assert 'class ResponsivePushButton' not in source
qa=json.loads((root/'examples/qa/SliderVisualReview_v8_7.wfpanel').read_text())
assert qa['format']=='WFScriptPanel' and len(qa['pages'])==1
controls={c['id']:c for c in qa['pages'][0]['controls']}
assert controls['harmonics']['text']=='3rd HARMONIC (%)'
assert controls['noise']['text']=='NOISE FLOOR (%)'
assert controls['compact']['height']==38
assert controls['signed']['min']==-500
assert controls['noValue']['properties']=={'showValue':False,'showRange':False}
assert controls['readout']['properties']['textFormat']=='Rich Text'
dsp=json.loads((root/'examples/panels/JavaScriptDSPStudio.wfpanel').read_text())
found={c['id']:c for p in dsp['pages'] for c in p['controls']}
assert found['harmonics']['text'].startswith('3rd HARMONIC')
assert found['noise']['text'].startswith('NOISE FLOOR')
assert 'WFScriptBasicWidgetVisualSmoke' in cmake
print('PASS: titled Slider composite, reactive numeric display, properties, old DSP Panel compatibility, and native smoke registration')
