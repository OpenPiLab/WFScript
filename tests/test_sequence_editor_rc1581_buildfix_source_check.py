"""Prevent duplicate named layout locals in the RC1.5.8.1 editor constructor.

This catches the MinGW-reported redeclaration of codeLayout that prevented
RC1.5.8 from compiling. Native Qt compilation is still the release gate.
"""
from pathlib import Path
import re
from collections import Counter
source=(Path(__file__).resolve().parents[1]/'src/TestSequenceEditor.cpp').read_text()
start=source.index('TestSequenceEditor::TestSequenceEditor(')
end=source.index('void TestSequenceEditor::reject()', start)
constructor=source[start:end]
layout_declarations=re.findall(r'\bauto\s*\*\s*([A-Za-z_]\w*)\s*=\s*new\s+Q(?:HBoxLayout|VBoxLayout|FormLayout|GridLayout)\b',constructor)
repeated={name:count for name,count in Counter(layout_declarations).items() if count>1}
assert not repeated, f'Duplicate layout pointer declarations: {repeated}'
assert 'auto* libraryLayout=new QVBoxLayout(codeTab);' in constructor
assert 'libraryLayout->addWidget(codeInfo);' in constructor
assert 'libraryLayout->addWidget(m_sourceEditor,1);' in constructor
assert 'auto handlerRow=[execution]' in constructor and 'new QHBoxLayout(row)' in constructor
assert 'WFScript RC1.5.14' in constructor
print('PASS RC1.5.8.1: unique constructor layout locals and correct library tab layout bindings')
