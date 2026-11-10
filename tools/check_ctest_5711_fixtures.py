#!/usr/bin/env python3
"""Guard corrected 5.7.0.11 native smoke fixtures; Qt execution still required."""
from pathlib import Path


def check(root: Path) -> list[str]:
    errors=[]
    def source(name): return (root/'tests'/name).read_text(encoding='utf-8')
    checks={
        'qt_api_smoke.cpp': ('stop.store(true);', 'stop.store(false);', "'AM recording'"),
        'label_text_format_smoke.cpp': ('CHECK(pages.size() == 7);', 'foundSharedUut', 'checkedControls > 20'),
        'test_api_explorer_560_smoke.cpp': ('guide->childCount()==8', 'guide->child(3)', 'QStringLiteral("libraries")'),
        'panel_script_lifecycle_smoke.cpp': ('edit->click();', 'WFScript Example: Getting Started', 'originalScript'),
        'panel_lifecycle_settings_smoke.cpp': ('PageAppearanceEditor', 'editPanelInitialize', 'editPanelShutdown'),
        'workspace_toolbar_smoke.cpp': ('panelDesignToolsToggle', 'openVisualTestSequenceEditor', 'Focus Canvas'),
        'digital_display_smoke.cpp': ('def.value=71.236;', 'formattedValue()=="71.24"'),
        'panel_lifecycle_hooks_smoke.cpp': ('PANEL SIMULATION: hardware outputs disabled', 'main script then onInitialize', 'initialization exception'),
        'panel_interaction_smoke.cpp': ('INTERACTION STAGE 1', 'INTERACTION STAGE 10', 'Marquee drag selects two widgets'),
        'panel_event_runtime_smoke.cpp': ('Selector diagnostics:', 'Option Selector event updates ui.get before script callback'),
    }
    for filename,tokens in checks.items():
        body=source(filename)
        for token in tokens:
            if token not in body: errors.append(f'{filename}: expected regression guard {token!r}')
    if 'CHECK(pages.size() == 6);' in source('label_text_format_smoke.cpp'):
        errors.append('label formatting test still assumes obsolete six-page example')
    if '"Panel Settings..."' in source('workspace_toolbar_smoke.cpp'):
        errors.append('toolbar smoke still assumes removed Panel Settings button')
    return errors

if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]
    e=check(root)
    for x in e: print('FAIL:',x)
    if e:raise SystemExit(1)
    print('PASS: v5.7.0.11 native fixture regression guards')
