#!/usr/bin/env python3
"""RC lab's real embedded scripts avoid object rest/spread unsupported by QJSEngine.
Native CTest (WFScriptRCStepLabSmoke) is the authoritative Qt parser check.
"""
import json
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
panel=json.loads((root/'examples/panels/RCStepResponseLaboratory.wfpanel').read_text(encoding='utf-8'))
script=(root/'examples/source/RCStepResponseLaboratory.js').read_text(encoding='utf-8')
lib=(root/'examples/libraries/RCStepAnalysis.mjs').read_text(encoding='utf-8')
assert panel['script']==script,'Embedded panel script out of sync'
assert len(panel['libraries'])==1 and panel['libraries'][0]['source']==lib,'Embedded module out of sync'
for name,source in [('Panel script',script),('RCStepAnalysis.mjs',lib)]:
    assert '...' not in source, f'{name}: object-rest/spread syntax may fail in QJSEngine'
assert re.search(r"\bconst\s+summary\s*=\s*\{\s*\}\s*;", script) and re.search(r"\bstate\.records\.push\(summary\)", script)
assert re.search(r"\bfit\.edgeTimeS\s*=\s*edge\.timeS", lib)
assert len(panel['pages'])==11 and panel['pages'][6]['id']=='method63'
print('PASS RC embedded JS/source sync, no object rest/spread syntax, fixed page ID')
