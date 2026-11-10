#!/usr/bin/env python3
"""Check native test files for accidental multiline ordinary C++ string literals.

No Qt dependency. C++ raw strings (R"DELIM(... )DELIM") are supported.
This intentionally checks lexical strings, not full C++ syntax or compilation.
"""
from __future__ import annotations
from pathlib import Path
import sys


def find_bad_literals(source: str) -> list[int]:
    bad = []
    i = 0
    n = len(source)
    line = 1
    state = "code"
    opened = 0
    while i < n:
        c = source[i]
        nxt = source[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                state = "line_comment"
                i += 2
                continue
            if c == "/" and nxt == "*":
                state = "block_comment"
                i += 2
                continue
            if c == '"':
                if i > 0 and source[i - 1] == "R" and (i < 2 or not (source[i - 2].isalnum() or source[i - 2] == "_")):
                    j = source.find("(", i + 1)
                    if j != -1 and "\n" not in source[i + 1:j] and j - i <= 18:
                        delimiter = source[i + 1:j]
                        terminator = ")" + delimiter + '"'
                        end = source.find(terminator, j + 1)
                        if end != -1:
                            end += len(terminator)
                            line += source[i:end].count("\n")
                            i = end
                            continue
                state = "string"
                opened = line
            elif c == "'":
                state = "char"
                opened = line
        elif state == "line_comment":
            if c == "\n":
                state = "code"
        elif state == "block_comment":
            if c == "*" and nxt == "/":
                i += 2
                state = "code"
                continue
        else:  # a normal C++ quoted string or character literal
            if c == "\\" and nxt:
                if nxt == "\n":
                    line += 1
                i += 2
                continue
            if (state == "string" and c == '"') or (state == "char" and c == "'"):
                state = "code"
            elif c == "\n":
                bad.append(opened)
                state = "code"
        if c == "\n":
            line += 1
        i += 1
    return bad


def check(test_dir: Path) -> list[str]:
    errors = []
    for f in sorted(test_dir.rglob("*")):
        if not f.is_file() or f.suffix not in {".cpp", ".h", ".hpp"}:
            continue
        for line in find_bad_literals(f.read_text(encoding="utf-8")):
            errors.append(f"{f.name}:{line}: unescaped newline in ordinary C++ string/char literal")
    return errors


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    errors = check(root / "tests")
    if errors:
        print("\n".join(errors))
        sys.exit(1)
    print("PASS: native C++ test string literal scan")
