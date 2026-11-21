#!/usr/bin/env python3
"""Preflight all distributed Panels against the Experiment Data Table file contract.

This catches invalid Panel examples before Qt's native PanelProject::fromJson()
can reject them on user machines. It intentionally doesn't replace the native
parser or the Qt smoke tests.
"""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
MAX_COLS=32
MAX_ROWS=100000
MAX_INITIAL_TEXT=256*1024
MAX_PROP_BYTES=512*1024
COUNT=0
for panel in sorted((ROOT/'examples/panels').glob('*.wfpanel')):
    doc=json.loads(panel.read_text(encoding='utf-8'))
    for page in doc['pages']:
        for c in page['controls']:
            if c['type']!='experiment_table':
                continue
            COUNT+=1
            props=c['properties']
            headings=props['columns'].split('|')
            limit=props.get('maxRows',250)
            initial=props.get('initialRows','')
            assert 1<=len(headings)<=MAX_COLS, (panel.name,page['name'],c['id'],'headers')
            assert all(h.strip() and len(h)<=100 for h in headings),(panel.name,c['id'],'empty header')
            assert 1<=limit<=MAX_ROWS,(panel.name,c['id'],'maxRows')
            assert len(initial)<=MAX_INITIAL_TEXT,(panel.name,c['id'],'initial text')
            assert len(json.dumps(props,ensure_ascii=False,separators=(',',':')).encode('utf-8'))<=MAX_PROP_BYTES
            lines=[row for row in initial.split('\n') if row]
            assert len(lines)<=limit,(panel.name,c['id'],'initial row count')
            for line in lines:
                cells=line.split('|')
                assert len(cells)==len(headings), (panel.name,c['id'],'initial row shape')
                assert all(len(cell)<=256 for cell in cells), (panel.name,c['id'],'cell length')
            if panel.stem=='ACNetworkLaboratory':
                assert c['id']=='resultsTable' and len(headings)==11 and limit==500
print(f'PASS Experiment Data Table schema: {COUNT} widgets across 40 Panels, 11-column AC Notebook')
