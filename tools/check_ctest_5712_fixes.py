#!/usr/bin/env python3
"""Source-level safeguards for the six remaining v5.7.0.11 CTest failures.

These guards do not replace compilation or execution under Windows Qt.
"""
from pathlib import Path

def check(root: Path) -> list[str]:
    errors=[]
    def has(rel, fragment):
        if fragment not in (root / rel).read_text(encoding='utf-8'):
            errors.append(f'{rel}: missing release regression guard: {fragment!r}')
    def lacks(rel, fragment):
        if fragment in (root / rel).read_text(encoding='utf-8'):
            errors.append(f'{rel}: obsolete or unsafe source fragment: {fragment!r}')

    has('src/ApiDiagnostics.cpp', "result[i]=' ';\n                continue;")
    has('tests/guided_development_smoke.cpp', 'quoted and template literals must not generate bogus diagnostics')
    has('tests/editor_knowledge_smoke.cpp', 'snippet prefix caret at end')
    has('tests/module_editor_popup_smoke.cpp', 'module alias caret must be on math expression')
    lacks('tests/module_editor_popup_smoke.cpp', 'MathTools.mjs\\\");\\nmath\\n')
    has('tests/panel_sizing_smoke.cpp', 'scroll->viewport()->width()+250')
    has('tests/panel_interaction_smoke.cpp', 'Inspector editors restored after Undo/Redo')
    has('tests/panel_interaction_smoke.cpp', 'INTERACTION STAGE 8c: commit ID')
    has('tests/panel_event_runtime_smoke.cpp', 'PanelEventCatalog::registerType(')
    has('tests/panel_event_runtime_smoke.cpp', 'Option Selector event registered in headless fixture')
    return errors

if __name__ == '__main__':
    root=Path(__file__).resolve().parents[1]
    results=check(root)
    for item in results: print('FAIL:', item)
    if results: raise SystemExit(1)
    print('PASS: six native CTest failure regression source guards')
