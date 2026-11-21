#!/usr/bin/env python3
"""Final four Windows-native CTest failure-site contracts (source-only)."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def s(p):return (root/p).read_text(encoding="utf-8")
runner=s('tests/test_runner_cpp_smoke.cpp')
assert 'accumulatedMeasurements.append(added.toList())' in runner
assert 'accumulatedMeasurements.push_back(added)' in runner
assert 'accumulatedMeasurements.size()==2' in runner
assert 'output.value(QStringLiteral("table")).toList().size()==2' not in runner
sql=s('src/SqliteBridge.cpp')
assert 'item.isNull()?QVariant(QMetaType::fromType<QString>())' in sql
assert 'cell=QVariant::fromValue(nullptr);' in sql  # preserve JS null on read
assert 'item.isNull()?QVariant(QByteArray())' not in sql
chart=s('tests/digital_chart_performance_smoke.cpp')
assert chart.count('assert(d.resolvedRanges()==currentView);')>=4
assert 'assert(d.resolvedRanges()==before);' not in chart
assert 'assert(d.digitalRenderDiagnostics().value("rasterBuilds").toLongLong()>beforeStyle);' in chart
cond=s('tests/test_conditional_rc1514_smoke.cpp')
assert 'assert(!runner.run());finishRun(app,runner);' in cond
assert 'assert(runner.state()==QStringLiteral("error"));' in cond
assert 'visited[visited.length-1]' in cond
assert 'assert(runner.run());finishRun(app,runner);' in cond
print('PASS RC1.5.14.10.7 four native failure-site source contracts')
