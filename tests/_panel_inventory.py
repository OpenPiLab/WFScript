#!/usr/bin/env python3
"""Inventory of the 40 catalogued examples, excluding explicit QA fixtures.

Do not count every .wfpanel under examples/panels as a shipped example:
ThemeLifecycleRC15149 is an intentionally unlisted, hardware-free regression
fixture. Unknown unlisted files still fail this check instead of being silently
ignored. The application catalog and its JS sources remain authoritative.
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
QA_PANEL_FIXTURES = frozenset({'ThemeLifecycleRC15149.wfpanel'})


def catalogued_example_panels(root=ROOT):
    root = Path(root)
    base = root / 'examples'
    catalog = json.loads((base / 'catalog.json').read_text(encoding='utf-8'))
    names = [entry['file'] for entry in catalog]
    assert len(names) == 40, f'Expected 40 published examples, got {len(names)}'
    assert len(set(names)) == len(names), 'Duplicate entry in example catalog'
    expected = {f'{name}.wfpanel' for name in names}
    published_sources = {f'{name}.js' for name in names}
    actual = {p.name for p in (base / 'panels').glob('*.wfpanel')}
    actual_sources = {p.name for p in (base / 'source').glob('*.js')}
    assert actual_sources == published_sources, (
        f'Catalogue/source mismatch: missing={sorted(published_sources - actual_sources)}, '
        f'unlisted={sorted(actual_sources - published_sources)}')
    assert expected <= actual, f'Missing example Panels: {sorted(expected - actual)}'
    extras = actual - expected
    assert extras == QA_PANEL_FIXTURES, (
        f'Unexpected .wfpanel files: {sorted(extras - QA_PANEL_FIXTURES)}; '
        f'missing QA fixtures: {sorted(QA_PANEL_FIXTURES - extras)}')
    for fixture_name in extras:
        fixture = json.loads((base / 'panels' / fixture_name).read_text(encoding='utf-8'))
        assert fixture.get('format') == 'WFScriptPanel'
        assert fixture.get('version') == 2 and fixture.get('pages')
        assert fixture.get('appearance', {}).get('themeMode') == 'follow'
    return [base / 'panels' / f'{name}.wfpanel' for name in names]


if __name__ == '__main__':
    examples = catalogued_example_panels()
    print(f'PASS: {len(examples)} catalogued Panels, matching source scripts, '
          f'{len(QA_PANEL_FIXTURES)} explicitly registered QA fixture; '
          'no unlisted Panels or missing source files')
