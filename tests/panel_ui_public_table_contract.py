#!/usr/bin/env python3
"Catch omissions between native PanelUiBridge and production ui JS object."
from pathlib import Path
import re, subprocess
root=Path(__file__).resolve().parents[1]
cpp=(root/'src/PanelRuntime.cpp').read_text(encoding='utf-8')
bridge=(root/'src/PanelUiBridge.h').read_text(encoding='utf-8')
for name in ('tableRowCount','tableSlice'):
    assert re.search(r'Q_INVOKABLE\s+(?:int|QJSValue)\s+'+name+r'\s*\(',bridge)
    assert re.search(r'\b'+name+r'\s*:\s*function\s*\(',cpp),f'Public ui.{name} missing from PanelRuntime bootstrap'
    assert f'__panelHost.{name}(' in cpp
# Get exactly the production ui object. Node evaluates it as-is, not a recreated mock API.
a=cpp.index('        var ui = {')
b=cpp.index('        };',a)+len('        };')
ui=cpp[a:b]
stub='''var __panelHandlers={}; var __panelHost={
  get:id=>[id], set:(id,v)=>true, boundDataForWidget:id=>'',
  tableRowCount:id=>50000,
  tableSlice:(id,start,count)=>[[id,start,count]]
};'''
checks='''
if(typeof ui.tableRowCount!=='function'||ui.tableRowCount('table')!==50000)throw Error('row count missing');
if(JSON.stringify(ui.tableSlice('table',25,2))!==JSON.stringify([['table',25,2]]))throw Error('slice missing');
if(!ui.set('table',{append:[1,2,3]}))throw Error('existing set broken');
if(ui.get('table')[0]!=='table')throw Error('existing get broken');
console.log('PASS: production public ui table API + legacy get/set');'''
script=stub+'\n'+ui+'\n'+checks
r=subprocess.run(['node','-e',script],capture_output=True,text=True)
if r.returncode: raise AssertionError(r.stderr)
print(r.stdout.strip())
