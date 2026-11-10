#!/usr/bin/env python3
"""Build/verify the self-contained Qt resource .wfpanel files from readable .js.

Usage:
    python tools/build_examples.py          # refresh source-embedded JavaScript
    python tools/build_examples.py --check  # fail if source and .wfpanel differ

Panel layout and event metadata stay in the pretty-printed .wfpanel JSON;
examples/source/*.js is the authority for script text. Legacy user files are
not processed or changed by this tool.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
EX=ROOT/'examples'


def run(check: bool) -> int:
    catalog=json.loads((EX/'catalog.json').read_text(encoding='utf-8'))
    if not isinstance(catalog,list) or not catalog:
        raise ValueError('Examples catalog is empty')
    names=set()
    failures=[]
    for entry in catalog:
        stem=entry['file']
        if not re.fullmatch(r'[A-Za-z][A-Za-z0-9]*',stem) or stem in names:
            raise ValueError(f'Invalid or duplicate example ID: {stem}')
        names.add(stem)
        jsfile=EX/'source'/f'{stem}.js'
        panelpath=EX/'panels'/f'{stem}.wfpanel'
        text=jsfile.read_text(encoding='utf-8')
        project=json.loads(panelpath.read_text(encoding='utf-8'))
        if project.get('format')!='WFScriptPanel' or not project.get('pages'):
            raise ValueError(f'Invalid Panel project: {stem}')
        if project.get('scriptName')!=stem+'.js':
            raise ValueError(f'Unexpected scriptName: {stem}')
        if project['script']!=text:
            if check:
                failures.append(stem)
            else:
                project['script']=text
                panelpath.write_text(json.dumps(project,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        # Embedded test-library source is authoritative alongside the main
        # .js example; keep the module portable inside its .wfpanel archive.
        if stem == 'AutomatedTestStation':
            libnames=('TestSteps.mjs',)
            expected=[{'name':name,'source':(EX/'libraries'/name).read_text(encoding='utf-8')} for name in libnames]
            if project.get('libraries')!=expected:
                if check:
                    failures.append(stem+' library')
                else:
                    project['libraries']=expected
                    panelpath.write_text(json.dumps(project,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(f'{stem}: {len(project["pages"])} Page(s), {len(text.splitlines())} JS lines')
    if failures:
        print('Out of sync (run python tools/build_examples.py): '+', '.join(failures),file=sys.stderr)
        return 1
    print(f'PASS: {len(catalog)} built-in examples in sync')
    return 0

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    raise SystemExit(run(args.check))
