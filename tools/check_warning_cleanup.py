"""Source-level guards for the pre-public v0.1 GCC/Qt warning cleanup.

These checks do not replace compiling with Qt 6.9.2/MinGW, but they catch
reintroduction of the exact high-volume warning patterns in the owner's log.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


factory = read("src/widgets/WidgetFactory.h")
assert "QStringList choices{};" in factory
assert "QVariant defaultValue{};" in factory
assert "QString category{};" in factory
assert "struct WidgetProperty" in factory

highlight = read("src/JavaScriptHighlighter.cpp")
assert "QRegularExpression::AnchorAtOffsetMatchOption" in highlight
assert "QRegularExpression::AnchoredMatchOption" not in highlight

editor = read("src/ScriptEditor.cpp")
assert "static_cast<int>(plain.size())" in editor
assert "static_cast<int>(m.captured(2).size())" in editor
assert "offsets.append({m.captured(1).toInt(),plain.size()" not in editor

runtime = read("src/DwfRuntime.cpp")
assert "const qint64 minCount = static_cast<qint64>" in runtime
assert "const qint64 maxCount = static_cast<qint64>" in runtime
assert "h >= minCount && l >= minCount" in runtime
assert "h>=qMax<quint64>(1u,cmin)" not in runtime

simulation = read("src/WaveFormsBindings.cpp")
assert "const double periods=std::floor(rate*t)" not in simulation

registry = read("src/widgets/BuiltinWidgets.cpp")
assert 'for(const char* type : {"button"' in registry
assert 'make_unique<BuiltinWidgetFactory>(QString::fromLatin1(type))' in registry

mouse_test = read("tests/chart_cursor_snap_smoke.cpp")
assert "QMouseEvent event(type,at,at,at,button,buttons,mods);" in mouse_test

csv = read("src/widgets/ExperimentWidgets.cpp")
assert "if(error)*error=file.errorString();return false;" not in csv

cmake = read("CMakeLists.txt")
assert "-Wno-missing-field-initializers" not in cmake
assert "-Wno-misleading-indentation" not in cmake
assert "-Wno-narrowing" not in cmake

print("PASS: source-level warning-cleanup regression checks (not a Qt native build)")
