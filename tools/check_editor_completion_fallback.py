#!/usr/bin/env python3
"""Guard the editor-key acceptance fallback against accidental removal.

The actual behavior is covered by native Qt6 CompletionPopup/SmartEditing
smoke tests. This source preflight cannot substitute for running those tests.
"""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'src/ScriptEditor.cpp').read_text(encoding='utf-8')
errors=[]
for token in ('case Qt::Key_Return:', 'case Qt::Key_Tab:',
              'm_completer->popup()->currentIndex()',
              'insertCompletion(completion);', 'event->accept();'):
    if token not in source:errors.append('Missing completion-key fallback token: '+token)
if 'case Qt::Key_Enter: case Qt::Key_Return: case Qt::Key_Escape:' in source:
    errors.append('Old ignore-only popup acceptance restored')
for rel,markers in {
    'tests/completion_popup_smoke.cpp': ['Return did not insert selected',
                                         "Tab did not insert selected"],
    'tests/smart_editing_smoke.cpp': ['accept enum without duplicating quote'],
}.items():
    t=(ROOT/rel).read_text(encoding='utf-8')
    for marker in markers:
        if marker not in t: errors.append('Missing original regression assertion: '+rel+' '+marker)
def check():
    return list(errors)

if __name__ == '__main__':
    for e in errors:print('FAIL:',e,file=sys.stderr)
    if errors:sys.exit(1)
    print('PASS: editor completion fallback and original native acceptance assertions present')
