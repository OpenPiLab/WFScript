#!/usr/bin/env python3
"""Check files referenced by CMake/Qt resources and embedded examples.

No Qt or hardware required; useful before git commit or GitHub upload.
"""
from __future__ import annotations
from pathlib import Path
import json
import re
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
errors=[]

def require(rel):
    if not (ROOT/rel).is_file(): errors.append('missing: '+str(rel))

require('CMakeLists.txt')
# Public release artwork and Windows executable identity must stay packaged.
for icon_asset in ('assets/wfscript-icon.png', 'src/resources/icons/app_icon.png',
                   'src/resources/icons/app_icon.ico', 'src/resources/wfscript.rc',
                   'LICENSE', 'docs/SCRIPT_REFERENCE.md', 'docs/QT_OPEN_SOURCE_NOTES.md'):
    require(icon_asset)

# C++ string syntax guard: catches unescaped newlines before MinGW Build All.
from check_cpp_test_literals import check as check_native_test_literals
errors.extend(check_native_test_literals(ROOT / 'tests'))

# Qt 6.9: incomplete-type return values require explicit direct includes.
from check_qt_test_type_includes import check as check_native_qt_includes
errors.extend(check_native_qt_includes(ROOT / 'tests'))

# v5.7.0.2: detect the exact native-test C++ issues exposed by GCC/MinGW.
qt_api_test = ROOT / 'tests/qt_api_smoke.cpp'
if qt_api_test.is_file():
    qt_api_source = qt_api_test.read_text(encoding='utf-8')
    if 'for(auto* v : {static_cast<QObject*>(mod)' in qt_api_source:
        errors.append('qt_api_smoke.cpp: mixed QObject-derived auto* initializer list (MinGW compile failure)')
    if 'QObject* nestedObjects[]' not in qt_api_source:
        errors.append('qt_api_smoke.cpp: missing explicitly typed QObject* ownership array')
for cpp_name in ('completion_popup_smoke.cpp', 'smart_editing_smoke.cpp'):
    text_block_test = ROOT / 'tests' / cpp_name
    if text_block_test.is_file():
        smoke_src = text_block_test.read_text(encoding='utf-8')
        if '.block().text()' in smoke_src and '#include <QTextBlock>' not in smoke_src:
            errors.append(f'{cpp_name}: QTextBlock used without an explicit include')


# Qt string conversion overloads take byte sequences, not an existing QString.
# The widget-registry test iterates `const QString& required`, so wrapping it in
# QString::fromLatin1 caused a real Qt 6.9/MinGW compilation error.
registry_smoke = ROOT / 'tests' / 'widget_registry_smoke.cpp'
if registry_smoke.is_file():
    registry_source = registry_smoke.read_text(encoding='utf-8')
    if re.search(r'QString::from(?:Latin1|Utf8|Local8Bit)\s*\(\s*required\s*\)',registry_source):
        errors.append('widget_registry_smoke.cpp: required is already QString; remove QString::fromLatin1 wrapper')
    if 'registry.typeIds().contains(required)' not in registry_source:
        errors.append('widget_registry_smoke.cpp: missing direct widget type membership assertion')

# Qt 6.9 requires a Q_OBJECT macro in typed findChild<T*> / qobject_cast<T*>.
# Measurement display widgets intentionally omit it, so tests must find a
# QWidget* first and use dynamic_cast to verify the concrete subclass.
without_qobject = ('MeasurementResultWidget','MeasurementTableWidget','TestSequenceWidget',
                   'PanelImageWidget','PanelInstructionWidget','PanelEquationWidget',
                   'ExperimentDataTableWidget')
for test_cpp in sorted((ROOT/'tests').glob('*.cpp')):
    test_source = test_cpp.read_text(encoding='utf-8')
    for method in ('findChild', 'findChildren', 'qobject_cast'):
        for widget_type in without_qobject:
            pattern = rf'\b{method}\s*<\s*{widget_type}\s*\*\s*>'
            if re.search(pattern, test_source):
                errors.append(f'{test_cpp.name}: {method}<{widget_type}*> requires Q_OBJECT; use QWidget* + dynamic_cast')

# Native PanelRuntime link/AUTOMOC dependencies must be present in every
# corresponding smoke-test target; otherwise MinGW reports undefined references.
from check_panel_runtime_smoke_links import check as check_panel_runtime_smoke_links
errors.extend(check_panel_runtime_smoke_links())

# Editor completion must accept a selected popup entry when Return/Tab is
# delivered to the editor instead of its QCompleter event filter.
from check_editor_completion_fallback import check as check_editor_completion_fallback
errors.extend(check_editor_completion_fallback())

# Test Executive must expose real JS collections while retaining typed native
# data for archiving; Results History must count valid reports, not all JSON.
from check_test_executive_contract import check as check_executive_contract
errors.extend(check_executive_contract(ROOT))

# Guard current CTest fixtures against stale assumptions about the native API.
from check_runner_smoke_contract import check as check_runner_smoke_contract
errors.extend(check_runner_smoke_contract(ROOT))

# v5.7.0.11: keep current native fixtures and failure-stage diagnostics.
from check_ctest_5711_fixtures import check as check_5711_fixtures
errors.extend(check_5711_fixtures(ROOT))

# v5.7.0.12: guard the six diagnosed smoke-test failures and the lexer fix.
from check_ctest_5712_fixes import check as check_5712_fixes
errors.extend(check_5712_fixes(ROOT))

# Qt Creator uses root CMakePresets.json entries as kits/configurations.
# The v5.7.0 presets omitted compiler and Qt paths, creating unusable pseudo-kits.
# The public Windows-first repo intentionally uses normal Qt Creator kits.
for preset_name in ('CMakePresets.json', 'CMakeUserPresets.json'):
    if (ROOT/preset_name).exists():
        errors.append(f'Unexpected root {preset_name}: verify Qt Creator kit import before release')

c=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
for rel in sorted(set(re.findall(r'(?:src|tests)/[A-Za-z0-9_./-]+\.(?:h|cpp|qrc)',c))): require(rel)
for rel in ('src/resources/icons.qrc','src/resources/examples.qrc'):
    f=ROOT/rel
    for e in ET.parse(f).iter('file'):
        target=(f.parent/(e.text or '')).resolve()
        if not target.is_file(): errors.append(f'{rel}: missing resource {e.text}')

catalog=json.loads((ROOT/'examples/catalog.json').read_text(encoding='utf-8'))
if len(catalog)!=22:errors.append(f'expected 22 examples; found {len(catalog)}')
expected={'Getting Started','Graphics & Charts','Tables & Data','Analog Instruments','Digital Instruments','Test Sequences & Executive'}
found={e['category'] for e in catalog}
if found!=expected:errors.append(f'categories differ: {found ^ expected}')
seen=set()
for entry in catalog:
    id=entry['file']
    if id in seen:errors.append(f'duplicate example {id}')
    seen.add(id)
    source=ROOT/'examples/source'/f'{id}.js'
    panel=ROOT/'examples/panels'/f'{id}.wfpanel'
    if not source.is_file() or not panel.is_file(): errors.append(f'example missing: {id}'); continue
    obj=json.loads(panel.read_text(encoding='utf-8'))
    if obj.get('format')!='WFScriptPanel':errors.append(f'{id}: wrong Panel format')
    if obj.get('script')!=source.read_text(encoding='utf-8'):errors.append(f'{id}: script differs')
    if not obj.get('pages'):errors.append(f'{id}: no Pages')

# Check all production files remain present (not just referenced in CMake).
if len(list((ROOT/'src').rglob('*.*'))) < 180:errors.append('Unexpectedly few src/ files; check release packaging')
if errors:
    for error in errors:print('FAIL:',error,file=sys.stderr)
    sys.exit(1)
print('PASS: CMake paths, QRC paths, 22 embedded examples, six menu groups, JS synchronization')
