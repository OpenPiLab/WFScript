#!/usr/bin/env python3
"""Offline contracts for Option Selector and universal Property Editor fields.
Native behavior and live widget rendering are tested in the two Qt CTests.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
registry=(root/'src/widgets/WidgetRegistry.cpp').read_text()
inspector=(root/'src/widgets/WidgetPropertyInspector.cpp').read_text()
codec=(root/'src/widgets/WidgetDocumentCodec.cpp').read_text()
canvas=(root/'src/PanelCanvas.cpp').read_text()
combo=(root/'src/widgets/OptionSelectorWidget.cpp').read_text()
assert all(k in registry for k in ['"toolTip"', '"runEnabled"', '"runVisible"'])
assert 'WidgetRegistry::instance().properties(type)' in inspector
assert 'WidgetRegistry::instance().properties(d.type)' in codec
assert 'p.defaultValue' in inspector and 'd.properties.remove(p.id)' in inspector
assert 'applySharedWidgetProperties' in canvas
assert 'widget->setToolTip(' in canvas and 'widget->setEnabled(' in canvas
assert 'widget->setVisible(' in canvas and 'if(activePage)' in canvas
assert 'QComboBox::drop-down' in combo and 'QComboBox::down-arrow' in combo
assert 'background:transparent' in combo and 'padding-right:36px' in combo
assert 'class RoundedOptionCombo' in combo and 'QPainterPath arrow' in combo
assert 'QComboBox::paintEvent(event)' in combo
assert 'm_combo->setCurrentIndex' in combo and 'userSelectionChanged' in combo
basic=(root/'tests/basic_widget_visual_smoke.cpp').read_text()
properties=(root/'tests/widget_properties_smoke.cpp').read_text()
assert 'RoundedOptionCombo' not in basic and 'QComboBox::drop-down' in basic
assert 'for(const QString& type:registry.typeIds())' in properties
assert 'runVisible' in properties and 'setDesignMode(false)' in properties
assert 'WFScriptBasicWidgetVisualSmoke' in (root/'CMakeLists.txt').read_text()
assert 'WFScriptWidgetPropertiesSmoke' in (root/'CMakeLists.txt').read_text()
print('PASS: shared properties, Option Selector styling, native smoke coverage')
