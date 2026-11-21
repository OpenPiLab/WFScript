#!/usr/bin/env python3
"""Source-only WFScript v0.2 release gate; requires no Qt installation."""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []

def require(condition, message):
    if not condition:
        errors.append(message)

cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
main = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
window = (ROOT / 'src/MainWindow.cpp').read_text(encoding='utf-8')
workflow = (ROOT / '.github/workflows/windows-source-build.yml').read_text(encoding='utf-8')
readme = (ROOT / 'README.md').read_text(encoding='utf-8')

require(bool(re.search(r'project\(WFScript\s+VERSION\s+0\.2\.0\b', cmake)), 'CMake project version must be 0.2.0')
require('app.setApplicationVersion(QStringLiteral("0.2.0"))' in main, 'QApplication version must be 0.2.0')
require('setWindowTitle(QStringLiteral("WFScript v0.2"))' in window, 'Main window title must be exactly WFScript v0.2')
require(window.count('setWindowTitle(QStringLiteral("WFScript v0.2"))') == 1, 'Exactly one main application title expected')
require('WFScript /' in window or 'QStringLiteral("WFScript")' in window, 'Operator header must keep dynamic Panel/Page identity')
require('-DWFSCRIPT_BUILD_TESTS=ON' in workflow, 'Windows CI must configure native CTests')
require('ctest --test-dir' in workflow, 'Windows CI must execute native CTests')
require('WFScript v0.2' in readme, 'README must identify the new release')
require('Public v0.1 baseline' not in readme, 'README should not present old development status as current')
for name in ('RELEASE_NOTES_v0.2.md', 'docs/RELEASE_V0_2_CHECKLIST.md', 'docs/GITHUB_V0_2_PUBLISHING.md', 'LICENSE', 'LICENSES', 'examples/LEARN_WFSCRIPT.md'):
    require((ROOT/name).exists(), f'Missing release asset: {name}')

catalog = json.loads((ROOT/'examples/catalog.json').read_text(encoding='utf-8'))
require(len(catalog) == 40, f'Expected 40 built-in examples, found {len(catalog)}')
for entry in catalog:
    name = entry['file']
    js = (ROOT/'examples/source'/f'{name}.js').read_text(encoding='utf-8')
    project = json.loads((ROOT/'examples/panels'/f'{name}.wfpanel').read_text(encoding='utf-8'))
    require(project.get('script') == js, f'Embedded script differs from authoring source: {name}')

if errors:
    for error in errors:
        print('FAIL:', error, file=sys.stderr)
    raise SystemExit(1)
print('PASS: v0.2 identity, CI native-test gate, release docs, 40 synchronized examples')
