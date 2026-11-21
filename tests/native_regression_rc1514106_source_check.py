#!/usr/bin/env python3
"""Offline source contracts for known RC1.5.14.10.5 Windows CTest failures.

These checks guard revision scope and compatibility. They are not replacements
for running the 116 Qt-native Windows executables.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def source(path):return (ROOT/path).read_text(encoding='utf-8')
checks={
 'src/ScriptUtilityBridge.cpp':['QJSValue ScriptUtilityBridge::readCsvArray','ScriptValueConversion::toJavaScript'],
 'src/ScriptUtilityBridge.h':['Q_INVOKABLE QJSValue readCsvArray'],
 'src/PanelRuntime.cpp':['__fileHost.readCsvArray(path)'],
 'src/CompletionCatalog.cpp':['"setNativeSamples","nativeSeriesInfo"','"getView","setView","onViewChanged"'],
 'src/plot/PanelChartWidget.cpp':['case PlotCommand::Action::StyleSeries:','s.digitalGeometry.valid=false; // color/width are baked into cached raster'],
 'src/SqliteBridge.cpp':['QVariantList SqliteBridge::sqlParameters','item.isNull()?QVariant(QMetaType::fromType<QString>())','{QStringLiteral("params"),sqlParameters(params)}'],
 'tests/PanelSchemaTestEvents.h':['"numeric_stepper"','"option_selector"','"switch_button"'],
 'tests/ScriptUtilitySmoke.cpp':['fileHost.readCsvArray(csvPath)'],
 'tests/test_runner_cpp_smoke.cpp':['lastCompleteSequence.size()==4'],
 'tests/panel_data_smoke.cpp':['"readyInput","toggle"','status beacon is a read-only boolean display'],
 'tests/digital_chart_performance_smoke.cpp':['rasterBuilds'],
 'tests/test_conditional_rc1514_smoke.cpp':['actuallySkipped>=2','skippedSteps']
}
for path,fragments in checks.items():
 text=source(path)
 for fragment in fragments:assert fragment in text,(path,fragment)
for path in ('tests/dsp_modules_smoke.cpp','tests/ac_network_lab_smoke.cpp','tests/rc_step_lab_smoke.cpp'):
 assert 'registerPanelSchemaTestEvents();' in source(path),path
for path in ('tests/test_sequence_editor_rc1511_smoke.cpp','tests/test_sequence_editor_rc1512_smoke.cpp'):
 text=source(path)
 assert text.index('editor.show();')<text.index('setText(QStringLiteral("-0.5"))'),path
assert 'Q_INVOKABLE QVariantList readCsv' in source('src/ScriptUtilityBridge.h'), 'C++ CSV preserved'
assert 'm_viewActive=false' not in source('src/plot/PanelChartWidget.cpp').split('case PlotCommand::Action::StyleSeries:')[1].split('case PlotCommand::Action::SetView:')[0]
assert len(list((ROOT/'tests').glob('*.cpp')))>=115
print('PASS RC1.5.14.10.6: native regression source contracts; Windows CTests remain to be run')
