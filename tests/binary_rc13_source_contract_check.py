"""RC1.3 source-only API consistency audit, not a substitute for Qt compilation."""
from pathlib import Path
import re,json
r=Path(__file__).resolve().parents[1]
header=(r/'src/BinaryBridge.h').read_text()
cpp=(r/'src/BinaryBridge.cpp').read_text()
catalog=(r/'src/CompletionCatalog.cpp').read_text()
knowledge=(r/'src/ApiKnowledge.cpp').read_text()
panel=json.loads((r/'development/binary_lab/NativeBinaryPerformanceLab.wfpanel').read_text())
assert len(panel['pages'])==4
assert panel['script']==(r/'development/binary_lab/NativeBinaryPerformanceLab.js').read_text()
classes=[('BinarySamples','BinarySamples'),('BinaryBuffer','BinaryBuffer'),('BinaryStream','BinaryStream'),('BinaryBridge','Binary')]
for classname, exposed in classes:
    pattern=r'class '+classname+r' final : public QObject\s*\{(.*?)\n\};'
    block=re.search(pattern,header,re.S)
    assert block, classname
    functions=re.findall(r'Q_INVOKABLE\s+(?:QJSValue|QObject\*|bool|int|double|QString|void)\s+(\w+)\(',block[1])
    assert functions, classname
    missing=[]
    for function in functions:
        if not re.search(r'\b'+classname+r'::'+function+r'\s*\(',cpp):missing.append(function)
        if not re.search(r'"'+re.escape(function)+r'"',catalog):missing.append('completion '+function)
        if f'{{"{exposed}.{function}"' not in knowledge:missing.append('F1 '+function)
    assert not missing,(classname,missing)
    print('PASS',classname,len(functions),'invokables implemented and documented')
# Every new FileIO API must be in both actual C++ and JS bootstrap.
for fn in ['readBuffer','writeBuffer','appendBuffer']:
    assert f'ScriptUtilityBridge::{fn}' in (r/'src/ScriptUtilityBridge.cpp').read_text()
    assert f'{fn}: function' in (r/'src/PanelRuntime.cpp').read_text()
    assert f'"FileIO.{fn}"' in knowledge
assert 'src/BinaryBridge.h src/BinaryBridge.cpp)' in (r/'CMakeLists.txt').read_text()
assert '"BinaryBuffer"' in (r/'src/ApiExplorer.cpp').read_text()
for fn in ['runCodec','runFileBuffer','runFrames','runCan','runSampleSeries']:
    assert 'function '+fn+'(' in panel['script']
    assert any(control.get('events',{}).get('clicked',{}).get('handler')==fn
        for pg in panel['pages'] for control in pg['controls'])
print('PASS FileIO bridging, CMake dependency, API Explorer and five Panel controls')
