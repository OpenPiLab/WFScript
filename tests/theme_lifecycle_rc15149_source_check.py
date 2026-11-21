from pathlib import Path
import json,re
r=Path(__file__).resolve().parents[1]
root=(r/'src/PanelCanvas.cpp').read_text(encoding='utf-8')
assert 'QApplication::sendEvent(widget,&themeChanged);' in root
assert 'QApplication::sendEvent(widget,&themeChanged);' in root
main=(r/'src/MainWindow.cpp').read_text(encoding='utf-8')
assert 'qApp->setPalette(p);' in main
assert 'appearance/darkTheme' in main
assert 'applyApplicationTheme();' in main
for name in ['MeasurementTableWidget','EventLogWidget','ExperimentWidgets','TestSequenceWidget']:
 s=(r/'src/widgets'/f'{name}.cpp').read_text(encoding='utf-8')
 assert 'QEvent::ThemeChange' in s
 assert 'applyTheme()' in s
 if name!='ExperimentWidgets':
  change=s.split('::changeEvent(',1)[1].split('}',1)[0]
  assert 'rebuild(' not in change and 'configure(' not in change and 'resetAll(' not in change
 # Confirm dedicated style-only path rather than calling configure() after change
 assert 'if(event->type()==QEvent::ThemeChange)applyTheme();' in s
check=(r/'src/widgets/PanelThemedCheckBox.h').read_text(encoding='utf-8')
assert 'QStyle::SE_CheckBoxIndicator' in check and 'QPainterPath tick' in check
assert 'class ResponsiveToggle final : public PanelThemedCheckBox' in (r/'src/widgets/BuiltinWidgets.cpp').read_text(encoding='utf-8')
assert 'new PanelThemedCheckBox' in (r/'src/widgets/RunInformationWidget.cpp').read_text(encoding='utf-8')
assert 'new PanelThemedCheckBox' in (r/'src/widgets/ExperimentWidgets.cpp').read_text(encoding='utf-8')
assert 'WFScriptPanelThemeLifecycleRC15149Smoke' in (r/'CMakeLists.txt').read_text(encoding='utf-8')
for f in ['PanelRuntime.cpp','TestRunnerBridge.cpp','DwfRuntime.cpp']:
 assert (r/'src'/f).is_file()
panel=json.loads((r/'examples/panels/ThemeLifecycleRC15149.wfpanel').read_text(encoding='utf-8'))
assert panel['appearance']['themeMode']=='follow' and len(panel['pages'])==2
assert len({c['id'] for p in panel['pages'] for c in p['controls']})==sum(len(p['controls']) for p in panel['pages'])
print('PASS RC1.5.14.9 explicit theme delivery, data-preserving widgets, contrast, CMake and Panel schema')
