#!/usr/bin/env python3
"""Offline contract for every bundled mathematical expression and Page.

This checks serialized source, glyph command coverage and geometry. Native
Qt painting and actual screen rendering are tested separately on Windows.
"""
from pathlib import Path
from _panel_inventory import catalogued_example_panels
import collections
import json
import re

ROOT = Path(__file__).resolve().parents[1]
PANEL_DIR = ROOT / 'examples' / 'panels'
SYMBOL_MAP = (ROOT / 'src' / 'widgets' / 'ExperimentWidgets.cpp').read_text(encoding='utf-8')

panels = sorted(catalogued_example_panels(ROOT))
assert len(panels) == 40, len(panels)
page_count = 0
control_count = 0
equation_count = 0
symbols = collections.Counter()
max_bottom = 0
long_pages = []
for file in panels:
    project = json.loads(file.read_text(encoding='utf-8'))
    source = (ROOT / 'examples' / 'source' / f'{file.stem}.js').read_text(encoding='utf-8')
    assert project['script'] == source, f'Embedded JavaScript mismatch: {file.stem}'
    for library in project.get('libraries', []):
        other = ROOT / 'examples' / 'libraries' / library['name']
        if other.exists():
            assert other.read_text(encoding='utf-8') == library['source'], (file, library['name'])
    page_ids = set()
    for page in project['pages']:
        page_count += 1
        assert page['id'] not in page_ids, (file.name, page['id'])
        assert re.fullmatch(r'[A-Za-z_][A-Za-z0-9_-]{0,63}', page['id']), (file, page['id'])
        page_ids.add(page['id'])
        w, h = page['width'], page['height']
        assert 320 <= w <= 8192 and 240 <= h <= 8192
        if h > 660:
            long_pages.append((file.stem, page['id'], w, h))
        for control in page['controls']:
            control_count += 1
            x, y = control['x'], control['y']
            cw, ch = control['width'], control['height']
            assert 0 <= x and 0 <= y and cw >= 25 and ch >= 22
            assert x + cw <= w and y + ch <= h, (file.stem, page['id'], control['id'])
            max_bottom = max(max_bottom, y + ch)
            if control['type'] != 'equation':
                continue
            equation_count += 1
            equation = control['properties'].get('source', '')
            for cmd in re.findall(r'\\([A-Za-z]+|.)', equation):
                if cmd in ('frac', 'dfrac', 'tfrac', 'sqrt', 'mathrm', 'text', 'operatorname'):
                    continue
                symbols[cmd] += 1
                assert re.search(r'\{"' + re.escape(cmd) + r'",', SYMBOL_MAP), (
                    'Equation command not implemented', cmd, file.stem, page['id'])

assert page_count == 92, page_count
assert control_count > 900, control_count
assert equation_count == 19, equation_count
assert 'Sigma' in symbols and 'varepsilon' in symbols and 'Longleftrightarrow' in symbols
assert len(long_pages) > 60
print(f'PASS {len(panels)} Panels, {page_count} Pages, {control_count} controls, '
      f'{equation_count} equations; all {len(symbols)} equation commands supported, '
      f'geometry in bounds, 40 JS sources identical to embedded copies')
print(f'INFO {len(long_pages)} saved Pages exceed 660 px; non-destructive Run Fit Page '
      f'presents them without scrolling, Design retains full-resolution coordinates')
