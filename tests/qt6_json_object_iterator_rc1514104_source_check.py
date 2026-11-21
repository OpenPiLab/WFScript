#!/usr/bin/env python3
"""Prevent unsupported QJsonObject cbegin/cend calls in production or native tests.

Qt 6.9.2 MinGW lacks QJsonObject::cbegin()/cend().  QMap/QSet/QVector
uses of those methods are intentionally NOT prohibited by this test.
"""
from pathlib import Path
import re, json
root=Path(__file__).resolve().parents[1]
cpp=(root/'tests/panel_event_binding_smoke.cpp').read_text(encoding='utf-8')
assert 'const QJsonObject bindings=control.value(QStringLiteral("events")).toObject();' in cpp
assert 'for(auto it=bindings.begin();it!=bindings.end();++it)' in cpp
assert 'bindings.cbegin()' not in cpp and 'bindings.cend()' not in cpp
assert 'require(validatedBindings==229' in cpp
# Scan every C++ file for a named QJsonObject local/argument with unsupported
# iterator calls; do not flag other Qt container types that support cbegin().
checked=0
for subtree in ('src','tests'):
    for file in (root/subtree).rglob('*'):
        if file.suffix not in {'.cpp','.h','.hpp'}:
            continue
        content=file.read_text(encoding='utf-8')
        checked+=1
        objects=set(re.findall(r'\bQJsonObject\s*(?:[&*]\s*)?(\w+)\s*(?=[=;({])',content))
        for name in objects:
            assert not re.search(r'\b'+re.escape(name)+r'\s*\.\s*c(?:begin|end)\s*\(',content), f'{file}: QJsonObject {name} uses unavailable iterator API'
# Production data contract: the Panel event bindings still exist; the test
# must continue checking actual saved visual bindings when compiled natively.
examples=root/'examples/panels'
count=sum(len(c.get('events',{})) for p in examples.glob('*.wfpanel') for page in json.loads(p.read_text(encoding='utf-8')).get('pages',[]) for c in page.get('controls',[]))
assert count>=229, f'Unexpected loss of visual event bindings: {count}'
print(f'PASS RC1.5.14.10.4: Qt 6.9-compatible JSON iterators across {checked} C++ files; saved event binding inventory intact')
