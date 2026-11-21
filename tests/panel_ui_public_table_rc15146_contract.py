#!/usr/bin/env python3
from pathlib import Path
import re, subprocess
root=Path(__file__).resolve().parents[1]
cpp=(root/'src/PanelRuntime.cpp').read_text(encoding='utf-8')
bridge=(root/'src/PanelUiBridge.h').read_text(encoding='utf-8')
for name in ('tableRowCount','tableSlice','tableSelection'):
    assert re.search(r'Q_INVOKABLE\s+\w+\s+'+name+r'\s*\(',bridge),name
    assert re.search(r'\b'+name+r'\s*:\s*function\s*\(',cpp),name
for name in ('tableRow','tableSetTitle','tableSetStatus','tableFollow','tableImportCsv','tableExportCsv','tableCopySelection'):
    assert re.search(r'\b'+name+r'\s*:\s*function\s*\(',cpp),name
start=cpp.index('        var ui = {');end=cpp.index('        };',start)+len('        };')
ui=cpp[start:end]
stub="""var __panelHandlers={};var __panelHost={
 get:id=>null,set:(id,v)=>true,boundDataForWidget:id=>'',
 tableRowCount:id=>50000,tableSlice:(id,start,count)=>[[id,start,count]],
 tableSelection:id=>({row:2,column:1,rows:[2]})
};"""
checks="""
if(ui.tableRowCount('t')!==50000)throw Error('missing count');
if(ui.tableRow('t',3)[1]!==3)throw Error('missing row');
if(ui.tableSelection('t').row!==2)throw Error('missing selection');
for(const fn of ['tableSetTitle','tableSetStatus','tableFollow','tableImportCsv','tableExportCsv','tableCopySelection'])
 if(typeof ui[fn]!=='function')throw Error(fn);
if(!ui.tableSetStatus('t','LIVE')||!ui.tableImportCsv('t')||!ui.tableExportCsv('t'))throw Error('bad command');
console.log('PASS RC1.5.14.6 production JS wrapper');"""
result=subprocess.run(['node','-e',stub+'\n'+ui+'\n'+checks],text=True,capture_output=True)
assert result.returncode==0,result.stderr
print(result.stdout.strip())
