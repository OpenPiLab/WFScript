#!/usr/bin/env python3
"""Lock all 55 first-party student-facing JavaScript sources after code formatting.

The authoring-time TypeScript parser verifies identical significant JS tokens
and syntax tree shape against the original before producing the lock. This
runtime-independent test checks the exact reviewed output and teaching comments.
"""
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'docs/EXAMPLE_JAVASCRIPT_FORMAT_LOCK.json').read_text(encoding='utf8'))
assert manifest['baseline']=='v0.2-dev.8.9.3.2'
for group, folder in [('scripts','source'),('modules','libraries'),('waveformsCompatible','waveforms_compatible')]:
    hashes=manifest[group]
    files=sorted((root/'examples'/folder).glob('*.mjs' if group=='modules' else '*.js'))
    assert len(files)==len(hashes),(group,len(files),len(hashes))
    for path in files:
        assert path.name in hashes,path
        contents=path.read_bytes()
        assert hashlib.sha256(contents).hexdigest()==hashes[path.name]['sha256'],f'JavaScript changed without review: {path}'
        assert len(hashes[path.name]['tokenSHA256'])==64,path
        # Every formatted script has a trailing newline and no tab indentation.
        assert contents.endswith(b'\n') and b'\t' not in contents,path
assert len(manifest['scripts'])==40 and len(manifest['modules'])==11
assert (root/'docs/JAVASCRIPT_EDUCATIONAL_STYLE.md').is_file()
for p in ('examples/source/ACNetworkLaboratory.js','examples/source/RCStepResponseLaboratory.js',
          'examples/libraries/CaptureAnalysis.mjs','examples/libraries/RCStepAnalysis.mjs'):
    s=(root/p).read_text(encoding='utf8')
    assert 'READING MAP' in s or 'LEAST SQUARES' in s or 'THE LINEAR FIT' in s, p
print('PASS 55 source-format locks, 40 Panel JS files, 11 analysis/support modules, 4 WaveForms-compatible scripts, learning references')
