# Qt open-source licensing notes

WFScript v0.1 is packaged as an **open-source source release** under the repository's root **LGPL v3** license.

## Important distinction

Two licensing layers are relevant:

1. **WFScript itself** — this repository's own source code license.
2. **Qt** — the framework used to build WFScript.

Qt is distributed under commercial and open-source terms. According to Qt's official licensing documentation, Qt under the open-source model is appropriate when you can comply with the **GNU LGPL v3** (or, for some modules, GPL) obligations.

## Practical takeaway for this repository

- Publishing WFScript source code under **LGPL v3** is compatible with the intended open-source release.
- If you distribute **binaries** built with Qt's open-source edition, you must still comply with Qt's own license obligations.
- The majority of standard Qt modules are available under LGPL v3, but some Qt modules are GPL-only under open-source terms. Review the modules you use before extending the project.

WFScript currently uses Qt6 **Widgets** and **Qml** as declared in `CMakeLists.txt`.

## When distributing binaries

Before distributing binaries, review the official Qt guidance and make sure you are comfortable with the open-source obligations, including items such as:

- providing the required license notices,
- providing access to the corresponding Qt source code as required,
- allowing end users the freedoms required by LGPL,
- avoiding distribution arrangements that conflict with LGPL terms.

## No legal advice

These notes are a practical summary for the project and are **not legal advice**. If you need certainty for a commercial or public binary distribution, review the official Qt licensing documentation and obtain legal advice if needed.
