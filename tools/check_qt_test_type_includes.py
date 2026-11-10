#!/usr/bin/env python3
"""Flag native test files that use Qt value types without direct Qt includes.

A declaration visible through another header may be incomplete or may disappear
when Qt updates its own include graph. This intentionally enforces direct includes
for QJsonArray/QJsonObject/QTextBlock in our native test translation units.
No Qt install is necessary to run this check.
"""
from __future__ import annotations
from pathlib import Path
import re
import sys

DEPENDENCIES = {
    'QJsonArray': (r'\bQJsonArray\b', r'\.toArray\s*\('),
    'QJsonObject': (r'\bQJsonObject\b', r'\.toObject\s*\('),
    'QTextBlock': (r'\bQTextBlock\b', r'\.block\s*\('),
}


def check(test_dir: Path) -> list[str]:
    errors = []
    for file in sorted(test_dir.glob('*.cpp')):
        source = file.read_text(encoding='utf-8')
        for header, uses in DEPENDENCIES.items():
            if not any(re.search(pattern, source) for pattern in uses):
                continue
            if not re.search(r'^\s*#\s*include\s*<'+header+r'>', source, re.MULTILINE):
                errors.append(f'{file.name}: uses {header} (or a method returning it) without #include <{header}>')
    return errors


if __name__ == '__main__':
    problems = check(Path(__file__).resolve().parents[1] / 'tests')
    if problems:
        for problem in problems:
            print('FAIL:', problem, file=sys.stderr)
        sys.exit(1)
    print('PASS: native Qt JSON/QTextBlock direct-include audit')
