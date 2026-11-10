#!/usr/bin/env python3
"""Static preflight for TestRunner JS values and Results History.

This complements native Qt tests; it does not execute Qt/C++ code.
"""
from pathlib import Path
import re


def check(root: Path) -> list[str]:
    root = Path(root)
    errors: list[str] = []
    hdr = (root / 'src/TestRunnerBridge.h').read_text(encoding='utf-8')
    impl = (root / 'src/TestRunnerBridge.cpp').read_text(encoding='utf-8')
    history = (root / 'src/TestResultsHistory.cpp').read_text(encoding='utf-8')
    js_smoke = (root / 'tests/panel_javascript_values_smoke.cpp').read_text(encoding='utf-8')
    history_smoke = (root / 'tests/test_results_history_567_smoke.cpp').read_text(encoding='utf-8')

    for method, native in [('report', 'QVariantMap'), ('results', 'QVariantList')]:
        if not re.search(rf'Q_INVOKABLE\s+QJSValue\s+{method}\(\)\s+const\s*;', hdr):
            errors.append(f'TestRunner.{method} must expose engine-native JS values')
        if not re.search(rf'{native}\s+{method}Data\(\)\s+const\s*;', hdr):
            errors.append(f'TestRunner.{method}Data must retain typed C++ data')
        if f'toJavaScript(m_engine,{method}Data())' not in impl:
            errors.append(f'TestRunner.{method} must recursively convert typed data')

    if 'TestResultsRepository::archive(reportData(),' not in impl:
        errors.append('archive must continue serializing the native QVariant report')
    if 'm_engine->globalObject()' not in impl or 'globalThis' not in impl:
        errors.append('TestRunner must expose globalThis on engines missing it')
    if 'entries.size()>maximum' not in history or 'entries=entries.mid(0,maximum)' not in history:
        errors.append('Results History must limit valid entries after sorting')
    if 'inspected<inspectionBudget' not in history:
        errors.append('Results History scan needs a bounded filesystem inspection')
    if not all(x in js_smoke for x in ['Array.isArray(results)', 'Array.isArray(report.steps)',
                                      'Array.isArray(report.measurements)', "result.push({name:'unexpected'})"]):
        errors.append('TestRunner JavaScript regression contract has been weakened')
    if 'invalid or unrelated JSON cannot displace newest completed report' not in history_smoke:
        errors.append('Results History newest-valid-record regression is missing')
    for test in (root / 'tests').glob('*.cpp'):
        text = test.read_text(encoding='utf-8')
        for match in re.finditer(r'\brunner\.(report|results)\(\)', text):
            errors.append(f'{test.name}: native C++ must use {match.group(1)}Data()')
    return errors


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    issues = check(root)
    for issue in issues:
        print('FAIL:', issue)
    print('PASS: TestRunner native JS arrays and bounded Results History contracts' if not issues else 'FAIL: contracts')
    raise SystemExit(1 if issues else 0)
