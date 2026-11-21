#!/usr/bin/env python3
"""Protect native Qt CTest use of the actual PanelEventDefinition metadata API.

This is a source contract, not a substitute for native Qt compilation.
"""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
header=(ROOT/'src/PanelEventCatalog.h').read_text(encoding='utf-8')
body=re.search(r'struct\s+PanelEventDefinition\s*\{([^}]*)\}',header,re.S)
assert body, 'PanelEventDefinition aggregate not found'
fields=set(re.findall(r'\bQString\s+(\w+)\s*;',body.group(1)))
assert {'name','signature','description','argument'} <= fields,fields
assert 'id' not in fields,'The event-name API must not be changed to mask a bad test'
source=(ROOT/'tests/communications_terminal_smoke.cpp').read_text(encoding='utf-8')
assert 'factory.events().first().name==QStringLiteral("sendRequested")' in source
assert 'factory.events().size()==1' in source
factory=(ROOT/'src/widgets/CommunicationsTerminalWidgetFactory.h').read_text(encoding='utf-8')
assert 'QStringLiteral("sendRequested")' in factory
# Scan all native CTests: avoid referring to the obsolete `.id` member
# on the QVector<PanelEventDefinition> returned by widget factories.
cpps=list((ROOT/'tests').rglob('*.cpp'))
for cpp in cpps:
    txt=cpp.read_text(encoding='utf-8')
    assert not re.search(r'\bevents\s*\(\s*\)\s*\.\s*(?:first|last)\s*\(\s*\)\s*\.\s*id\b',txt),cpp
print(f'PASS RC1.5.14.10.5: PanelEventDefinition.name, communications sendRequested, and {len(cpps)} native test event-member audits')
