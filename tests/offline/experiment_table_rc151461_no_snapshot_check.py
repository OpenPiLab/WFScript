#!/usr/bin/env python3
"""RC1.5.14.6.1: verify full snapshots are opt-in and import sync is bounded."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]
widget=(root/'src/widgets/ExperimentWidgets.cpp').read_text()
runtime=(root/'src/PanelRuntime.cpp').read_text()
header=(root/'src/widgets/ExperimentWidgets.h').read_text()
assert 'm_emitFullChange=false' in header
assert 'emitFullChangeEvent")).toBool(false)' in widget
assert 'legacy.defaultValue=false' in widget
assert widget.count('m_sink(QStringLiteral("valueChanged"),rows())') == 3
assert widget.count('if(m_emitFullChange)m_sink(QStringLiteral("valueChanged"),rows());') == 3
assert 'constexpr int MirrorChunkRows=1024' in widget
assert 'm_sink(QStringLiteral("__experimentTableMirrorReset")' in widget
assert 'm_sink(QStringLiteral("__experimentTableMirrorAppend")' in widget
assert 'name==QStringLiteral("__experimentTableMirrorReset")' in runtime
assert 'name==QStringLiteral("__experimentTableMirrorAppend")' in runtime
# Private events must terminate before JS dispatch.
position=runtime.index('name==QStringLiteral("__experimentTableMirrorReset")')
assert position<runtime.index('PanelEventCatalog::carriesPersistentState(name)')
# Older bundled examples that rely on the event explicitly opt in.
for stem in ('EquationExperimentLab','CalibrationTable','TableDrivenWavegen','TableLimitPlot'):
    doc=json.loads((root/'examples/panels'/f'{stem}.wfpanel').read_text())
    tables=[c for page in doc['pages'] for c in page['controls']
            if c.get('type')=='experiment_table' and 'valueChanged' in c.get('events',{})]
    assert tables and all(c['properties']['emitFullChangeEvent'] is True for c in tables),stem
# The 50k streaming example must remain on the efficient default.
stream=json.loads((root/'tests/fixtures/ExperimentDataTable_NoFullSnapshot_RC151461.wfpanel').read_text())
table=next(c for c in stream['controls'] if c['type']=='experiment_table')
assert 'emitFullChangeEvent' not in table['properties']  # Exercise the new native default
print('PASS: default cell edits without snapshots, private bounded import/clear sync, explicit legacy examples')
